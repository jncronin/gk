#ifndef VMEM_H
#define VMEM_H

#include "gkos_vmem.h"
#include "ostypes.h"
#include "logger.h"

#include <cstddef>

#define PMEM_TO_VMEM(a) (((uintptr_t)(a) + UH_START))
#define PMEM_TO_VMEM_DEVICE(a) (((uintptr_t)(a) + UH_DEVICE_START))
#define VMEM_TO_PMEM(a) (((uintptr_t)(a) - UH_START))

#define DEBUG_VTP  1

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

#if 1
    uint64_t _ttbr0;
    __asm__ volatile(
        "mrs %[_ttbr0], ttbr0_el1\n"
        : [_ttbr0] "=r" (_ttbr0) :: "memory");
    vmem_invlpg(vaddr, _ttbr0);
#endif

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
    {
#if DEBUG_VTP
        uint64_t ttbr0, ttbr1;
        __asm__ volatile("mrs %[ttbr0], ttbr0_el1\n"
            "mrs %[ttbr1], ttbr1_el1\n"
            : [ttbr0] "=r" (ttbr0), [ttbr1] "=r" (ttbr1) :: "memory");
        klog("vtp: failed for %llx: %llx, ttbr0 = %llx, ttbr1 = %llx\n", vaddr, paddr, ttbr0, ttbr1);

        auto slow_ret = vmem_vaddr_to_paddr(vaddr);
        klog("vtp: our version: %llx\n", slow_ret);
#endif
        return 0;
    }
    else
        return (paddr & 0xffffffff0000ULL) | (vaddr & 0xffffULL);
}

/* Quick clear/copy, assumes multiples of 64 bytes, 16 byte alignment */
extern "C" void quick_clear_64(void *dest, size_t n = 65536);
extern "C" void quick_copy_64(void *dest, const void *src, size_t n = 65536);

#endif
