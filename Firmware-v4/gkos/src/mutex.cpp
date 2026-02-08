#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "ipi.h"
#include "threadproclist.h"
#include "logger.h"

Mutex::Mutex(bool recursive, bool error_check) : 
    owner(0),
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

void Mutex::_lock(bool allow_deadlk)
{
    int reason;
    while(!_try_lock(&reason))
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
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg(sl, t->locked_mutexes.sl);

    return _try_lock(reason, block, tout);
}

bool Mutex::_try_lock(int *reason, bool block, kernel_time tout)
{
    auto t = GetCurrentThreadForCore();
    auto towner = ThreadList.Get(owner).v;
    if(towner == nullptr || (is_recursive && towner.get() == t))
    {
        owner = t->id;
        if(is_recursive)
            lockcount++;
        t->locked_mutexes.pset.insert(id);
        return true;
    }
    else if(towner.get() == t)
    {
        if(reason) *reason = EDEADLK;
        if(echeck)
        {
            return false;
        }
        else
        {
            // non-error checking non-recursive mutex already owned - deadlock
            klog("mutex: recursive lock of non-recursive mutex\n");
            t->blocking.block_indefinite();
            Yield();
            return false;
        }
    }
    else
    {
        if(reason) *reason = EBUSY;

        if(block)
        {
#if DEBUG_MUTEX
            klog("mutex: %s blocking on %p owned by %s\n",
                t->name.c_str(), this, towner->name.c_str());
#endif
            t->blocking.block(towner, tout);
            waiting_threads.insert(t->id);
            Yield();
        }
        return false;
    }
}

bool Mutex::unlock(bool do_unlock)
{
    return do_unlock ? unlock() : false;
}

bool Mutex::unlock(int *reason, bool force)
{
    CriticalGuard cg(sl, ThreadList.sl);
    return _unlock(reason, force);
}

bool Mutex::_unlock(int *reason, bool force)
{
    auto towner = ThreadList._get(owner).v;
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
            auto pwt = ThreadList._get(wt).v;
            if(pwt)
            {
                pwt->blocking.unblock();
                signal_thread_woken(pwt);
            }
        }
        waiting_threads.clear();
        owner = 0;
    }

    t->locked_mutexes.Delete(id);

    return true;
}

bool Mutex::try_delete(int *reason)
{
    CriticalGuard cg(sl, ThreadList.sl);

    auto towner = ThreadList._get(owner).v;
    if(towner == nullptr || towner.get() == GetCurrentThreadForCore())
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
        waiting_threads.clear();

        if(towner)
            GetCurrentThreadForCore()->locked_mutexes.Delete(id);
        
        return true;
    }
    if(reason) *reason = EBUSY;
    return false;
}

