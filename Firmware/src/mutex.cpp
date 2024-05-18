#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "clocks.h"
#include "ipi.h"
#include "gk_conf.h"

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
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
#if DEBUG_SPINLOCK
    uint32_t lr;
    __asm__ volatile ("mov %0, lr \n" : "=r" (lr));
    _s.lock(lr & ~1UL);
#else
    _s.lock();
#endif
#endif
}

CriticalGuard::~CriticalGuard()
{
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
    _s.unlock();
#endif
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
    locking_pc = 0;
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
        waiting_thread = nullptr;
        return ret;
    }
    waiting_thread = t;
    t->is_blocking = true;
    t->blocking_on = BLOCKING_ON_SS(this);   
    if(tout != UINT64_MAX)
        t->block_until = tout;
    Yield();
    if(signal_value)
    {
        auto ret = signal_value;
        do_op(op, vop);
        waiting_thread = nullptr;
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
    do_op(op, val);
    if(waiting_thread)
    {
        waiting_thread->is_blocking = false;
        if(waiting_thread->base_priority > t->base_priority)
            hpt = true;
        signal_thread_woken(waiting_thread);
        waiting_thread = nullptr;
    }
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

void Condition::Wait(uint64_t tout, int *signalled_ret)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    timeout to { tout, signalled_ret };
    waiting_threads.insert_or_assign(t, to);
    t->is_blocking = true;
    t->blocking_on = BLOCKING_ON_CONDITION(this);
    if(tout != UINT64_MAX)
        t->block_until = tout;
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

Condition::~Condition()
{
    // wake up all waiting threads
    CriticalGuard cg(sl);
    bool hpt = false;
    auto t = GetCurrentThreadForCore();
    for(auto &bt : waiting_threads)
    {
        bt.first->is_blocking = false;
        bt.first->block_until = 0;
        if(bt.first->base_priority > t->base_priority)
            hpt = true;
        signal_thread_woken(bt.first);
    }

    if(hpt)
    {
        Yield();
    }
}

void Condition::Signal(bool signal_all)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    bool hpt = false;
    auto tnow = clock_cur_ms();

    if(signal_all)
    {
        for(auto &bt : waiting_threads)
        {
            bool timedout = false;
            if(bt.second.tout != UINT64_MAX &&
                tnow > bt.second.tout)
            {
                timedout = 1;
            }

            if(!timedout)
            {
                if(bt.second.signalled)
                    *bt.second.signalled = true;
                
                bt.first->is_blocking = false;
                bt.first->block_until = 0;
                if(bt.first->base_priority > t->base_priority)
                    hpt = true;
                signal_thread_woken(bt.first);
            }
        }
        waiting_threads.clear();
    }
    else
    {
        // need to keep looking for a single thread to wake
        auto iter = waiting_threads.begin();
        while(iter != waiting_threads.end())
        {
            bool timedout = false;
            auto &tp = iter->second;
            if(tp.tout != UINT64_MAX &&
                tnow > tp.tout)
            {
                timedout = 1;
            }

            if(!timedout)
            {
                if(tp.signalled)
                    *tp.signalled = true;
                
                iter->first->is_blocking = false;
                iter->first->block_until = 0;
                if(iter->first->base_priority > t->base_priority)
                    hpt = true;
                signal_thread_woken(iter->first);
                waiting_threads.erase(iter);
                break;
            }
            else
            {
                iter = waiting_threads.erase(iter);
            }
        }
    }

    if(hpt)
    {
        Yield();
    }
}

Mutex::Mutex(bool recursive) : 
    owner(nullptr),
    is_recursive(recursive),
    lockcount(0)
{}

void Mutex::lock()
{
    while(!try_lock());
}

bool Mutex::try_lock()
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    if(owner == nullptr || (is_recursive && owner == t))
    {
        owner = t;
        if(is_recursive)
            lockcount++;
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
    bool hpt = false;
    if(owner != t)
    {
        return false;
    }
    if(!is_recursive || --lockcount == 0)
    {
        for(auto wt : waiting_threads)
        {
            wt->is_blocking = false;
            wt->block_until = 0;
            wt->blocking_on = nullptr;

            if(wt->base_priority > t->base_priority)
                hpt = true;
            signal_thread_woken(wt);
        }
        waiting_threads.clear();
        owner = nullptr;
    }

    if(hpt)
        Yield();
    
    return true;
}

bool Mutex::try_delete()
{
    CriticalGuard cg(sl);
    if(owner == nullptr || owner == GetCurrentThreadForCore())
    {
        for(auto wt : waiting_threads)
        {
            wt->is_blocking = false;
            wt->block_until = 0;
            wt->blocking_on = nullptr;
        }
        waiting_threads.clear();
        return true;
    }
    return false;
}
