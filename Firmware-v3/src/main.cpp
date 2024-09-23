#include <stm32h7rsxx.h>
#include <cstring>
#include "pins.h"
#include "i2c.h"
#include "memblk.h"
#include "scheduler.h"
#include "process.h"
#include "SEGGER_RTT.h"

uint32_t test_val;

uint32_t test_range[256];

static const constexpr pin CTP_NRESET { GPIOC, 0 };
void system_init_cm7();

Scheduler sched;
Process kernel_proc;
Process p_supervisor;

SRAM4_DATA std::vector<std::string> empty_string_vector;
SRAM4_DATA MemRegion memblk_persistent_log;

SRAM4_DATA pid_t pid_gkmenu = 0;

extern uint32_t _tls_pointers[2];
extern uint64_t _cur_ms;
extern struct timespec toffset;

// default environment variables
SRAM4_DATA std::vector<std::string> gk_env;


void *idle_thread(void *p);

int main()
{
    system_init_cm7();

    init_memblk();

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

    Schedule(Thread::Create("idle_cm7", idle_thread, (void*)0, true, GK_PRIORITY_IDLE, kernel_proc, CPUAffinity::M7Only,
        memblk_allocate_for_stack(512, CPUAffinity::PreferM4, "idle_cm7 stack")));

    
    // Prepare systick
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 7680000UL - 1UL;    // 20 ms tick at 384 MHz

    BKPT();

    s().StartForCurrentCore();

    return 0;
}

void *idle_thread(void *p)
{
    (void)p;
    while(true)
    {
        __WFI();
    }
}

extern "C" void SysTick_Handler()
{
    Yield();
}
