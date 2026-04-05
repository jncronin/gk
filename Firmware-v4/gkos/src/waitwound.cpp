#include "waitwound.h"
#include "clocks.h"

int WaitWoundContext::lock(std::shared_ptr<Mutex> &m)
{
    if(done_acquire)
    {
        klog("ww: lock after acquire_done()\n");
        return -1;
    }
    auto ret = m->lock(ticket);
    if(ret == 0)
    {
        CriticalGuard cg(sl);
        locked_mutexes.push_back(m);
    }
    return ret;
}

int WaitWoundContext::lock_slow(std::shared_ptr<Mutex> &m)
{
    if(done_acquire)
    {
        klog("ww: lock after acquire_done()\n");
        return -1;
    }
    // slow locking is just to add more debugging stuff
    auto ret = m->lock(ticket);
    if(ret == 0)
    {
        CriticalGuard cg(sl);
        locked_mutexes.push_back(m);
    }
    return ret;
}

void WaitWoundContext::acquire_done()
{
    done_acquire = true;
}

WaitWoundContext::WaitWoundContext()
{
    ticket = clock_cur();
}

WaitWoundContext::~WaitWoundContext()
{
    for(auto &m : locked_mutexes)
    {
        m->unlock();
    }
}
