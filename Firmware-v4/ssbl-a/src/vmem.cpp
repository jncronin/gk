/* 
    Set up the higher half paging structures for the kernel in TTBR1_EL1.

    We define a 64 kiB granularity and 42-bit virtual address space.
    This means only two levels of page tables are required.

    The first (level 2) covers bits 41:29 and so can reference blocks of
        512 MiB = 0x2000 0000.
    This is sufficient granularity for the physical address space to
        be identity mapped with the appropriate memory types (device vs normal).

    A 42-bit higher half covers:
        0xffff fc00 0000 0000
         to
        0xffff ffff ffff ffff

    Therefore, we map the physical memory to the lowest address here.
    Physical memory on MP2 is 33 bits, so we scale up to 36 bits here.
        0xffff fc00 0000 0000
         to
        0xffff fc0f ffff ffff (we could go to 40 bit here and have 2nd part be fcff...)
    
    This is all possible in the first level (level 2) table using the first 128
        8 byte entries in the 64 kiB page directory.

    Then, we map the actual kernel to the -4 GiB mark (or wherever it is specified in
        ELF... as long as > minimum)
    -4GiB =
        0xffff ffff 0000 0000
    -2GiB =
        0xffff ffff 8000 0000

    This means we need a further 16/15x 64 kiB page tables to map, addressed by the
        last 16/15 entries in the page directory.
*/

#include <cstdint>
#include "log.h"
#include "vmem.h"

/* We get to define our own memory types for the MMU.

    Slot    0       =   write back inner/outer shareable normal memory  = 0xff
    Slot    1       =   write through inner/outer shareable normal mem  = 0x99
    Slot    2       =   non cacheable normal memory                     = 0x44
    Slot    3       =   nGnRnE device memory                            = 0x00
    Slot    4       =   nGnRE device memory                             = 0x04
    
    other 3 are zeros (i.e. nGnRnE device memory) */

#define MT_NORMAL           0
#define MT_NORMAL_WT        1
#define MT_NORMAL_NC        2
#define MT_DEVICE           3
#define MT_DEVICE_NGNRE     4

// e.g. (TODO: put this in asm file which enables paging)
const uint64_t mair = 0x04004499ff;

/* basic pmem allocator in DDR */
uint64_t cur_pmem_brk = 0x80000000ULL;

#define GRANULARITY 65536ULL
#define PTS_BASE    0xffffffff00000000ULL
#define UH_START    0xfffffc0000000000

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

/* store pd + high 16 pts */
static uint64_t pd = 0;
static uint64_t pts[16] = { 0 };

#define DT_BLOCK    0x1ULL
#define DT_PAGE     0x3ULL
#define DT_PT       0x3ULL

#define PAGE_XN                 (0x3ULL << 53)
#define PAGE_NG                 (0x1ULL << 11)
#define PAGE_ACCESS             (0x1ULL << 10)
#define PAGE_INNER_SHAREABLE    (0x3ULL << 8)
#define PAGE_PRIV_RW            (0x0ULL << 6)
#define PAGE_PRIV_RO            (0x2ULL << 6)
#define PAGE_NON_SECURE         (0x1ULL << 5)
#define PAGE_ATTR(x)            ((x) << 2)

void init_vmem()
{
    pd = pmem_alloc();

    // MMU types
    __asm__ volatile("msr mair_el1, %[mair] \n" : : [mair] "r" (mair) : "memory");

    // Upper half page directory
    __asm__ volatile("msr ttbr1_el1, %[pd] \n" : : [pd] "r" (pd) : "memory");

    // Disable level 2 paging
    __asm__ volatile("msr hcr_el2, %[hcr_el2] \n" : : [hcr_el2] "r" (0) : "memory");

    /* identity map, include standard 32-bit device map as RW, secure, XN */
    volatile uint64_t *pd_entries = (volatile uint64_t *)pd;

    for(auto i = 0ULL; i < 128ULL; i++)
    {
        uint64_t attr = PAGE_XN | PAGE_ACCESS | PAGE_INNER_SHAREABLE | DT_BLOCK;

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

uint64_t pmem_vaddr_to_paddr(uint64_t vaddr, bool writeable, bool xn)
{
    auto adj_vaddr = vaddr - PTS_BASE;
    auto pts_idx = adj_vaddr >> 29;

    if(pts_idx >= (sizeof(pts) / sizeof(pts[0])))
    {
        klog("vmem: invalid vaddr: %llx\n", vaddr);
        return 0;
    }

    if(pts[pts_idx] == 0)
    {
        pts[pts_idx] = pmem_alloc();

        auto pd_idx = (vaddr - UH_START) >> 29;
        volatile uint64_t *pd_entries = (volatile uint64_t *)pd;
        pd_entries[pd_idx] = pts[pts_idx] |
            DT_PT |
            PAGE_ACCESS;
    }

    auto pt_entries = (volatile uint64_t *)pts[pts_idx];
    auto pt_idx = (vaddr >> 16) & 0x1fffULL;
    if(pt_entries[pt_idx] == 0)
    {
        uint64_t attrs = DT_PAGE |
            PAGE_ACCESS |
            PAGE_INNER_SHAREABLE;
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

void pmem_map_region(uint64_t base, uint64_t size, bool writeable, bool xn)
{
    auto start = base & ~(GRANULARITY - 1ULL);
    auto end = (base + size + (GRANULARITY - 1ULL)) & ~(GRANULARITY - 1ULL);

    for(auto i = start; i < end; i += GRANULARITY)
    {
        pmem_vaddr_to_paddr(i, writeable, xn);
    }
}
