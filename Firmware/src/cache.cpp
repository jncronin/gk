#include "cache.h"
#include "osnet.h"
#include "thread.h"
#include "gk_conf.h"
#include "ipi.h"

void InvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
#if GK_USE_CACHE
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
                ipi_messages[0].Write({ ipi_message::M7InstCacheInv, nullptr, .cache_req = { base, length } });
                __SEV();
                break;

            case CacheType_t::Data:
                ipi_messages[0].Write({ ipi_message::M7DataCacheInv, nullptr, .cache_req = { base, length } });
                __SEV();
                break;

            case CacheType_t::Both:
                ipi_messages[0].Write({ ipi_message::M7InstCacheInv, nullptr, .cache_req = { base, length } });
                ipi_messages[0].Write({ ipi_message::M7DataCacheInv, nullptr, .cache_req = { base, length } });
                __SEV();
                break;
        }
    }
#endif
}

void CleanM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
    volatile bool m4_await_m7_completion = false;

#if GK_USE_CACHE
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
                ipi_messages[0].Write({ ipi_message::M7DataCacheClean, &m4_await_m7_completion, .cache_req = { base, length } });
                __SEV();
                while(!m4_await_m7_completion);
                break;

            case CacheType_t::Both:
                ipi_messages[0].Write({ ipi_message::M7DataCacheClean, &m4_await_m7_completion, .cache_req = { base, length } });
                __SEV();
                while(!m4_await_m7_completion);
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
    volatile bool m4_await_m7_completion = false;

#if GK_USE_CACHE
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
                ipi_messages[0].Write({ ipi_message::M7InstCacheInv, nullptr, .cache_req = { base, length } });
                __SEV();
                break;

            case CacheType_t::Data:
                ipi_messages[0].Write({ ipi_message::M7DataCacheCleanInv, &m4_await_m7_completion, .cache_req = { base, length } });
                __SEV();
                while(!m4_await_m7_completion);
                break;

            case CacheType_t::Both:
                ipi_messages[0].Write({ ipi_message::M7InstCacheInv, nullptr, .cache_req = { base, length } });
                ipi_messages[0].Write({ ipi_message::M7DataCacheCleanInv, &m4_await_m7_completion, .cache_req = { base, length } });
                __SEV();
                while(!m4_await_m7_completion);
                break;
        }
    }
#endif
}
