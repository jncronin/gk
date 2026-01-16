#include <stm32mp2xx.h>
#include "memchk.h"
#include "vmem.h"
#include "gic.h"
#include "thread.h"
#include "process.h"

#define IAC_VMEM ((IAC_TypeDef *)PMEM_TO_VMEM(IAC_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define EXTI2_VMEM ((EXTI_TypeDef *)PMEM_TO_VMEM(EXTI2_BASE))
#define RISAF1_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF1_BASE))
#define RISAF2_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF2_BASE))
#define RISAF3_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF3_BASE))
#define RISAF4_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF4_BASE))
#define RISAF5_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF5_BASE))
#define RISAF6_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF6_BASE))


static void iac_irq(exception_regs *regs, uint64_t elr_el1);

/* Enable the various STM32 invalid address checking mechanisms */
void init_memchk()
{
    for(auto i = 0U; i <= 5; i++)
    {
        IAC_VMEM->IER[i] = 0xffffffffU;
    }

    EXTI2_VMEM->C1IMR2 |= (1U << 30);

    gic_set_handler(278, iac_irq);
    gic_set_target(278, GIC_ENABLED_CORES);
    gic_set_enable(278);
}

static void iac_risaf(unsigned int id, RISAF_TypeDef *risaf, uintptr_t baseaddr)
{
    for(unsigned int port = 0; port < 2; port++)
    {
        if(risaf->IASR & (1U << (port + 1)))
        {
            auto illegal = risaf->IAR[port];
            klog("iac: RISAF%u[%u]: access to %x, sr: %x\n", id, port,
                baseaddr + illegal.IADDR, illegal.IAESR);
            risaf->IACR = 1U << (port + 1);
        }
    }
}

static void iac_isr(unsigned int rifidx)
{
    klog("iac: RIF ID: %u\n", rifidx);

    switch(rifidx)
    {
        case 134:
            iac_risaf(1, RISAF1_VMEM, BKP_SRAM_BASE);
            break;
        case 135:
            iac_risaf(2, RISAF2_VMEM, 0x60000000);
            break;
        case 137:
            iac_risaf(4, RISAF4_VMEM, 0x80000000);
            break;
        case 138:
            iac_risaf(5, RISAF5_VMEM, 0x10000000);
            break;
    }
}

void iac_irq(exception_regs *regs, uint64_t elr_el1)
{
    auto t = GetCurrentThreadForCore();
    if(t)
    {
        klog("iac: ILLEGAL ACCESS by %s.%s @ %p\n", GetCurrentProcessForCore()->name.c_str(),
            t->name.c_str(), elr_el1);
    }
    for(auto i = 0U; i <= 5; i++)
    {
        auto isr = IAC_VMEM->ISR[i];
        if(isr)
        {
            for(auto j = 0U; j < 32; j++)
            {
                if(isr & (1U << j))
                {
                    iac_isr(i * 32U + j);
                }
            }
        }
    }

    for(auto i = 0U; i <= 5; i++)
    {
        IAC_VMEM->ICR[i] = IAC_VMEM->ISR[i];
    }

    __DMB();
}
