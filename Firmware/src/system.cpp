#include <stm32h7xx.h>

// there are up to 150 maskable interrupts on the NVIC, plus 16 interrupt lines, plus 1 word for stack pointer */
const int nvtors = 150 + 16 + 1;
__attribute__((aligned(1024))) __attribute__((section(".dtcm_bss"))) static uint32_t cm7_vtor[nvtors] = { 0 };
__attribute__((aligned(1024))) __attribute__((section(".sram_bss"))) static uint32_t cm4_vtor[nvtors] = { 0 };

extern int _ecm7_stack;
extern int _ecm4_stack;

void system_init_cm7()
{
    // enable FPU
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
   
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

    // disable LDO
    PWR->CR3 &= ~PWR_CR3_LDOEN;

    // perform clock init .. TODO
}

void system_init_cm4()
{
    // enable FPU
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */

    // copy VTORs to ram
    uint32_t *orig_vtors = (uint32_t *)0x08000000;
    cm4_vtor[0] = (uint32_t)(uintptr_t)&_ecm4_stack;
    for(int i = 1; i < nvtors; i++)
    {
        cm4_vtor[i] = orig_vtors[i];
    }
    SCB->VTOR = (uint32_t)(uintptr_t)&cm4_vtor[0];

    // enable detection of CPU interrupt whilst in WFI state
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;
}
