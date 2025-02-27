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
#include "cleanup.h"
#include "pwr.h"
#include "btnled.h"
#include "osnet.h"
#include "tilt.h"
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
    btnled_setcolor_init(0x00ff00);

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
#define GK_MEMTEST_DUMP_ALL 0
#define GK_MEMTEST_REPEAT 1
#if GK_MEMTEST
    uint32_t error_bits = 0;
#if GK_XSPI_DUAL_MEMORY
    const uint32_t test_max = 0x98000000U;
#else
    const uint32_t test_max = 0x94000000U;
#endif
    for(uint32_t addr = test_max - 4U; addr >= 0x90000000U; addr -= 4)
    {
        *(volatile uint32_t *)addr = addr;
    }
#if GK_MEMTEST == 2
    uint64_t seed = 123456789U;
#endif
    for(uint32_t addr = test_max - 4U; addr >= 0x90000000U; addr -= 4)
    {
        auto v = *(volatile uint32_t *)addr;
        if(v != addr)
        {
            error_bits |= (addr ^ v);
#if GK_MEMTEST_DUMP_ALL
            SEGGER_RTT_printf(0, "memchk: fail at %08x - got %08x (error bits: %08x)\n", addr, v, error_bits);
#if GK_MEMTEST == 2
            // try again
            for(int i = 0; i < 10; i++)
            {
                // do some random dummy reads to flush prefetch buffer (not sure how big it is)
                for(int j = 0; j < 1024; j++)
                {
                    seed = (1103515245ULL * seed + 12345ULL) % 0x80000000ULL;
                    auto new_addr = 0x90000000U + (uint32_t)(seed % (unsigned long long)(test_max - 0x90000000UL));
                    *(volatile uint32_t *)(new_addr & ~0x3U);
                }
                auto v2 = *(volatile uint32_t *)addr;
                SEGGER_RTT_printf(0, "memchk:   retry %d, got %x (%s)\n", i, v2,
                    (v2 == addr) ? "SUCCESS" : "FAIL");
            }
#endif
#endif
        }
    }

    // recheck calibration after test
    (void)*(volatile uint32_t *)(test_max - 4U);
    __DMB();
    __DSB();
    XSPI1->CR |= XSPI_CR_ABORT;
    while((XSPI1->CR & XSPI_CR_ABORT) || (XSPI1->SR & XSPI_SR_BUSY));

    // trigger calibration
    XSPI1->DCR2 = XSPI1->DCR2;

    // dummy read
    (void)*(volatile uint32_t *)(test_max - 0x1000U);
    SEGGER_RTT_printf(0, "kernel: xspi1: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI1->CALFCR, XSPI1->CALSOR, XSPI1->CALSIR);
    SEGGER_RTT_printf(0, "kernel: xspi2: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI2->CALFCR, XSPI2->CALSOR, XSPI2->CALSIR);

    SEGGER_RTT_printf(0, "kernel: memtest: error_bits: %08x\n", error_bits);

#if GK_MEMTEST_REPEAT
    // repeat after recalibration.  Better?
    error_bits = 0;
    for(uint32_t addr = test_max - 4U; addr >= 0x90000000U; addr -= 4)
    {
        *(volatile uint32_t *)addr = addr;
    }
    for(uint32_t addr = test_max - 4U; addr >= 0x90000000U; addr -= 4)
    {
        auto v = *(volatile uint32_t *)addr;
        if(v != addr)
        {
            error_bits |= (addr ^ v);
        }
    }

    // recheck calibration after test
    (void)*(volatile uint32_t *)(test_max - 4U);
    __DMB();
    __DSB();
    XSPI1->CR |= XSPI_CR_ABORT;
    while((XSPI1->CR & XSPI_CR_ABORT) || (XSPI1->SR & XSPI_SR_BUSY));

    // trigger calibration
    XSPI1->DCR2 = XSPI1->DCR2;

    // dummy read
    (void)*(volatile uint32_t *)(test_max - 0x1000U);
    SEGGER_RTT_printf(0, "kernel: xspi1: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI1->CALFCR, XSPI1->CALSOR, XSPI1->CALSIR);
    SEGGER_RTT_printf(0, "kernel: xspi2: calfcr: %x, calsor: %x, calsir: %x\n",
        XSPI2->CALFCR, XSPI2->CALSOR, XSPI2->CALSIR);

    SEGGER_RTT_printf(0, "kernel: memtest repeat: error_bits: %08x\n", error_bits);
#endif
#endif

    btnled_setcolor_init(0xffff00);
    BKPT_IF_DEBUGGER();
    btnled_setcolor_init(0xff00ff);

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
        BKPT_OR_RESET();
        while(true);
    }
    if((uint32_t)(uintptr_t)&_cur_ms != GK_CUR_MS_ADDRESS)
    {
        klog("kernel: _cur_ms is invalid\n");
        BKPT_OR_RESET();
        while(true);
    }
    if((uint32_t)(uintptr_t)&toffset != GK_TOFFSET_ADDRESS)
    {
        klog("kernel: toffset is invalid\n");
        BKPT_OR_RESET();
        while(true);
    }

    auto ithread = (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) ? idle_thread_dbg :
        idle_thread;
    Schedule(Thread::Create("idle_cm7", ithread, (void*)0, true, GK_PRIORITY_IDLE, kernel_proc, CPUAffinity::M7Only,
        memblk_allocate_for_stack(512, CPUAffinity::PreferM4, "idle_cm7 stack")));

    btnled_setcolor_init(0xff);

    init_sd();
    if(!(reboot_flags & GK_REBOOTFLAG_RAWSD))
    {
        init_ext4();
    }
    init_log();
    init_screen();
    if(reboot_flags == 0)
        init_buttons();
    init_btnled();
    if(reboot_flags == 0)
        init_ctp();

#if GK_ENABLE_PROFILE
    init_profile();
#endif

    auto init_stack = memblk_allocate(8192, MemRegionType::AXISRAM, "init thread stack");
    if(!init_stack.valid)
        init_stack = memblk_allocate(8192, MemRegionType::SDRAM, "init thread stack");
    Schedule(Thread::Create("init", init_thread, nullptr, true, GK_PRIORITY_NORMAL, kernel_proc, CPUAffinity::PreferM4, init_stack));

    Schedule(Thread::Create("gpu", gpu_thread, nullptr, true, GK_PRIORITY_VHIGH, kernel_proc, CPUAffinity::PreferM4));

    Schedule(Thread::Create("cleanup", cleanup_thread, (void*)0, true, GK_PRIORITY_VHIGH, kernel_proc,
        CPUAffinity::M7Only,
        memblk_allocate_for_stack(4096, CPUAffinity::PreferM4, "cleanup stack")));

    Schedule(Thread::Create("pwr_monitor", pwr_monitor_thread, nullptr, true, GK_PRIORITY_HIGH, kernel_proc,
        CPUAffinity::PreferM4));

#if GK_ENABLE_NETWORK
    if(reboot_flags == 0)
        init_net();
#endif

#if GK_ENABLE_TILT
    if(reboot_flags == 0)
        init_tilt();
#endif

    // Prepare systick
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 7680000UL - 1UL;    // 20 ms tick at 384 MHz

    // Recheck IO compensation cell calibration
    SBS->CCSWVALR = SBS->CCVALR;
    (void)SBS->CCSWVALR;

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
