#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "ipi.h"
#include "threadproclist.h"
#include "logger.h"
#include "clocks.h"

Mutex::Mutex(bool recursive, bool error_check) : 
    owner(0),
    is_recursive(recursive),
    echeck(error_check),
    lockcount(0)
{
}

/* We need to handle wound/wait mutexes differently here.
    pthread_mutex_lock returns +EBUSY if someone else owns the mutex
     and +EDEADLK if we already own it (assuming allow_deadlk is true)
    wound_wait mutex lock returns -EDEADLK if someone else owns the mutex with
     an older ticket, or +EBUSY if someone else owns it with a newer ticket,
     and -EALREADY if we already own it
*/
int Mutex::lock(ticket_t new_ticket, bool allow_deadlk)
{
    int reason, reason2 = 0;
    while(!try_lock(&reason, true, kernel_time_invalid(), new_ticket, &reason2))
    {
        if(reason == -EDEADLK || reason == -EALREADY)
        {
            return reason;
        }
        if(allow_deadlk && reason2 == EDEADLK)
        {
            return -EDEADLK;
        }
        
        if(reason != EBUSY)
        {
            __asm__ volatile("brk #212\n" ::: "memory");
        }
    }
    return 0;
}

int Mutex::_lock(ticket_t new_ticket, bool allow_deadlk)
{
    int reason;
    while(!_try_lock(&reason, true, kernel_time_invalid(), new_ticket))
    {
        if(reason == -EDEADLK || reason == -EALREADY)
        {
            return reason;
        }
        if(allow_deadlk && reason == EDEADLK)
        {
            return -EDEADLK;
        }
        
        if(reason != EBUSY)
        {
            __asm__ volatile("brk #212\n" ::: "memory");
        }
    }
    return 0;
}

bool Mutex::try_lock(int *reason, bool block, kernel_time tout, ticket_t ticket, int *reason2)
{
    auto t = GetCurrentThreadForCore();
    CriticalGuard cg(sl, t->locked_mutexes.sl);

    auto ret = _try_lock(reason, block, tout, ticket);
    if(reason && *reason == EDEADLK)
    {
        *reason = EBUSY;        // try_lock cannot return EDEADLK
        if(reason2) *reason2 = EDEADLK;     // but lock() can with error check, therefore capture this too
    }

    return ret;
}

bool Mutex::_try_lock(int *reason, bool block, kernel_time tout, ticket_t ticket)
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
        if(kernel_time_is_valid(ticket))
        {
            if(reason) *reason = -EALREADY;
            return true;
        }
        if(reason) *reason = EDEADLK;
        return false;
    }
    else
    {
        if(kernel_time_is_valid(ticket))
        {
            if(ticket > owning_ticket)
            {
                if(reason) *reason = -EDEADLK;
                return false;
            }
            if(reason) *reason = EBUSY;
        }
        else
        {
            if(reason) *reason = EBUSY;
        }

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
    auto [ ret, to_unlock ] = _unlock(reason, force);
    cg.unlock();
    if(ret)
    {
        for(auto pwt : to_unlock)
        {
            pwt->blocking.unblock();
            signal_thread_woken(pwt);
        }
    }
    return ret;
}

void Mutex::force_release(id_t)
{
    unlock(nullptr, true);
}

std::pair<bool, std::vector<PThread>> Mutex::_unlock(int *reason, bool force)
{
    auto towner = ThreadList._get(owner).v;
    if(!towner)
    {
        if(reason) *reason = EPERM;
        return std::make_pair(false, std::vector<PThread>());
    }
    auto t = GetCurrentThreadForCore();
    if(((towner.get() != t) && (force == false)) ||
        ((force == true) && towner->is_privileged && !GetCurrentThreadForCore()->is_privileged))
    {
        if(reason) *reason = EPERM;
        return std::make_pair(false, std::vector<PThread>());
    }

    std::vector<PThread> to_unlock{};
    if(!is_recursive || --lockcount == 0)
    {

        for(auto wt : waiting_threads)
        {
            auto pwt = ThreadList._get(wt).v;
            if(pwt)
            {
                to_unlock.push_back(std::move(pwt));
            }
        }
        waiting_threads.clear();
        owner = 0;
    }

    t->locked_mutexes.Delete(id);

    owning_ticket = kernel_time_invalid();

    return std::make_pair(true, to_unlock);
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

bool Mutex::held()
{
    auto t = GetCurrentThreadForCore();

    CriticalGuard cg(sl);
    return owner == t->id;
}
