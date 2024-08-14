#define USE_MY_PREFIX ECM
#define MORECORE ECMsbrk

extern "C" void *ECMsbrk(int n);

#include "osallocator.h"
#include "memblk.h"
#include "dlmalloc.h"

template <> void region_free<0>(void *ptr)
{
    ECMfree(ptr);
}

template <> void *region_alloc<0>(size_t size)
{
    return ECMmalloc(size);
}

void *ECMsbrk(int n)
{
    static MemRegion mr = InvalidMemregion();
    static size_t mr_top = 0;

    if(!mr.valid)
    {
        mr = memblk_allocate(8192, MemRegionType::SRAM, "ext4_cache");
        if(!mr.valid)
            mr = memblk_allocate(8192, MemRegionType::SDRAM, "ext4_cache");
        if(!mr.valid)
            mr = memblk_allocate(8192, MemRegionType::AXISRAM, "ext4_cache");
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
