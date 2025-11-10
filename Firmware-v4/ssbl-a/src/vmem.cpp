/* 
    Set up the lower half paging structures for the secure monitor in TTBR0_EL3.

    We define a 64 kiB granularity and 42-bit virtual address space.
    This means only two levels of page tables are required.

    The first (level 2) covers bits 41:29 and so can reference blocks of
        512 MiB = 0x2000 0000.
    This is sufficient granularity for the physical address space to
        be identity mapped with the appropriate memory types (device vs normal).
    We ignore the 0x2000 0000 - 0x4000 0000 region here as this is mapped at
        higher granularity to the secure monitor itself.
*/

#include <cstdint>
#include "logger.h"
#include "vmem.h"
#include "gkos_vmem.h"

/* basic pmem allocator in DDR */
uint64_t cur_pmem_brk = 0x80000000ULL;

static uint64_t pmem_alloc(uint64_t size = GRANULARITY, bool clear = true)
{
    // align up to granule size
    size = (size + (GRANULARITY - 1ULL)) & ~(GRANULARITY - 1ULL);

    auto ret = cur_pmem_brk;
    cur_pmem_brk += size;

    if(clear)
    {
        quick_clear_64((void *)ret, size);
    }
    return ret;
}

/* store pd + pt for 20000000-40000000 region */
static uint64_t pd = 0;
static uint64_t pt = 0;

void init_vmem(int el)
{
    if(el != 3)
    {
        klog("vmem: invalid el: %d\n", el);
        while(true);
    }
    pd = pmem_alloc();
    pt = pmem_alloc();

    // MMU types
    __asm__ volatile("msr mair_el3, %[mair] \n" : : [mair] "r" (mair) : "memory");

    // Lower half page directory
    __asm__ volatile("msr ttbr0_el3, %[pd] \n" : : [pd] "r" (pd) : "memory");

    // Disable level 2 paging
    __asm__ volatile("msr hcr_el2, %[hcr_el2] \n" : : [hcr_el2] "r" (0) : "memory");

    // Get tcr_el3
    uint64_t tcr_el3 = 0;
    tcr_el3 |= (64ULL - 42ULL) << 0;                // 42 bit lower half paging
    tcr_el3 |= (0x1ULL << 8) | (0x1ULL << 10);      // cached accesses to page tables
    tcr_el3 |= (0x3ULL << 12);                      // shareable page tables
    tcr_el3 |= 1ULL << 14;                          // 64 kiB granule
    tcr_el3 |= 2ULL << 16;                          // intermediate physical address 40 bits
    __asm__ volatile("msr tcr_el3, %[tcr_el3] \n" : : [tcr_el3] "r" (tcr_el3) : "memory");

    /* identity map, include standard 32-bit device map as RW, secure, XN */
    volatile uint64_t *pd_entries = (volatile uint64_t *)pd;

    for(auto i = 0ULL; i < 128ULL; i++)
    {
        if(i == 1)                              // 0x20000000 - 0x3fffffff - map a page table
        {
            uint64_t attr = DT_PT | PAGE_ACCESS;
            pd_entries[i] = pt | attr;
        }
        else
        {
            uint64_t attr = PAGE_ACCESS | PAGE_INNER_SHAREABLE | DT_BLOCK;
            
            if(i == 2)
                attr |= PAGE_ATTR(MT_DEVICE);       // 0x40000000 - 0x5fffffff
            else
                attr |= PAGE_ATTR(MT_NORMAL);

            if(i == 3)
                attr |= PAGE_PRIV_RO;               // 0x60000000 - QSPI/FMC NOR flash
            else
                attr |= PAGE_PRIV_RW;

            pd_entries[i] = (0x20000000ULL * i) |
                attr;
        }
    }
}

uint64_t pmem_vaddr_to_paddr(uint64_t vaddr, bool writeable, bool xn, int el)
{
    if(vaddr < 0x20000000 || vaddr >= 0x40000000)
    {
        klog("vmem: invalid vaddr: %llx\n", vaddr);
        return 0;
    }

    auto pt_entries = (volatile uint64_t *)pt;
    auto pt_idx = (vaddr >> 16) & 0x1fffULL;
    if(pt_entries[pt_idx] == 0)
    {
        uint64_t attrs = DT_PAGE |
            PAGE_ACCESS |
            PAGE_INNER_SHAREABLE |
            PAGE_ATTR(MT_NORMAL);
        if(xn)
            attrs |= PAGE_XN;
        if(writeable)
            attrs |= PAGE_PRIV_RW;
        else
            attrs |= PAGE_PRIV_RO;
        pt_entries[pt_idx] = pmem_alloc(GRANULARITY, false) |
            attrs;        
    }

    return pt_entries[pt_idx] & 0xffffffff0000ULL;
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

uint64_t pmem_get_cur_brk()
{
    return cur_pmem_brk;
}

uint64_t pmem_paddr_to_vaddr(uint64_t paddr, int el)
{
    switch(el)
    {
        case 1:
            return UH_START + paddr;
        default:
            klog("vmem: unsupported el: %d\n", el);
            while(true);
    }
}
