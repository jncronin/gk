#include "osmutex.h"
#include "scheduler.h"
#include "ipi.h"

bool UserspaceSemaphore::try_wait(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    if(val)
    {
        val--;
        return true;
    }
    else
    {
        // val == 0
        if(reason) *reason = EBUSY;
        if(block)
        {
            t->blocking.block(this, tout);
            waiting_threads.insert(t->id);
            Yield();
        }
        return false;
    }
}

void UserspaceSemaphore::post(int n, bool add)
{
    CriticalGuard cg(sl, ThreadList.sl);
    if(val == 0 && n > 0)
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
    }
    if(add)
    {
        if(n < 0)
        {
            if((unsigned int)-n > val)
                val = 0;
            else val -= (unsigned int)-n;
        }
        else
        {
            val += (unsigned int)n;
        }
    }
    else
    {
        if(n < 0)
            val = 0;
        else
            val = (unsigned int)n;
    }
}

bool UserspaceSemaphore::try_delete(int *reason)
{
    // always succeeds - posix states UB if threads are waiting on it
    return true;
}

UserspaceSemaphore::UserspaceSemaphore(unsigned int value)
{
    val = value;
}

unsigned int UserspaceSemaphore::get_value()
{
    CriticalGuard cg(sl);
    return val;
}
