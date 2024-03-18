#include "cache.h"
#include "osnet.h"
#include "thread.h"

struct cache_req
{
    uint32_t base_addr;
    uint32_t len;
};

SRAM4_DATA static RingBuffer<cache_req, 8> data_cache_inv_req;
SRAM4_DATA static RingBuffer<cache_req, 8> inst_cache_inv_req;
SRAM4_DATA static RingBuffer<cache_req, 8> data_cache_clean_req;

/* M4 asking M7 to clean cache may mean the M4 needs the data in in,
    therefore need to wait until the cleaning is done in order to proceed */
SRAM4_DATA static Spinlock m4_await_m7_sl;
SRAM4_DATA static volatile bool m4_await_m7_completion;

void InvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
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
                inst_cache_inv_req.Write({ base, length });
                __SEV();
                break;

            case CacheType_t::Data:
                data_cache_inv_req.Write({ base, length });
                __SEV();
                break;

            case CacheType_t::Both:
                inst_cache_inv_req.Write({ base, length });
                data_cache_inv_req.Write({ base, length });
                __SEV();
                break;
        }
    }
}

void CleanM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
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
                    CriticalGuard cg(m4_await_m7_sl);
                    m4_await_m7_completion = false;
                    data_cache_clean_req.Write({ base, length });
                    __SEV();
                    while(!m4_await_m7_completion);
                }
                break;

            case CacheType_t::Both:
                {
                    CriticalGuard cg(m4_await_m7_sl);
                    m4_await_m7_completion = false;
                    data_cache_clean_req.Write({ base, length });
                    __SEV();
                    while(!m4_await_m7_completion);
                }
                break;
        }
    }
}

void CleanInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype)
{
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
                inst_cache_inv_req.Write({ base, length });
                __SEV();
                break;

            case CacheType_t::Data:
                {
                    CriticalGuard cg(m4_await_m7_sl);
                    m4_await_m7_completion = false;
                    data_cache_inv_req.Write({ base, length });
                    data_cache_clean_req.Write({ base, length });
                    __SEV();
                    while(!m4_await_m7_completion);
                }
                break;

            case CacheType_t::Both:
                {
                    CriticalGuard cg(m4_await_m7_sl);
                    m4_await_m7_completion = false;
                    inst_cache_inv_req.Write({ base, length });
                    data_cache_inv_req.Write({ base, length });
                    data_cache_clean_req.Write({ base, length });
                    __SEV();
                    while(!m4_await_m7_completion);
                }
                break;
        }
    }
}

extern "C" void CM4_SEV_IRQHandler()
{
    // This is a signal from M4 that we need to do something to the cache
    cache_req cr;
    while(data_cache_inv_req.Read(&cr))
    {
        SCB_InvalidateDCache_by_Addr((void *)cr.base_addr, cr.len);
    }
    while(inst_cache_inv_req.Read(&cr))
    {
        SCB_InvalidateICache_by_Addr((void *)cr.base_addr, cr.len);
    }
    while(data_cache_clean_req.Read(&cr))
    {
        SCB_CleanDCache_by_Addr((uint32_t *)cr.base_addr, cr.len);
    }
    m4_await_m7_completion = true;
}
