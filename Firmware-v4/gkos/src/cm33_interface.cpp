#include <stm32mp2xx.h>
#include "vmem.h"
#include "gic.h"
#include "cm33_interface.h"

#define RISAF2_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF2_BASE))
#define RAMCFG_VMEM ((RAMCFG_TypeDef *)PMEM_TO_VMEM(RAMCFG_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define RISAB3_VMEM ((RISAB_TypeDef *)PMEM_TO_VMEM(RISAB3_BASE))
#define RISAB4_VMEM ((RISAB_TypeDef *)PMEM_TO_VMEM(RISAB4_BASE))
#define RISC_VMEM ((RISC_TypeDef *)PMEM_TO_VMEM(RISC_BASE))
#define CA35SYSCFG_VMEM ((CA35SYSCFG_TypeDef *)PMEM_TO_VMEM(CA35SYSCFG_BASE))
#define EXTI1_VMEM ((EXTI_TypeDef *)PMEM_TO_VMEM(EXTI1_BASE))

static void cm33_irq(exception_regs *, uint64_t);

void init_cm33_interface()
{
    /* CM33 runs code from OSPI and uses SRAM1 as data area and SRAM2 for comms with A35

        It uses LPTIM1 as a 5 ms timer

        It has access to I2C1 for interface with the IO expander and LSM6DSL

        It has access to ADC1 for joystick readings
    */

    // Give CM33 secure access to OSPI
    RISAF2_VMEM->REG[0].CFGR = 0;
    RISAF2_VMEM->REG[0].STARTR = 0x3fc000;
    RISAF2_VMEM->REG[0].ENDR = 0x3fffff;
    RISAF2_VMEM->REG[0].CIDCFGR = 7U;    // TRACE/CPU0/CPU1
    RISAF2_VMEM->REG[0].CFGR = 0xf0101;  // all privilege, secure, enable

    // Enable SRAM1 and 2
    RCC_VMEM->SRAM1CFGR |= RCC_SRAM1CFGR_SRAM1EN;
    RCC_VMEM->SRAM2CFGR |= RCC_SRAM2CFGR_SRAM2EN;
    __asm__ volatile ("dsb sy\n" ::: "memory");

    // RISAB3 (SRAM1) and RISAB4 (SRAM2) each have 32 pages of 8 blocks each
    for(auto risab : { RISAB3_VMEM, RISAB4_VMEM })
    {
        for(unsigned int page = 0; page < 32; page++)
        {
            // Secure accesses only
            risab->PGSECCFGR[page] = 0xffU;
            // Privileged accesses only
            risab->PGPRIVCFGR[page] = 0xffU;
            // CID filtering
            risab->PGCIDCFGR[page] = RISAB_PGCIDCFGR_CFEN;
        }

        // Give privileged read/write access to CIDs 0,1,2
        for(unsigned int cid = 0; cid < 3; cid++)
        {
            risab->CID[cid].PRIVCFGR = 0xffffffffU;
            risab->CID[cid].RDCFGR = 0xffffffffU;
            risab->CID[cid].WRCFGR = 0xffffffffU;
        }
    }

    // Start up the CM33 code running from QSPI @ 0x603fc000
    // Boot in secure mode
    RCC_VMEM->SYSCPU1CFGR |= RCC_SYSCPU1CFGR_SYSCPU1EN;
    (void)RCC_VMEM->SYSCPU1CFGR;
    CA35SYSCFG_VMEM->M33_TZEN_CR |= CA35SYSCFG_M33_TZEN_CR_CFG_SECEXT;
    CA35SYSCFG_VMEM->M33_INITSVTOR_CR = 0x603fc000;
    RCC_VMEM->CPUBOOTCR &= ~RCC_CPUBOOTCR_BOOT_CPU2;
    (void)RCC_VMEM->CPUBOOTCR;
    RCC_VMEM->C2RSTCSETR = RCC_C2RSTCSETR_C2RST;
    while(RCC_VMEM->C2RSTCSETR & RCC_C2RSTCSETR_C2RST);

    /* Start CPU2 */
    RCC_VMEM->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;

    /* Allow CM33 to send interrupts to us - EXTI1 channel 64 */
    EXTI1_VMEM->RTSR3 |= (1U << 0);

    unsigned int cm33_sev_irq = 252U;
    gic_set_handler(cm33_sev_irq, cm33_irq);
    gic_set_target(cm33_sev_irq, GIC_ENABLED_CORES);
    gic_set_enable(cm33_sev_irq);

    klog("cm33: CPU2 started\n");
}

void cm33_irq(exception_regs *, uint64_t)
{
    klog("cm33: irq\n");
    EXTI1_VMEM->RPR3 = (1U << 0);
    __DMB();
}
