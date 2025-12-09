#include "cache.h"
#include "logger.h"

#define DEBUG_UNALIGNED     0
#define ERROR_UNALIGNED     0

/* PoU = all cores see the same thing (i.e. as far as L2 cache)
    PoC = all cores and external agents (e.g. DMA) see the same thing (i.e. as far as main memory) */

void InvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma)
{
#if DEBUG_UNALIGNED || ERROR_UNALIGNED
    if((base & (CACHE_LINE_SIZE - 1)) || (length & (CACHE_LINE_SIZE - 1)))
    {
        klog("cache: inv: unaligned %llx:%llx\n", base, length);
#if ERROR_UNALIGNED
        __asm__ volatile("brk #249\n" ::: "memory");
#endif
    }
#endif
    auto end = base + length;
    end = (end + (CACHE_LINE_SIZE - 1)) & ~(CACHE_LINE_SIZE - 1);
    base &= ~(CACHE_LINE_SIZE - 1);
    length = end - base;

    switch(ctype)
    {
        case CacheType_t::Both:
            InvalidateA35Cache(base, length, CacheType_t::Data, for_dma);
            InvalidateA35Cache(base, length, CacheType_t::Instruction, for_dma);
            return;

        case CacheType_t::Data:
            while(length)
            {
                __asm__ volatile("dc ivac, %[base]\n" : : [base] "r" (base) : "memory");
                base += CACHE_LINE_SIZE;
                length -= CACHE_LINE_SIZE;
            }
            break;

        case CacheType_t::Instruction:
            while(length)
            {
                __asm__ volatile("ic ivau, %[base]\n" : : [base] "r" (base) : "memory");
                base += CACHE_LINE_SIZE;
                length -= CACHE_LINE_SIZE;
            }
            break;
    }        
}

void CleanA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma)
{
#if DEBUG_UNALIGNED || ERROR_UNALIGNED
    if((base & (CACHE_LINE_SIZE - 1)) || (length & (CACHE_LINE_SIZE - 1)))
    {
        klog("cache: clean: unaligned %llx:%llx\n", base, length);
#if ERROR_UNALIGNED
        __asm__ volatile("brk #250\n" ::: "memory");
#endif
    }
#endif
    auto end = base + length;
    end = (end + (CACHE_LINE_SIZE - 1)) & ~(CACHE_LINE_SIZE - 1);
    base &= ~(CACHE_LINE_SIZE - 1);
    length = end - base;

    switch(ctype)
    {
        case CacheType_t::Both:
        case CacheType_t::Data:
            if(for_dma)
            {
                while(length)
                {
                    __asm__ volatile("dc cvac, %[base]\n" : : [base] "r" (base) : "memory");
                    base += CACHE_LINE_SIZE;
                    length -= CACHE_LINE_SIZE;
                }
            }
            else
            {
                while(length)
                {
                    __asm__ volatile("dc cvau, %[base]\n" : : [base] "r" (base) : "memory");
                    base += CACHE_LINE_SIZE;
                    length -= CACHE_LINE_SIZE;
                }
            }
            break;

        case CacheType_t::Instruction:
            break;
    }
    __asm__ volatile("dsb sy\n" ::: "memory");
}
