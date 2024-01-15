#include "osmutex.h"

UninterruptibleGuard::UninterruptibleGuard()
{
    cpsr = DisableInterrupts();
}

UninterruptibleGuard::~UninterruptibleGuard()
{
    RestoreInterrupts(cpsr);
}

CriticalGuard::CriticalGuard(Spinlock &s) : _s(s)
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
