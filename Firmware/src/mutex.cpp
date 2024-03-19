#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "clocks.h"

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
#if DEBUG_SPINLOCK
    uint32_t lr;
    __asm__ volatile ("mov %0, lr \n" : "=r" (lr));
    _s.lock(lr & ~1UL);
#else
    _s.lock();
#endif
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

void Spinlock::lock(
#if DEBUG_SPINLOCK
    uint32_t _locking_pc
#endif
)
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
#if DEBUG_SPINLOCK
            locking_core = GetCoreID();
            locked_by = GetCurrentThreadForCore(locking_core);
            locking_pc = _locking_pc;
#endif
            __DMB();
            return;
        }
    }
}

void Spinlock::unlock()
{
#if DEBUG_SPINLOCK
    locked_by = nullptr;
    locking_core = 0;
#endif
    set(&_lock_val, 0UL);
    __DMB();
}

SimpleSignal::SimpleSignal(uint32_t v) : signal_value(v)
{}

uint32_t SimpleSignal::WaitOnce(SignalOperation op, uint32_t vop, uint64_t tout)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    if(waiting_thread && t != waiting_thread)
        return false;
    if(signal_value)
    {
        auto ret = signal_value;
        do_op(op, vop);
        return ret;
    }
    waiting_thread = t;
    t->is_blocking = true;
    if(tout != UINT64_MAX)
        t->block_until = tout;
    Yield();
    if(signal_value)
    {
        auto ret = signal_value;
        do_op(op, vop);
        return ret;
    }
    return signal_value;    
}

uint32_t SimpleSignal::Wait(SignalOperation op, uint32_t vop, uint64_t tout)
{
    while(true)
    {
        auto sv = WaitOnce(op, vop, tout);
        if(sv)
            return sv;
        else if(clock_cur_ms() >= tout)
            return 0;
    }
}

void SimpleSignal::Signal(SignalOperation op, uint32_t val)
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
    do_op(op, val);
    if(hpt)
    {
        Yield();
    }
}

void SimpleSignal::do_op(SignalOperation op, uint32_t vop)
{
    switch(op)
    {
        case SignalOperation::Add:
            signal_value += vop;
            break;
        case SignalOperation::And:
            signal_value &= vop;
            break;
        case SignalOperation::Noop:
            break;
        case SignalOperation::Or:
            signal_value |= vop;
            break;
        case SignalOperation::Set:
            signal_value = vop;
            break;
        case SignalOperation::Sub:
            signal_value -= vop;
            break;
        case SignalOperation::Xor:
            signal_value ^= vop;
            break;
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
    waiting_threads.insert(t);
    t->is_blocking = true;
    Yield();
}

void BinarySemaphore::Signal()
{
    ss.Signal();
}

bool BinarySemaphore::Wait(uint64_t tout)
{
    return ss.Wait(SimpleSignal::Set, 0, tout) != 0;
}

bool BinarySemaphore::WaitOnce(uint64_t tout)
{
    return ss.WaitOnce(SimpleSignal::Set, 0, tout) != 0;
}

void CountingSemaphore::Signal()
{
    ss.Signal(SimpleSignal::Add, 1);
}

bool CountingSemaphore::Wait(uint64_t tout)
{
    return ss.Wait(SimpleSignal::Sub, 1, tout) != 0;
}

bool CountingSemaphore::WaitOnce(uint64_t tout)
{
    return ss.WaitOnce(SimpleSignal::Sub, 1, tout) != 0;
}

void Condition::Signal()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    bool hpt = false;
    for(auto bt : waiting_threads)
    {
        bt->is_blocking = false;
        bt->block_until = 0;
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
        waiting_threads.insert(t);
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
        wt->block_until = 0;
        wt->blocking_on = nullptr;
    }
    waiting_threads.clear();
    owner = nullptr;
    return true;
}
