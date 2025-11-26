#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "ipi.h"
#include "threadproclist.h"

Mutex::Mutex(bool recursive, bool error_check) : 
    owner(WPThread{}),
    is_recursive(recursive),
    echeck(error_check),
    lockcount(0)
{
}

void Mutex::lock(bool allow_deadlk)
{
    int reason;
    while(!try_lock(&reason))
    {
        if(allow_deadlk && reason == EDEADLK)
        {
            return;
        }
        
        if(reason != EBUSY)
        {
            __asm__ volatile("brk #212\n" ::: "memory");
        }
    }
}

bool Mutex::try_lock(int *reason, bool block, kernel_time tout)
{
    auto t = GetCurrentPThreadForCore();
    CriticalGuard cg(sl, t->locked_mutexes.sl);
    auto towner = owner.lock();
    if(towner == nullptr || (is_recursive && towner == t))
    {
        owner = t;
        if(is_recursive)
            lockcount++;
        t->locked_mutexes.pset.insert(id);
        return true;
    }
    else if(towner == t)
    {
        if(reason) *reason = EDEADLK;
        if(echeck)
        {
            return false;
        }
        else
        {
            // non-error checking non-recursive mutex already owned - deadlock
            t->set_is_blocking(true);
            t->blocking_on_prim = nullptr;
            Yield();
            return false;
        }
    }
    else
    {
        if(reason) *reason = EBUSY;

        if(block)
        {
            t->set_is_blocking(true);
            t->blocking_on_thread = towner;
            waiting_threads.insert(t->id);

            if(kernel_time_is_valid(tout))
                t->block_until = tout;
            Yield();
        }
        return false;
    }
}

bool Mutex::unlock(int *reason, bool force)
{
    CriticalGuard cg(sl, ThreadList.sl);
    auto towner = owner.lock();
    if(!towner)
    {
        if(reason) *reason = EPERM;
        return false;
    }
    auto t = GetCurrentThreadForCore();
    if(((towner.get() != t) && (force == false)) ||
        ((force == true) && towner->is_privileged))
    {
        if(reason) *reason = EPERM;
        return false;
    }
    if(!is_recursive || --lockcount == 0)
    {
        for(auto wt : waiting_threads)
        {
            auto pwt = ThreadList._get(wt);
            if(pwt)
            {
                pwt->set_is_blocking(false);
                pwt->block_until = kernel_time_invalid();
                pwt->blocking_on_prim = nullptr;

                signal_thread_woken(pwt);
            }
        }
        waiting_threads.clear();
        owner = WPThread{};
    }

    t->locked_mutexes.Delete(id);

    return true;
}

bool Mutex::try_delete(int *reason)
{
    CriticalGuard cg(sl, ThreadList.sl);

    auto towner = owner.lock();
    if(towner == nullptr || towner.get() == GetCurrentThreadForCore())
    {
        for(auto wt : waiting_threads)
        {
            auto pwt = ThreadList._get(wt);
            if(pwt)
            {
                pwt->set_is_blocking(false);
                pwt->block_until = kernel_time_invalid();
                pwt->blocking_on_prim = nullptr;
                signal_thread_woken(pwt);
            }
        }
        waiting_threads.clear();

        if(towner)
            GetCurrentThreadForCore()->locked_mutexes.Delete(id);
        
        return true;
    }
    if(reason) *reason = EBUSY;
    return false;
}

