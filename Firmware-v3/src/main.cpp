#include <stm32h7rsxx.h>
#include <cstring>
#include "pins.h"
#include "i2c.h"
#include "memblk.h"
#include "scheduler.h"
#include "process.h"
#include "sd.h"
#include "ext4_thread.h"
#include "screen.h"
#include "gpu.h"
#include "buttons.h"
#include "ctp.h"
#include "buddy.h"
#include "profile.h"
#include "SEGGER_RTT.h"

uint32_t test_val;

uint32_t test_range[256];

static const constexpr pin CTP_NRESET { GPIOF, 3 };
void system_init_cm7();

Scheduler sched;
Process kernel_proc;

SRAM4_DATA std::vector<std::string> empty_string_vector;
SRAM4_DATA MemRegion memblk_persistent_log;

extern pid_t pid_gkmenu;

extern uint32_t _tls_pointers[2];
extern uint64_t _cur_ms;
extern struct timespec toffset;

// default environment variables
SRAM4_DATA std::vector<std::string> gk_env;


void *idle_thread(void *p);
void *idle_thread_dbg(void *p);
void *init_thread(void *p);

int main()
{
    /* Check for debugger */
    if(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        SEGGER_RTT_printf(0, "kernel: startup, debugger detected\n");
        DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;
    }

    /* Dump XSPI calibration */
    SEGGER_RTT_printf(0, "kernel: xspi1: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI1->CALFCR, XSPI1->CALSOR, XSPI1->CALSIR);
    SEGGER_RTT_printf(0, "kernel: xspi2: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI2->CALFCR, XSPI2->CALSOR, XSPI2->CALSIR);

    /* Memory test prior to enabling caches */
#define GK_MEMTEST 0
#if GK_MEMTEST
    for(uint32_t addr = 0x98000000U - 4U; addr >= 0x90000000U; addr -= 4)
    {
        *(volatile uint32_t *)addr = addr;
    }
    uint64_t seed = 123456789U;
    for(uint32_t addr = 0x98000000U - 4U; addr >= 0x90000000U; addr -= 4)
    {
        auto v = *(volatile uint32_t *)addr;
        if(v != addr)
        {
            SEGGER_RTT_printf(0, "memchk: fail at %x - got %x\n", addr, v);
            // try again
            for(int i = 0; i < 10; i++)
            {
                // do some random dummy reads to flush prefetch buffer (not sure how big it is)
                for(int j = 0; j < 1024; j++)
                {
                    seed = (1103515245ULL * seed + 12345ULL) % 0x80000000ULL;
                    auto new_addr = 0x90000000U + (uint32_t)(seed % 0x08000000ULL);
                    *(volatile uint32_t *)new_addr;
                }
                auto v2 = *(volatile uint32_t *)addr;
                SEGGER_RTT_printf(0, "memchk:   retry %d, got %x (%s)\n", i, v2,
                    (v2 == addr) ? "SUCCESS" : "FAIL");
            }
        }
    }

    // recheck calibration after test
    (void)*(volatile uint32_t *)0x94000000U;
    __DMB();
    __DSB();
    XSPI1->CR |= XSPI_CR_ABORT;
    while((XSPI1->CR & XSPI_CR_ABORT) || (XSPI1->SR & XSPI_SR_BUSY));

    // trigger calibration
    XSPI1->DCR2 = XSPI1->DCR2;

    // dummy read
    (void)*(volatile uint32_t *)0x92000000U;
    SEGGER_RTT_printf(0, "kernel: xspi1: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI1->CALFCR, XSPI1->CALSOR, XSPI1->CALSIR);
    SEGGER_RTT_printf(0, "kernel: xspi2: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI2->CALFCR, XSPI2->CALSOR, XSPI2->CALSIR);
#endif

    BKPT();

    system_init_cm7();
    memcpy(kernel_proc.p_mpu, mpu_default, sizeof(mpu_default));
    kernel_proc.name = "kernel";

    init_memblk();

    extern BuddyAllocator<256, 0x80000, 0x24000000> b_axisram;
    extern BuddyAllocator<256, 0x40000, 0x20000000> b_dtcm;
    extern BuddyAllocator<256, 0x8000, 0x30000000> b_sram;
    extern BuddyAllocator<1024, 0x40000, 0> b_itcm;

    b_itcm.dump(klog);
    b_dtcm.dump(klog);
    b_axisram.dump(klog);
    b_sram.dump(klog);    

    // Check _tls_pointers[] is valid
    klog("kernel: _tls_pointers[] at %x\n", (uint32_t)(uintptr_t)_tls_pointers);
    if((uint32_t)(uintptr_t)_tls_pointers != GK_TLS_POINTER_ADDRESS)
    {
        klog("kernel: _tls_pointers[] is invalid\n");
        __BKPT();
        while(true);
    }
    if((uint32_t)(uintptr_t)&_cur_ms != GK_CUR_MS_ADDRESS)
    {
        klog("kernel: _cur_ms is invalid\n");
        BKPT();
        while(true);
    }
    if((uint32_t)(uintptr_t)&toffset != GK_TOFFSET_ADDRESS)
    {
        klog("kernel: toffset is invalid\n");
        BKPT();
        while(true);
    }

    auto ithread = (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) ? idle_thread_dbg :
        idle_thread;
    Schedule(Thread::Create("idle_cm7", ithread, (void*)0, true, GK_PRIORITY_IDLE, kernel_proc, CPUAffinity::M7Only,
        memblk_allocate_for_stack(512, CPUAffinity::PreferM4, "idle_cm7 stack")));

    init_sd();
    init_ext4();
    init_screen();
    init_buttons();
    init_ctp();

#if GK_ENABLE_PROFILE
    init_profile();
#endif

    auto init_stack = memblk_allocate(8192, MemRegionType::AXISRAM, "init thread stack");
    if(!init_stack.valid)
        init_stack = memblk_allocate(8192, MemRegionType::SDRAM, "init thread stack");
    Schedule(Thread::Create("init", init_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::PreferM4, init_stack));

    Schedule(Thread::Create("gpu", gpu_thread, nullptr, true, GK_PRIORITY_VHIGH, kernel_proc, CPUAffinity::PreferM4));

    // Prepare systick
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 7680000UL - 1UL;    // 20 ms tick at 384 MHz

    //BKPT();

    s().StartForCurrentCore();
    while(true);

    return 0;
}

void *idle_thread(void *p)
{
    (void)p;
    while(true)
    {
        __asm__ volatile("yield \n" ::: "memory");
        __WFI();
    }
}

void *idle_thread_dbg(void *p)
{
    (void)p;
    while(true)
    {
        __asm__ volatile("yield \n" ::: "memory");
    }
}

extern "C" void SysTick_Handler()
{
    Yield();
}
