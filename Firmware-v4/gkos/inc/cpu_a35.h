#ifndef CPU_A35_H
#define CPU_A35_H

#include <cstdint>
#include "logger.h"
#include "vmem.h"

struct backtrace_start
{
    uint64_t fp, lr;
};

static inline __attribute__((always_inline)) void backtrace(const backtrace_start &bstart)
{
    uint64_t lr = bstart.lr;
    uint64_t fp = bstart.fp;

    int level = 1;
    klog("BACKTRACE: %3d: %16llx\n", 0, lr);
    while(true)
    {
        if(!vmem_vaddr_to_paddr_quick(fp) || !vmem_vaddr_to_paddr_quick(fp + 15))
            break;  // cannot read from fp/fp+8
        if(!vmem_vaddr_to_paddr_quick(*(uintptr_t *)(fp + 8)) || !vmem_vaddr_to_paddr_quick(*(uintptr_t *)(fp + 8) + 3))
            break;  // cannot read from lr
        klog("BACKTRACE: %3d: %16llx\n", level, *(uint64_t *)(fp + 8));
        auto new_fp = *(uint64_t *)fp;
        if(new_fp == fp) break;
        fp = new_fp;
        level++;
    }
}

static inline __attribute__((always_inline)) void backtrace()
{
    backtrace_start bs;
    __asm__ volatile(
        "mov %[lr], lr\n"
        "mov %[fp], fp\n"
        : [lr] "=r" (bs.lr), [fp] "=r" (bs.fp) :: "memory"
    );
    backtrace(bs);
}

#endif
