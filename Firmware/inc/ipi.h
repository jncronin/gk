#ifndef IPI_H
#define IPI_H

#include "osmutex.h"
#include "osringbuffer.h"
#include "thread.h"
#include "gk_conf.h"

/* Interface for passing messages between cores using SEV interrupts */
struct ipi_message
{
    enum ipi_message_type_t { M7DataCacheClean, M7DataCacheInv, M7DataCacheCleanInv, M7InstCacheInv,
        M4Wakeup,
        ThreadUnblocked };
    ipi_message_type_t type;
    volatile bool *complete;

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
extern RingBuffer<ipi_message, n_ipi_messages> ipi_messages[2];
#define M4_MAGIC 0xa1b2c3d4


#endif
