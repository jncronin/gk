#include "vblock.h"
#include "osspinlock.h"

Spinlock sl_vblock;

extern uint64_t vaddr_ptr;

static inline uint64_t align(uint64_t v)
{
    return (v + 65535ULL) & ~65535ULL;
}

uint64_t vmem_alloc(uint64_t len)
{
    CriticalGuard cg(sl_vblock);
    len = align(len);
    auto avail = 0x40000000ULL - vaddr_ptr;
    if(avail < len)
    {
        while(true);
    }
    auto ret = vaddr_ptr;
    vaddr_ptr += len;
    return ret;
}
