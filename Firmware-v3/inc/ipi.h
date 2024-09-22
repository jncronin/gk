#ifndef IPI_H
#define IPI_H

#include "osmutex.h"
#include "osringbuffer.h"
#include "gk_conf.h"
#include "scheduler.h"

static inline void signal_thread_woken(Thread *t)
{
    if(t->base_priority >= GetCurrentThreadForCore()->base_priority)
        Yield();
}

#endif
