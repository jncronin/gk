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
#if 0
    if(base & 0x1fU || (base + length) & 0x1fU)
    {
        __asm__ volatile("bkpt \n" ::: "memory");
    }
#endif
}
#endif

static SimpleSignal *get_ss()
{
    return &GetCurrentThreadForCore()->ss;
}

static inline void wait_completion(SimpleSignal *ss, IpiRingBuffer *rb)
{
    while(!ss->Wait(SimpleSignal::Set, 0U, clock_cur() + kernel_time::from_ms(2)))
    {
        if(rb->empty())
        {
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "cache: M7 didn't signal completion but ring buffer empty - potentially a bug\n");
            }
            return;
        }
    }
}

void InvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
#if DEBUG_CACHE
    check_ptr(base, length);
#endif

    if(GetCoreID() == 0)
    {
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
    }
    else
    {
        switch(ctype)
        {
            case CacheType_t::Instruction:
                {
                    ipi_messages[0].Write({ ipi_message::M7InstCacheInv, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;

            case CacheType_t::Data:
                {
                    ipi_messages[0].Write({ ipi_message::M7DataCacheInv, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;

            case CacheType_t::Both:
                {
                    ipi_messages[0].Write({ ipi_message::M7InstCacheInv, nullptr, .cache_req = { base, length } }, true);
                    ipi_messages[0].Write({ ipi_message::M7DataCacheInv, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;
        }
    }
#endif
}

void CleanM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
#if DEBUG_CACHE
    check_ptr(base, length);
#endif
    if(GetCoreID() == 0)
    {
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
    }
    else
    {
        switch(ctype)
        {
            case CacheType_t::Instruction:
                break;

            case CacheType_t::Data:
                {
                    ipi_messages[0].Write({ ipi_message::M7DataCacheClean, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;

            case CacheType_t::Both:
                {
                    ipi_messages[0].Write({ ipi_message::M7DataCacheClean, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;
        }
    }
#endif
}

void CleanOrInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
    // Called after write to cached memory.  If M7 wrote then clean cache, if M4 wrote then invalidate cache
    if(GetCoreID() == 0)
        CleanM7Cache(base, length, ctype);
    else
        InvalidateM7Cache(base, length, ctype);
}

void CleanAndInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
#if DEBUG_CACHE
    check_ptr(base, length);
#endif
    if(GetCoreID() == 0)
    {
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
    }
    else
    {
        switch(ctype)
        {
            case CacheType_t::Instruction:
                {
                    ipi_messages[0].Write({ ipi_message::M7InstCacheInv, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;

            case CacheType_t::Data:
                {
                    ipi_messages[0].Write({ ipi_message::M7DataCacheCleanInv, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;

            case CacheType_t::Both:
                {
                    ipi_messages[0].Write({ ipi_message::M7InstCacheInv, nullptr, .cache_req = { base, length } }, true);
                    ipi_messages[0].Write({ ipi_message::M7DataCacheCleanInv, get_ss(), .cache_req = { base, length } }, true);
                    __SEV();
                    wait_completion(get_ss(), &ipi_messages[0]);
                }
                break;
        }
    }
#endif
}
