#include "ossharedmem.h"
#include "cache.h"
#include "gk_conf.h"

SharedMemoryGuard::SharedMemoryGuard(const void *_start, size_t _len, bool will_read, bool will_write)
{
#if GK_USE_CACHE
#if GK_DUAL_CORE
    // if nothing is going to happen (used, e.g. in wifi rxtx command for either rx or tx), do nothing
    if(!_start || !_len || (!will_read && !will_write))
    {
        t = nullptr;
        return;
    }

    auto curt = GetCurrentThreadForCore();
    {
        CriticalGuard cg(curt->sl);
        t = curt;
        old_core_pin = curt->tss.pinned_on_core;
        coreid = GetCoreID();
        curt->tss.pinned_on_core = coreid;
    }

    start = (uint32_t)(uintptr_t)_start;
    len = _len;
    is_read = will_read;
    is_write = will_write;

    if(will_read && coreid == 0)
    {
        /* if we are reading from the memory, and we are core 1, and we are unaligned, we need to
            save what is in the cache _to_ memory first, and then invalidate */
        if((start & 0x1f) || (_len & 0x1f))
        {
            SCB_CleanDCache_by_Addr((uint32_t *)_start, _len);
        }

        SCB_InvalidateDCache_by_Addr((uint32_t *)_start, _len);
    }
    if(coreid == 1)
    {
        // Signal the M7 to clean its cache prior to either a write or read from the M4
        CleanM7Cache(start, len, CacheType_t::Data);
    }
#endif
#endif
}

SharedMemoryGuard::~SharedMemoryGuard()
{
#if GK_USE_CACHE
#if GK_DUAL_CORE
    if(!t)
        return;
    if(coreid == 0 && is_write)
    {
        SCB_CleanDCache_by_Addr((uint32_t *)start, len);
    }

    {
        CriticalGuard cg(t->sl);
        t->tss.pinned_on_core = old_core_pin;
    }
#endif
#endif
}