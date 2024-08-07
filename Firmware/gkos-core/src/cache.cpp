#include "gkos.h"
#include "cache.h"
#include "thread.h"
#include "gk_conf.h"
#include "ipi.h"
#include "kernel_time.h"
#include "gkos.h"

#define DEBUG_CACHE     1

#if DEBUG_CACHE
static inline void GKOS_FUNC(check_ptr)(uint32_t base, uint32_t length)
{
#if 1
    if(base & 0x1fU || (base + length) & 0x1fU)
    {
        __asm__ volatile("bkpt \n" ::: "memory");
    }
#endif
}
#endif

static SimpleSignal *GKOS_FUNC(get_ss)()
{
    return &GetCurrentThreadForCore()->ss;
}

static inline void GKOS_FUNC(wait_completion)(SimpleSignal *ss, IpiRingBuffer *rb)
{
    while(!ss->Wait(SimpleSignal::Set, 0U, clock_cur() + kernel_time::from_ms(2)))
    {
        if(rb->empty())
        {
            {
                CriticalGuard cg(s_rtt);
                klog("cache: M7 didn't signal completion but ring buffer empty - potentially a bug\n");
            }
            return;
        }
    }
}

void GKOS_FUNC(InvalidateM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype, bool override_sram_check)
{
#if GK_USE_CACHE
    if(!override_sram_check && base >= 0x30000000 && base < 0x40000000) return;
#if DEBUG_CACHE
    GKOS_FUNC(check_ptr)(base, length);
#endif

#if IS_GENERIC
    if(GKOS_FUNC(GetCoreID)() == 0)
#endif
    {
#if !IS_CM4_ONLY
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
#if IS_GENERIC
    else
#endif
    {
#if !IS_CM7_ONLY
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
#endif
    }
#endif
}

void GKOS_FUNC(CleanM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
    if(base >= 0x30000000 && base < 0x40000000) return;
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

void GKOS_FUNC(CleanOrInvalidateM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
    if(base >= 0x30000000 && base < 0x40000000) return;
    // Called after write to cached memory.  If M7 wrote then clean cache, if M4 wrote then invalidate cache
    if(GetCoreID() == 0)
        CleanM7Cache(base, length, ctype);
    else
        InvalidateM7Cache(base, length, ctype);
#endif
}

void GKOS_FUNC(CleanAndInvalidateM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
    if(base >= 0x30000000 && base < 0x40000000) return;
#if DEBUG_CACHE
    GKOS_FUNC(check_ptr)(base, length);
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
