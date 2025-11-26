#include "syscalls_int.h"
#include "process.h"

intptr_t syscall_sbrk(intptr_t incr, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->heap.sl);
    if(!p->heap.vb_heap.valid)
    {
        // try allocating a vblock
        CriticalGuard cg2(p->user_mem->sl);
        auto vb_heap = vblock_alloc(VBLOCK_512M, true, true, false, 0, 0, p->user_mem->blocks);
        if(!vb_heap.valid)
            vb_heap = vblock_alloc(VBLOCK_4M, true, true, false, 0, 0, p->user_mem->blocks);
        if(!vb_heap.valid)
            vb_heap = vblock_alloc(VBLOCK_64k, true, true, false, 0, 0, p->user_mem->blocks);
        if(!vb_heap.valid)
        {
            klog("sbrk: could not allocate vblock\n");
            *_errno = ENOMEM;
            return -1;
        }
        p->heap.vb_heap = vb_heap;
    }

    auto ret = p->heap.vb_heap.data_start() + p->heap.brk;

    if(incr == 0)
    {
        return ret;
    }

    if(incr > 0)
    {
        auto uincr = (uintptr_t)incr;
        auto max_incr = p->heap.vb_heap.data_length() - p->heap.brk;
        if(uincr > max_incr)
        {
            *_errno = ENOMEM;
            return -1;
        }
        p->heap.brk += uincr;
        return ret;
    }
    else if(incr == INTPTR_MIN)
    {
        // can't negate
        *_errno = ENOMEM;
        return -1;
    }
    else
    {
        // incr < 0
        auto udecr = (uintptr_t)-incr;

        if(udecr > p->heap.brk)
        {
            *_errno = ENOMEM;
            return -1;
        }
        p->heap.brk -= udecr;
        return ret;
    }
}
