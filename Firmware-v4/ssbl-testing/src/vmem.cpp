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

/* store pd */
static uint64_t pd = 0x0e000000;

void init_vmem(int el)
{
    if(el != 3)
    {
        klog("vmem: invalid el: %d\n", el);
        while(true);
    }

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
