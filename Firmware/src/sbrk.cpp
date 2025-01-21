#include <memblk.h>
#include "gk_conf.h"

extern int _kheap_start, _kheap_end;
static uint32_t _khsp = (uint32_t)(uintptr_t)&_kheap_start;
static uint32_t _khep = (uint32_t)(uintptr_t)&_kheap_end;

extern "C" void * _sbrk(int n)
{
    static size_t mr_top = 0;

    auto ret = (void *)(_khsp + mr_top);
    if(n == 0)
    {
        return ret;
    }
    else if(n > 0)
    {
        auto free_space = (_khep - _khsp) - mr_top;
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
