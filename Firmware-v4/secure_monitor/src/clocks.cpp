#include "clocks.h"
#include "vblock.h"
#include "vmem.h"
#include "gkos_vmem.h"
#include <stm32mp2xx.h>

#define TIM3_VMEM ((TIM_TypeDef *)PMEM_TO_VMEM(TIM3_BASE))

uint64_t clock_block_vaddr = 0;
uint64_t clock_block_paddr = 0;

uintptr_t _clocks_cur_s_address()
{
    if(clock_block_vaddr == 0)
        return 0x0e0bfe00;      // SSBL-A address
    else
        return clock_block_vaddr;
}

#define _cur_s (volatile uint64_t *)_clocks_cur_s_address()

void clock_takeover()
{
    auto cb_vaddr = vmem_alloc(GRANULARITY);
    clock_block_paddr = pmem_vaddr_to_paddr(cb_vaddr, true, true, 3);

    *(volatile uint64_t *)(cb_vaddr) = *(volatile uint64_t *)(0x0e0bfe00);
    *(volatile uint64_t *)(cb_vaddr + 8) = *(volatile uint64_t *)(0x0e0bfe00 + 8);

    clock_block_vaddr = cb_vaddr;
    __asm__ volatile("msr daifclr, #0x3\n");
}

void clock_irq_handler()
{
    TIM3_VMEM->SR = 0;
    *_cur_s = *_cur_s + 1;
}
