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

void Spinlock::lock()
{
    while(true)
    {
        uint32_t expected_zero = 0;
        __atomic_compare_exchange_n(&_lock_val, &expected_zero, 1UL, false,
            __ATOMIC_RELAXED, __ATOMIC_RELAXED);
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
    __atomic_store_n(&_lock_val, 0, __ATOMIC_RELAXED);
    __DMB();
}

void Condition::Wait()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    waiting_threads.push_back(t);
    t->is_blocking = true;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
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
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}