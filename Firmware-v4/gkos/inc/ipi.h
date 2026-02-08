#ifndef IPI_H
#define IPI_H

#include "osmutex.h"
//#include "osringbuffer.h"
#include "gk_conf.h"
#include "scheduler.h"

static inline void signal_thread_woken(const PThread &t)
{
    // other cores first
    auto core_id = GetCoreID();
    CriticalGuard cg(sched.sl_cur_next);
    for(auto i = 0U; i < sched.ncores; i++)
    {
        if(i == core_id)
            continue;
        
        if(sched.golden_thread[i] == nullptr && sched.current_thread[i] && t->base_priority >= sched.current_thread[i]->base_priority)
        {
            gic_send_sgi(GIC_SGI_YIELD, i);
        }
    }

    if(sched.golden_thread[core_id] == nullptr && t->base_priority >= GetCurrentThreadForCore()->base_priority)
        Yield();
}

#endif
