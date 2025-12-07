#include "osmutex.h"
#include "scheduler.h"
#include "ipi.h"
#include "clocks.h"

void Condition::Wait(kernel_time tout, int *signalled_ret)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    timeout to { tout, signalled_ret };
    waiting_threads.insert_or_assign(t->id, to);
    t->blocking.block(this, tout);
    Yield();
}

Condition::~Condition()
{
    // wake up all waiting threads
    CriticalGuard cg(sl, ThreadList.sl);
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

    if(signal_all)
    {
        for(auto &bt : waiting_threads)
        {
            bool timedout = false;
            if(kernel_time_is_valid(bt.second.tout) &&
                tnow > bt.second.tout)
            {
                timedout = 1;
            }

            if(!timedout)
            {
                if(bt.second.signalled)
                    *bt.second.signalled = true;
                
                auto pwt = ThreadList._get(bt.first);
                if(pwt)
                {
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
            }

            if(!timedout)
            {
                if(tp.signalled)
                    *tp.signalled = true;
                
                auto pwt = ThreadList._get(iter->first);
                if(pwt)
                {
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
