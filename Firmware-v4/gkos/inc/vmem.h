#ifndef VMEM_H
#define VMEM_H

#include "gkos_vmem.h"
#include "ostypes.h"

#include <cstddef>

#define PMEM_TO_VMEM(a) (((uintptr_t)(a) + UH_START))
#define VMEM_TO_PMEM(a) (((uintptr_t)(a) - UH_START))

int vmem_map(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec, uintptr_t ttbr0 = ~0ULL,
    uintptr_t ttbr1 = ~0ULL, uintptr_t *paddr_out = nullptr, unsigned int mt = MT_NORMAL);
int vmem_map(const VMemBlock &vaddr, const PMemBlock &paddr, uintptr_t ttbr0 = ~0ULL, uintptr_t ttbr1 = ~0ULL);
int vmem_unmap(const VMemBlock &vaddr, uintptr_t ttbr0 = ~0ULL, uintptr_t ttbr1 = ~0ULL);
void vmem_invlpg(uintptr_t vaddr, uintptr_t ttbr);
uintptr_t vmem_vaddr_to_paddr(uintptr_t vaddr, uintptr_t ttbr0 = ~0ULL, uintptr_t ttbr1 = ~0ULL);

static inline uintptr_t vmem_vaddr_to_paddr_quick(uintptr_t vaddr)
{
    uint64_t daif;
    uint64_t paddr;

    __asm__ volatile(
        "mrs %[daif], daif\n"
        "msr daifset, 0xf\n"
        "at s1e1r, %[vaddr]\n"
        "mrs %[paddr], par_el1\n"
        "msr daif, %[daif]\n"
        :
        [daif] "=r" (daif),
        [paddr] "=r" (paddr)
        :
        [vaddr] "r" (vaddr)
        :
        "memory"
    );
    if(paddr & 0x1)
        return 0;
    else
        return (paddr & 0xffffffff0000ULL) | (vaddr & 0xffffULL);
}

/* Quick clear/copy, assumes multiples of 64 bytes, 16 byte alignment */
extern "C" void quick_clear_64(void *dest, size_t n = 65536);
extern "C" void quick_copy_64(void *dest, const void *src, size_t n = 65536);

#endif
