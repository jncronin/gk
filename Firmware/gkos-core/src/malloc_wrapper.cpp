#include "gkos.h"

/* malloc interface */
#include "osmutex.h"
#include "region_allocator.h"
#include "memblk.h"
__attribute__((section(".sram4"))) static Spinlock sl_sram4;

extern "C"
{
    void *dlmalloc(size_t n);
    void dlfree(void *p);
    void *dlcalloc(size_t nmemb, size_t size);
    void *dlrealloc(void *ptr, size_t size);
}

void *malloc_region(size_t n, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                return dlmalloc(n);
            }

        default:
            return nullptr;
    }
}

void free_region(void *p, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                dlfree(p);
            }

        default:
            return;
    }
}

void *realloc_region(void *ptr, size_t n, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                return dlrealloc(ptr, n);
            }

        default:
            return nullptr;
    }
}

void *calloc_region(size_t nmemb, size_t size, int reg_id)
{
    switch(reg_id)
    {
        case REG_ID_SRAM4:
            {
                CriticalGuard cg(sl_sram4);
                return dlcalloc(nmemb, size);
            }

        default:
            return nullptr;
    }
}

/* sbrk implementation for sram */
extern "C" void *sbrksram4(int n)
{
    static MemRegion mr = InvalidMemregion();
    static size_t mr_top = 0;

    if(!mr.valid)
    {
        extern void init_memblk();
        init_memblk();

        mr = memblk_allocate(128*1024, MemRegionType::SRAM);
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
