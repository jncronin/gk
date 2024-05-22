#include <stm32h7xx.h>
#include "ipi.h"
#include "scheduler.h"

SRAM4_DATA RingBuffer<ipi_message, n_ipi_messages> ipi_messages[2];
SRAM4_DATA volatile uint32_t m4_wakeup = 0;

#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
static inline void handle_thread_unblocked(int coreid, Thread *t)
{
    auto taff = t->tss.affinity;
    if((unsigned int)taff & (1U << coreid))
    {
        auto curt = GetCurrentThreadForCore(coreid);
        if(t->base_priority > curt->base_priority)
            Yield();
    }
}
#endif

extern "C" void CM4_SEV_IRQHandler()
{
    // This is a signal from M4 to M7
    ipi_message msg;
    while(ipi_messages[0].Read(&msg))
    {
        switch(msg.type)
        {
#if GK_USE_CACHE
            case ipi_message::M7DataCacheClean:
                SCB_CleanDCache_by_Addr((uint32_t *)msg.cache_req.base_addr, msg.cache_req.len);
                break;
            case ipi_message::M7DataCacheCleanInv:
                SCB_CleanInvalidateDCache_by_Addr((uint32_t *)msg.cache_req.base_addr, msg.cache_req.len);
                break;
            case ipi_message::M7DataCacheInv:
                SCB_InvalidateDCache_by_Addr((void *)msg.cache_req.base_addr, msg.cache_req.len);
                break;
            case ipi_message::M7InstCacheInv:
                SCB_InvalidateICache_by_Addr((void *)msg.cache_req.base_addr, msg.cache_req.len);
                break;
#endif
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
            case ipi_message::ThreadUnblocked:
                handle_thread_unblocked(0, msg.t);
                break;
#endif
            default:
                break;
        }
        if(msg.ss)
            msg.ss->Signal();
    }
}

extern "C" void CM7_SEV_IRQHandler()
{
    // this is a message from CM7 to CM4
    ipi_message msg;
    while(ipi_messages[1].Read(&msg))
    {
        switch(msg.type)
        {
            case ipi_message::M4Wakeup:
                m4_wakeup = M4_MAGIC;
                break;

#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
            case ipi_message::ThreadUnblocked:
                handle_thread_unblocked(1, msg.t);
                break;
#endif

            default:
                break;
        }
        if(msg.ss)
            msg.ss->Signal();
    }
}
