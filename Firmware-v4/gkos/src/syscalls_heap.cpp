#include "syscalls_int.h"
#include "process.h"

intptr_t syscall_sbrk(intptr_t incr, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->heap.sl);
    if(!p->heap.vb_heap.valid)
    {
        // try allocating a vblock
        MutexGuard mg(p->user_mem->m);
        for(auto heap_size = 2ULL*1024*1024*1024; heap_size >= VBLOCK_64k; heap_size /= 2)
        {
            p->heap.vb_heap = p->user_mem->vblocks.AllocAny(
                MemBlock::ZeroBackedReadWriteMemory(0, heap_size, true, false), false);
            if(p->heap.vb_heap.valid)
                break;
        }

        if(!p->heap.vb_heap.valid)
        {
            klog("sbrk: could not allocate vblock\n");
            *_errno = ENOMEM;
            return -1;
        }
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
