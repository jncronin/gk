#include <stm32h7rsxx.h>
#include "SEGGER_RTT.h"
#include "thread.h"
#include "gk_conf.h"

// there are up to 155 maskable interrupts on the NVIC, plus 16 interrupt lines, plus 1 word for stack pointer */
const int nvtors = 155 + 16 + 1;
__attribute__((aligned(1024))) __attribute__((section(".vtors"))) static uint32_t cm7_vtor[nvtors] = { 0 };

extern int _ecm7_stack;

void init_clocks();

static void init_nvic()
{
#if GK_USE_IRQ_PRIORITIES
    /* NVIC priorities:
        All faults priority 0
         Peripheral IRQs priority 1
         SVCall and PendSV lowest priority i.e. they can be interrupted but cannot interrupt each other */
    
    // H7 has 16 interrupt priorities for both M4 and M7
    static_assert(__NVIC_PRIO_BITS == 4U);
    NVIC_SetPriority(SVCall_IRQn, 0xfU);
    NVIC_SetPriority(PendSV_IRQn, 0xfU);
    NVIC_SetPriority(HardFault_IRQn, 0);
    NVIC_SetPriority(MemoryManagement_IRQn, 0);
    NVIC_SetPriority(UsageFault_IRQn, 0);
    NVIC_SetPriority(BusFault_IRQn, 0);

    for(int i = 0; i < nvtors; i++)
    {
        NVIC_SetPriority((IRQn_Type)i, 1U);
    }
#endif
}

void system_init_cm7()
{
    // enable FPU
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */

    // enable bus/usage/memmanage faults
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk |
        SCB_SHCSR_BUSFAULTENA_Msk |
        SCB_SHCSR_MEMFAULTENA_Msk;
   
    // copy VTORs to ram
    uint32_t *orig_vtors = (uint32_t *)0x08000000;
    cm7_vtor[0] = (uint32_t)(uintptr_t)&_ecm7_stack;
    for(int i = 1; i < nvtors; i++)
    {
        cm7_vtor[i] = orig_vtors[i];
    }
    SCB->VTOR = (uint32_t)(uintptr_t)&cm7_vtor[0];

    init_nvic();

    // enable detection of CPU interrupt whilst in WFI state
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

    // disable FMC1 speculative access
    FMC_Bank1_R->BTCR[0] = 0x000030D2;

#if GK_USE_CACHE
#if GK_USE_MPU
    __DMB();
    for(int i = 0; i < 16; i++)
    {
        MPU->RBAR = mpu_default[i].rbar;
        MPU->RASR = mpu_default[i].rasr;
    }
    __DSB();
    __ISB();
    MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
#endif

    // Enable caches
    SCB_InvalidateDCache();
    SCB_InvalidateICache();
    SCB_EnableICache();
    SCB_EnableDCache();
#endif
}
