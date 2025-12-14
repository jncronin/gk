#include "osmutex.h"
#include "scheduler.h"
#include "ipi.h"
#include "clocks.h"

#define DEBUG_COND 0

void Condition::Wait(kernel_time tout, int *signalled_ret)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    timeout to { tout, signalled_ret };
    waiting_threads.insert_or_assign(t->id, to);
#if DEBUG_COND
    klog("cond: thread %s (%u) waiting on %u (%p), tout: %ld.%ld\n", t->name.c_str(), t->id, id, this,
        tout.tv_sec, tout.tv_nsec);
#endif
    t->blocking.block(this, tout);
    Yield();
}

Condition::~Condition()
{
    // wake up all waiting threads
    CriticalGuard cg(sl, ThreadList.sl);
#if DEBUG_COND
    klog("cond: %u (%p) destructor\n", id, this);
#endif

    for(auto &bt : waiting_threads)
    {
        //CriticalGuard cg2(bt.first->sl);
        auto pwt = ThreadList._get(bt.first);
        if(pwt)
        {
            pwt->blocking.unblock();
            signal_thread_woken(pwt);
        }
    }
}

void Condition::Signal(bool signal_all)
{
    CriticalGuard cg(sl, ThreadList.sl);
    auto tnow = clock_cur();

#if DEBUG_COND
    klog("cond: %u (%p) signalled%s\n", id, this, signal_all ? " (signal all)" : "");
#endif


    if(signal_all)
    {
        for(auto &bt : waiting_threads)
        {
            bool timedout = false;
            if(kernel_time_is_valid(bt.second.tout) &&
                tnow > bt.second.tout)
            {
                timedout = 1;
#if DEBUG_COND
                klog("cond: %u (%p): waiting thread %u timed out\n", id, this,
                    bt.first);
#endif

            }

            if(!timedout)
            {
                if(bt.second.signalled)
                    *bt.second.signalled = true;

#if DEBUG_COND
                klog("cond: %u (%p): waiting thread %u to be signalled\n", id, this,
                    bt.first);
#endif

                auto pwt = ThreadList._get(bt.first);
                if(pwt)
                {
#if DEBUG_COND
                    klog("cond: %u (%p): waiting thread %u (%s) signalled\n", id, this,
                        bt.first, pwt->name.c_str());
#endif

                    pwt->blocking.unblock();
                    signal_thread_woken(pwt);
                }
            }
        }
        waiting_threads.clear();
    }
    else
    {
        // need to keep looking for a single thread to wake
        auto iter = waiting_threads.begin();
        while(iter != waiting_threads.end())
        {
            bool timedout = false;
            auto &tp = iter->second;
            if(kernel_time_is_valid(tp.tout) &&
                tnow > tp.tout)
            {
                timedout = 1;
#if DEBUG_COND
                klog("cond: %u (%p): waiting thread %u timed out\n", id, this,
                    iter->first);
#endif
            }

            if(!timedout)
            {
                if(tp.signalled)
                    *tp.signalled = true;

#if DEBUG_COND
                klog("cond: %u (%p): waiting thread %u to be signalled\n", id, this,
                    iter->first);
#endif
                
                auto pwt = ThreadList._get(iter->first);
                if(pwt)
                {
#if DEBUG_COND
                    klog("cond: %u (%p): waiting thread %u (%s) signalled\n", id, this,
                        iter->first, pwt->name.c_str());
#endif

                    pwt->blocking.unblock();
                    signal_thread_woken(pwt);
                    break;
                }
                else
                {
                    iter = waiting_threads.erase(iter);
                }
            }
            else
            {
                iter = waiting_threads.erase(iter);
            }
        }
    }
}
