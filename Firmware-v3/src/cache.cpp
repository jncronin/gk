#include "cache.h"
#include "osnet.h"
#include "thread.h"
#include "gk_conf.h"
#include "ipi.h"
#include "clocks.h"

#define DEBUG_CACHE     1

#if DEBUG_CACHE
static inline void check_ptr(uint32_t base, uint32_t length)
{
#if 1
    if(base & 0x1fU || (base + length) & 0x1fU)
    {
        __asm__ volatile("bkpt \n" ::: "memory");
    }
#endif
}
#endif

static inline SimpleSignal *get_ss()
{
    return &GetCurrentThreadForCore()->ss;
}

void InvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype, bool override_sram_check)
{
#if GK_USE_CACHE
#if DEBUG_CACHE
    check_ptr(base, length);
#endif

    switch(ctype)
    {
        case CacheType_t::Instruction:
            SCB_InvalidateICache_by_Addr((void *)base, length);
            break;

        case CacheType_t::Data:
            SCB_InvalidateDCache_by_Addr((void *)base, length);
            break;

        case CacheType_t::Both:
            SCB_InvalidateDCache_by_Addr((void *)base, length);
            SCB_InvalidateICache_by_Addr((void *)base, length);
            break;
    }
#endif
}

void CleanM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
#if DEBUG_CACHE
    check_ptr(base, length);
#endif
    switch(ctype)
    {
        case CacheType_t::Instruction:
            break;

        case CacheType_t::Data:
            SCB_CleanDCache_by_Addr((uint32_t *)base, length);
            break;

        case CacheType_t::Both:
            SCB_CleanDCache_by_Addr((uint32_t *)base, length);
            break;
    }
#endif
}

void CleanOrInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
    // Called after write to cached memory.  If M7 wrote then clean cache, if M4 wrote then invalidate cache
    CleanM7Cache(base, length, ctype);
#endif
}

void CleanAndInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
#if DEBUG_CACHE
    check_ptr(base, length);
#endif
    switch(ctype)
    {
        case CacheType_t::Instruction:
            SCB_InvalidateICache_by_Addr((void *)base, length);
            break;

        case CacheType_t::Data:
            SCB_CleanInvalidateDCache_by_Addr((uint32_t *)base, length);
            break;

        case CacheType_t::Both:
            SCB_CleanInvalidateDCache_by_Addr((uint32_t *)base, length);
            SCB_InvalidateICache_by_Addr((void *)base, length);
            break;
    }
#endif
}
