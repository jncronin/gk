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

uint32_t SimpleSignal::WaitOnce()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    if(waiting_thread && t != waiting_thread)
        return false;
    if(signal_value)
        return signal_value;
    waiting_thread = t;
    t->is_blocking = true;
    Yield();
    return signal_value;    
}

uint32_t SimpleSignal::Wait()
{
    while(true)
    {
        auto sv = WaitOnce();
        if(sv)
            return sv;
    }
}

void SimpleSignal::Signal(uint32_t val)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    bool hpt = false;
    if(waiting_thread)
    {
        waiting_thread->is_blocking = false;
        if(waiting_thread->base_priority > t->base_priority)
            hpt = true;
        waiting_thread = nullptr;
    }
    signal_value = true;
    if(hpt)
    {
        Yield();
    }
}

void SimpleSignal::Reset()
{
    CriticalGuard cg(sl);
    waiting_thread = nullptr;
    signal_value = 0;
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

void Mutex::lock()
{
    while(!try_lock());
}

bool Mutex::try_lock()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    if(owner == nullptr)
    {
        owner = t;
        return true;
    }
    else
    {
        t->is_blocking = true;
        t->blocking_on = owner;
        waiting_threads.push_back(t);
        Yield();
        return false;
    }
}

bool Mutex::unlock()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    if(owner != t)
    {
        return false;
    }
    for(auto wt : waiting_threads)
    {
        wt->is_blocking = false;
        wt->blocking_on = nullptr;
    }
    waiting_threads.clear();
    owner = nullptr;
    return true;
}
