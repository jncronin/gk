#ifndef GKOS_VMEM_H
#define GKOS_VMEM_H

#include <cstdint>

/* Store the vmem setup used by the SSBL for EL1 loading */
/* We get to define our own memory types for the MMU.

    Slot    0       =   write back inner/outer shareable normal memory  = 0xff
    Slot    1       =   write through inner/outer shareable normal mem  = 0xbb
    Slot    2       =   non cacheable normal memory                     = 0x44
    Slot    3       =   nGnRnE device memory                            = 0x00
    Slot    4       =   nGnRE device memory                             = 0x04
    
    other 3 are zeros (i.e. nGnRnE device memory) */

#define MT_NORMAL           0
#define MT_NORMAL_WT        1
#define MT_NORMAL_NC        2
#define MT_DEVICE           3
#define MT_DEVICE_NGNRE     4

const uint64_t mair = 0x040044bbff;

#define GRANULARITY 65536ULL
#define PTS_BASE    0xffffffff00000000ULL
#define UH_START            0xfffffc0000000000ULL
#define UH_DEVICE_START     0xfffffd0000000000ULL
#define LH_END      0x40000000000ULL

#define PAGE_PADDR_MASK         0xffffffff0000ULL
#define PAGE_VADDR_MASK         0xffffffffffff0000ULL

#define DT_BLOCK    0x1ULL
#define DT_PAGE     0x3ULL
#define DT_PT       0x3ULL

#define PAGE_XN                 (0x3ULL << 53)
#define PAGE_NG                 (0x1ULL << 11)
#define PAGE_ACCESS             (0x1ULL << 10)
#define PAGE_INNER_SHAREABLE    (0x3ULL << 8)
#define PAGE_PRIV_RW            (0x0ULL << 6)
#define PAGE_PRIV_RO            (0x2ULL << 6)
#define PAGE_USER_RW            (0x1ULL << 6)
#define PAGE_USER_RO            (0x3ULL << 6)
#define PAGE_PRIV_MASK          (0x3ULL << 6)
#define PAGE_NON_SECURE         (0x1ULL << 5)
#define PAGE_ATTR(x)            ((x) << 2)

#endif
