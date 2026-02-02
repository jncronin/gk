#include "retram.h"
#include "vmem.h"
#include <stm32mp2xx.h>

#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))

void init_retram()
{
    RCC_VMEM->RETRAMCFGR = RCC_RETRAMCFGR_RETRAMEN |
        RCC_RETRAMCFGR_RETRAMLPEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    PWR_VMEM->CR10 = 1U;        // backup supply in Standby and VBAT modes
    __asm__ volatile("dsb sy\n" ::: "memory");
}
