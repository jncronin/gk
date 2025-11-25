#ifndef IPI_H
#define IPI_H

#include "osmutex.h"
//#include "osringbuffer.h"
#include "gk_conf.h"
#include "scheduler.h"

static inline void signal_thread_woken(PThread t)
{
    // other cores first
    for(auto i = 0U; i < sched.ncores; i++)
    {
        if(i == GetCoreID())
            continue;
        
        {
            CriticalGuard cg(sched.sl_cur_next);
            if(t->base_priority > sched.current_thread[i]->base_priority)
            {
                gic_send_sgi(GIC_SGI_YIELD, i);
            }
        }
    }
    if(t->base_priority > GetCurrentThreadForCore()->base_priority)
        Yield();
}

#endif
