#ifndef IPI_H
#define IPI_H

#include "osmutex.h"
#include "osringbuffer.h"
#include "gk_conf.h"
#include "scheduler.h"

/* Interface for passing messages between cores using SEV interrupts */
struct ipi_message
{
    enum ipi_message_type_t { M7DataCacheClean, M7DataCacheInv, M7DataCacheCleanInv, M7InstCacheInv,
        M4Wakeup,
        ThreadUnblocked,
        Yield };
    ipi_message_type_t type;
    
    SimpleSignal *ss;

    union
    {
        struct
        {
            uint32_t base_addr;
            uint32_t len;
        } cache_req;

        Thread *t;
    };
};

constexpr const unsigned int n_ipi_messages = 32;
using IpiRingBuffer = RingBuffer<ipi_message, n_ipi_messages>;
extern IpiRingBuffer ipi_messages[2];
#define M4_MAGIC 0xa1b2c3d4

static inline void signal_thread_woken(Thread *t)
{
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    auto other_core = 1U - GetCoreID();
    ipi_messages[other_core].Write({ ipi_message::ThreadUnblocked, nullptr, .t = t });
    __SEV();
#endif
}

#endif
