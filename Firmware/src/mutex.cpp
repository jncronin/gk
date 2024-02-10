#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"

extern Scheduler s;

UninterruptibleGuard::UninterruptibleGuard()
{
    cpsr = DisableInterrupts();
}

UninterruptibleGuard::~UninterruptibleGuard()
{
    RestoreInterrupts(cpsr);
}

CriticalGuard::CriticalGuard(Spinlock &sl) : _s(sl)
{
    cpsr = DisableInterrupts();
    _s.lock();
}

CriticalGuard::~CriticalGuard()
{
    _s.unlock();
    RestoreInterrupts(cpsr);
}

Spinlock::Spinlock()
{
    RCC->AHB3ENR |= RCC_AHB4ENR_HSEMEN;
    (void)RCC->AHB3ENR;
}

void Spinlock::lock()
{
    while(true)
    {
        uint32_t expected_zero = 0;
        cmpxchg(&_lock_val, &expected_zero, 1UL);
        if(expected_zero)
        {
            // spin in non-locking mode until unset
            while(_lock_val) __DMB();
        }
        else
        {
            __DMB();
            return;
        }
    }
}

void Spinlock::unlock()
{
    set(&_lock_val, 0UL);
    __DMB();
}

void Condition::Wait()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    bool already_waiting = false;
    for(const auto it : waiting_threads)
    {
        if(it == t)
        {
            already_waiting = true;
            break;
        }
    }
    if(!already_waiting)
        waiting_threads.push_back(t);
    t->is_blocking = true;
    Yield();
}

void Condition::Signal()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    bool hpt = false;
    for(auto bt : waiting_threads)
    {
        bt->is_blocking = false;
        if(bt->base_priority > t->base_priority)
            hpt = true;
    }
    waiting_threads.clear();
    if(hpt)
    {
        Yield();
    }
}