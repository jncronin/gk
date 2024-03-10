#include <stm32h7xx.h>
#include "SEGGER_RTT.h"
#include "thread.h"

// there are up to 150 maskable interrupts on the NVIC, plus 16 interrupt lines, plus 1 word for stack pointer */
const int nvtors = 150 + 16 + 1;
__attribute__((aligned(1024))) __attribute__((section(".dtcm_bss"))) static uint32_t cm7_vtor[nvtors] = { 0 };

extern int _ecm7_stack;
extern int _ecm4_stack;
extern int _scm4_vtors;

extern Spinlock s_rtt;

void init_clocks();

void system_init_cm7()
{
    // disable LDO, enable SMPS - can only write lower byte once
    PWR->CR3 = (PWR->CR3 & ~0xffUL) | PWR_CR3_SMPSEN;
    // check valid
    while(!(PWR->CSR1 & PWR_CSR1_ACTVOSRDY));

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

    // enable detection of CPU interrupt whilst in WFI state
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

    // from stm32 startup
    if((DBGMCU->IDCODE & 0xFFFF0000U) < 0x20000000U)
    {
    /* if stm32h7 revY*/
    /* Change  the switch matrix read issuing capability to 1 for the AXI SRAM target (Target 7) */
    *((__IO uint32_t*)0x51008108) = 0x000000001U;
    }

    // disable FMC1 speculative access
    FMC_Bank1_R->BTCR[0] = 0x000030D2;

    // perform clock init
    init_clocks();

    // Enable caches
    SCB_InvalidateDCache();
    SCB_InvalidateICache();
    SCB_EnableICache();
    SCB_EnableDCache();
}

void system_init_cm4()
{
    // enable FPU
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */

    // enable bus/usage/memmanage faults
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk |
        SCB_SHCSR_BUSFAULTENA_Msk |
        SCB_SHCSR_MEMFAULTENA_Msk;

    // SRAM enable
    RCC->AHB2ENR |= RCC_AHB2ENR_D2SRAM1EN |
        RCC_AHB2ENR_D2SRAM2EN |
        RCC_AHB2ENR_D2SRAM3EN;
    (void)RCC->AHB2ENR;

    // copy VTORs to ram
    uint32_t *orig_vtors = (uint32_t *)0x08000000;
    auto cm4_vtor = reinterpret_cast<uint32_t *>(&_scm4_vtors);
    cm4_vtor[0] = (uint32_t)(uintptr_t)&_ecm4_stack;
    {
        //CriticalGuard cg(s_rtt);
        for(int i = 1; i < nvtors; i++)
        {
            cm4_vtor[i] = orig_vtors[i];
            //SEGGER_RTT_printf(0, "m4irq: %d = %x\n", i, orig_vtors[i]);
        }
    }
    SCB->VTOR = (uint32_t)(uintptr_t)&cm4_vtor[0];

    // enable detection of CPU interrupt whilst in WFI state
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

    // enable caches
    RCC->AHB1ENR |= RCC_AHB1ENR_ARTEN;
    (void)RCC->AHB1ENR;
    ART->CTR = 0x08000000UL | ART_CTR_EN;       // FLASH A (most code in flash A) + enable
}
