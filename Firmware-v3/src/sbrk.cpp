#include <memblk.h>
#include "gk_conf.h"

extern "C" void * _sbrk(int n)
{
    static MemRegion mr = InvalidMemregion();
    static size_t mr_top = 0;

    if(!mr.valid)
    {
        extern void init_memblk();
        init_memblk();

        mr = memblk_allocate(GK_KHEAP_SIZE, MemRegionType::AXISRAM, GK_MEMBLK_USAGE_KERNEL_HEAP);
        if(!mr.valid)
        {
            __asm__ volatile("bkpt \n" ::: "memory");
            while(true);
        }
    }

    auto ret = (void *)(mr.address + mr_top);
    if(n == 0)
    {
        return ret;
    }
    else if(n > 0)
    {
        auto free_space = mr.length - mr_top;
        if((size_t)n > free_space)
        {
            return (void *)-1;
        }
        mr_top += n;
        return ret;
    }
    else
    {
        auto to_reduce = (size_t)(-n);
        if(to_reduce > mr_top)
        {
            return (void *)-1;
        }
        mr_top -= to_reduce;
        return ret;
    }
}
