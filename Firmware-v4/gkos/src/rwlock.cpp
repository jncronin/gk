#include "osmutex.h"
#include "scheduler.h"
#include "ipi.h"

bool RwLock::try_rdlock(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    auto twrowner = wrowner.lock();
    if(twrowner)
    {
        if(twrowner.get() == t)
        {
            if(reason) *reason = EDEADLK;
            return false;
        }
        else
        {
            if(reason) *reason = EBUSY;
            if(block)
            {
                t->set_is_blocking(true);
                t->blocking_on_thread = twrowner;
                waiting_threads.insert(t->id);

                if(kernel_time_is_valid(tout))
                    t->block_until = tout;
                Yield();
            }
            return false;
        }
    }
    rdowners.insert(t->id);
    t->locked_rwlocks.Add(id);
    return true;
}

bool RwLock::try_wrlock(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg(sl, ThreadList.sl);
    auto t = GetCurrentThreadForCore();
    auto twrowner = wrowner.lock();
    if(twrowner)
    {
        if(twrowner.get() == t)
        {
            if(reason) *reason = EDEADLK;
            return false;
        }
        else
        {
            if(reason) *reason = EBUSY;
            if(block)
            {
                t->set_is_blocking(true);
                t->blocking_on_thread = twrowner;
                waiting_threads.insert(t->id);

                if(kernel_time_is_valid(tout))
                    t->block_until = tout;
                Yield();
            }
            return false;
        }
    }
    if(!rdowners.empty())
    {
        if(rdowners.find(t->id) != rdowners.end())
        {
            if(reason) *reason = EDEADLK;
            return false;
        }
        else
        {
            if(reason) *reason = EBUSY;
            if(block)
            {
                t->set_is_blocking(true);

                bool found_rdthread = false;
                for(auto rdowner : rdowners)
                {
                    auto trdowner = ThreadList._get(rdowner);
                    if(trdowner)
                    {
                        found_rdthread = true;
                        t->blocking_on_thread = trdowner;
                        break;
                    }
                }

                if(!found_rdthread)
                {
                    // there are threads listed as the rdthread but they have since been deleted
                    // therefore, we can acquire the wrlock
                    rdowners.clear();
                    wrowner = GetCurrentPThreadForCore();
                    t->locked_rwlocks.Add(id);
                    return true;
                }

                waiting_threads.insert(t->id);

                if(kernel_time_is_valid(tout))
                    t->block_until = tout;
                Yield();
            }
            return false;
        }
    }
    wrowner = GetCurrentPThreadForCore();
    t->locked_rwlocks.Add(id);
    return true;
}

bool RwLock::unlock(int *reason)
{
    CriticalGuard cg(sl, ThreadList.sl);
    auto t = GetCurrentThreadForCore();
    auto twrowner = wrowner.lock();
    if(twrowner)
    {
        if(twrowner.get() == t)
        {
            for(auto wt : waiting_threads)
            {
                auto pwt = ThreadList._get(wt);
                if(pwt)
                {
                    pwt->set_is_blocking(false);
                    pwt->block_until = kernel_time_invalid();
                    pwt->blocking_on_thread = WPThread{};

                    signal_thread_woken(pwt);
                }
            }
            waiting_threads.clear();
            wrowner = WPThread{};

            t->locked_rwlocks.Delete(id);

            return true;
        }
        else
        {
            if(reason) *reason = EPERM;
            return false;
        }
    }
    if(rdowners.empty())
    {
        if(reason) *reason = EPERM;
        return false;
    }
    auto iter = rdowners.find(t->id);
    if(iter == rdowners.end())
    {
        if(reason) *reason = EPERM;
        return false;
    }
    rdowners.erase(iter);

    if(rdowners.empty())
    {
        for(auto wt : waiting_threads)
        {
            auto pwt = ThreadList._get(wt);
            if(pwt)
            {
                pwt->set_is_blocking(false);
                pwt->block_until = kernel_time_invalid();
                pwt->blocking_on_thread = WPThread{};

                signal_thread_woken(pwt);
            }
        }
        waiting_threads.clear();
    }
    t->locked_rwlocks.Delete(id);
    return true;
}

bool RwLock::try_delete(int *reason)
{
    CriticalGuard cg(sl, ThreadList.sl);
    auto twrowner = wrowner.lock();
    if(twrowner)
    {
        if(reason) *reason = EBUSY;
        return false;
    }
    if(rdowners.empty())
    {
        return true;
    }
    for(auto rdo : rdowners)
    {
        auto prdo = ThreadList._get(rdo);
        if(prdo)
        {
            if(reason) *reason = EBUSY;
            return false;
        }
    }
    return true;
}
