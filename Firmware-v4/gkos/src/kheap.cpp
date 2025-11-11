#include "kheap.h"
#include "osmutex.h"
#include "vblock.h"

// TODO: replace with a mutex
static Spinlock sl_heap;

static BuddyEntry be_heap { 0, 0, false };
static uintptr_t sbrk_end;

void init_kheap()
{
    be_heap = vblock_alloc(VBLOCK_512M, VBLOCK_TAG_WRITE);
    if(!be_heap.valid)
    {
        klog("kheap: failed to allocate\n");
        while(true);
    }

    sbrk_end = be_heap.base;
}

extern "C"
{

void *_sbrk(intptr_t increment)
{
    // should already have lock from surrounding malloc call

    if(!be_heap.valid)
    {
        klog("kheap: sbrk called before init_kheap\n");
        while(true);
    }
    
    if(increment == 0)
        return (void *)sbrk_end;
    
    if(increment > 0)
    {
        auto ui = (uintptr_t)increment;
        if(((be_heap.base + be_heap.length) - sbrk_end) < ui)
        {
            return (void *)-1;
        }
        auto ret = sbrk_end;
        sbrk_end += ui;
        return (void *)ret;
    }
    else
    {
        auto ud = (uintptr_t)-increment;
        if((sbrk_end - be_heap.base) < ud)
        {
            return (void *)-1;
        }
        auto ret = sbrk_end;
        sbrk_end -= ud;
        return (void *)ret;
    }
}

void *__real_malloc(size_t);
void __real_free(void *);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void *__real_reallocarray(void *, size_t, size_t);

void *__wrap_malloc(size_t size)
{
    CriticalGuard cg(sl_heap);
    return __real_malloc(size);
}

void __wrap_free(void *p)
{
    CriticalGuard cg(sl_heap);
    __real_free(p);
}

void *__wrap_realloc(void *p, size_t size)
{
    CriticalGuard cg(sl_heap);
    return __real_realloc(p, size);
}

void *__wrap_calloc(size_t n, size_t size)
{
    CriticalGuard cg(sl_heap);
    return __real_calloc(n, size);
}

void *__wrap_reallocarray(void *p, size_t n, size_t size)
{
    CriticalGuard cg(sl_heap);
    return __real_reallocarray(p, n, size);
}

}