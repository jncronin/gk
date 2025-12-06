#include "vmem.h"
#include "gkos_vmem.h"
#include "logger.h"
#include "osspinlock.h"
#include <cstdint>

#define PAGE_ADDR_MASK 0x0000ffffffff0000ULL

extern uint64_t ddr_start, ddr_end;

Spinlock sl_pmem;
Spinlock sl_vmem;

uint64_t pmem_alloc(uint64_t size = GRANULARITY, bool clear = true)
{
    // align up to granule size
    uint64_t ret;

    size = (size + (GRANULARITY - 1ULL)) & ~(GRANULARITY - 1ULL);

    {
        CriticalGuard cg(sl_pmem);
        ret = ddr_start;
        ddr_start += size;
    }

    if(clear)
    {
        quick_clear_64((void *)ret, size);
    }
    return ret;
}

uint64_t pmem_vaddr_to_paddr_el3(uint64_t vaddr, bool writeable, bool xn, uint64_t mt = MT_NORMAL,
    uintptr_t paddr = 0)
{
    if(vaddr < 0x20000000 || vaddr >= 0x40000000)
    {
        klog("vmem: invalid vaddr: %llx\n", vaddr);
        return 0;
    }

    CriticalGuard cg(sl_vmem);

    // get paging base
    uint64_t ttbr_el3;
    __asm__ volatile("mrs %[ttbr_el3], ttbr0_el3\n" : [ttbr_el3] "=r" (ttbr_el3) : : "memory");
    ttbr_el3 &= PAGE_ADDR_MASK;
    ttbr_el3 = PMEM_TO_VMEM(ttbr_el3);

    auto pd = (volatile uint64_t *)ttbr_el3;
    auto pt = PMEM_TO_VMEM(pd[1] & PAGE_ADDR_MASK);

    auto pt_entries = (volatile uint64_t *)pt;
    auto pt_idx = (vaddr >> 16) & 0x1fffULL;
    if(pt_entries[pt_idx] == 0)
    {
        uint64_t attrs = DT_PAGE |
            PAGE_ACCESS |
            PAGE_INNER_SHAREABLE |
            PAGE_ATTR(mt);
        if(xn)
            attrs |= PAGE_XN;
        if(writeable)
            attrs |= PAGE_PRIV_RW;
        else
            attrs |= PAGE_PRIV_RO;
        
        if(paddr == 0)
            paddr = pmem_alloc(GRANULARITY, false);
        pt_entries[pt_idx] = paddr |
            attrs;        
    }

    return (pt_entries[pt_idx] & PAGE_ADDR_MASK) | (vaddr & 0xffffULL);
}

uint64_t pmem_vaddr_to_paddr(uint64_t vaddr, bool writeable, bool xn, int el, uint64_t mt,
    uintptr_t paddr)
{
    switch(el)
    {
        case 3:
            return pmem_vaddr_to_paddr_el3(vaddr, writeable, xn, mt, paddr);

        case 1:
            uint64_t pmem_vaddr_to_paddr1(uint64_t vaddr, bool writeable, bool xn);
            return pmem_vaddr_to_paddr1(vaddr, writeable, xn);
            
        default:
            klog("vmem: invalid el: %d\n", el);
            return 0;
    }
}

void pmem_map_region(uint64_t base, uint64_t size, bool writeable, bool xn, int el)
{
    auto start = base & ~(GRANULARITY - 1ULL);
    auto end = (base + size + (GRANULARITY - 1ULL)) & ~(GRANULARITY - 1ULL);

    for(auto i = start; i < end; i += GRANULARITY)
    {
        pmem_vaddr_to_paddr(i, writeable, xn, el);
    }
}
