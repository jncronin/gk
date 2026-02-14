#include "osmutex.h"
#include "thread.h"
#include "process.h"
#include "osspinlock.h"

Barrier::Barrier(unsigned int _nreq)
{
    nrequired = _nreq;
}

int Barrier::Wait()
{
    CriticalGuard cg(sl, ThreadList.sl);

    ncur++;
    if(ncur >= nrequired)
    {
        for(auto wt : waiting_threads)
        {
            auto pwt = ThreadList._get(wt).v;
            if(pwt)
            {
                pwt->blocking.unblock();
                signal_thread_woken(pwt);
            }
        }

        // reset
        waiting_threads.clear();
        ncur = 0;

        return 1;       // return PTHREAD_BARRIER_SIGNAL_THREAD only in 1 thread
    }
    else
    {
        auto t = GetCurrentThreadForCore();
        waiting_threads.push_back(t->id);
        t->blocking.block(this);
        cg.unlock();
        Yield();

        // we have been unlocked by another thread calling Wait();
        return 0;
    }
}

Barrier::~Barrier()
{
    for(auto wt : waiting_threads)
    {
        auto pwt = ThreadList.Get(wt).v;
        if(pwt)
        {
            pwt->blocking.unblock();
            signal_thread_woken(pwt);
        }
    }
}
