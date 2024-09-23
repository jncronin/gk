#include "osmutex.h"
#include "thread.h"
#include "scheduler.h"
#include "clocks.h"
#include "sync_primitive_locks.h"
#include "util.h"
#include "gk_conf.h"

Spinlock::Spinlock()
{

}

INTFLASH_FUNCTION CriticalGuard::CriticalGuard()
{
    cpsr = DisableInterrupts();
}

INTFLASH_FUNCTION CriticalGuard::CriticalGuard(Spinlock &s1)
{
    cpsr = DisableInterrupts();
}

INTFLASH_FUNCTION CriticalGuard::~CriticalGuard()
{
    RestoreInterrupts(cpsr);
}

SimpleSignal::SimpleSignal(uint32_t v) : signal_value(v)
{}

uint32_t SimpleSignal::WaitOnce(SignalOperation op, uint32_t vop, kernel_time tout)
{
    CriticalGuard cg;
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
    if(tout.is_valid())
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

uint32_t SimpleSignal::Wait(SignalOperation op, uint32_t vop, kernel_time tout)
{
    while(true)
    {
        auto sv = WaitOnce(op, vop, tout);
        if(sv)
            return sv;
        else if(tout.is_valid() && clock_cur() >= tout)
            return 0;
    }
}

void SimpleSignal::Signal(SignalOperation op, uint32_t val)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    bool hpt = false;
    do_op(op, val);
    if(waiting_thread)
    {
        //CriticalGuard cg2(waiting_thread->sl);
        waiting_thread->is_blocking = false;
        waiting_thread->blocking_on = nullptr;
        if(waiting_thread->base_priority > t->base_priority)
            hpt = true;
        signal_thread_woken(waiting_thread);
    }
    if(hpt)
    {
        Yield();
    }
}

uint32_t SimpleSignal::Value()
{
    return signal_value;
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

void Condition::Wait(kernel_time tout, int *signalled_ret)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    timeout to { tout, signalled_ret };
    waiting_threads.insert_or_assign(t, to);
    t->is_blocking = true;
    t->blocking_on = BLOCKING_ON_CONDITION(this);
    if(tout.is_valid())
        t->block_until = tout;
    Yield();
}

void BinarySemaphore::Signal()
{
    ss.Signal();
}

bool BinarySemaphore::Wait(kernel_time tout)
{
    return ss.Wait(SimpleSignal::Set, 0, tout) != 0;
}

bool BinarySemaphore::WaitOnce(kernel_time tout)
{
    return ss.WaitOnce(SimpleSignal::Set, 0, tout) != 0;
}

void CountingSemaphore::Signal()
{
    ss.Signal(SimpleSignal::Add, 1);
}

bool CountingSemaphore::Wait(kernel_time tout)
{
    return ss.Wait(SimpleSignal::Sub, 1, tout) != 0;
}

bool CountingSemaphore::WaitOnce(kernel_time tout)
{
    return ss.WaitOnce(SimpleSignal::Sub, 1, tout) != 0;
}

unsigned int CountingSemaphore::Value()
{
    return ss.Value();
}

Condition::~Condition()
{
    // wake up all waiting threads
    CriticalGuard cg;
    bool hpt = false;
    auto t = GetCurrentThreadForCore();
    for(auto &bt : waiting_threads)
    {
        //CriticalGuard cg2(bt.first->sl);
        bt.first->is_blocking = false;
        bt.first->blocking_on = nullptr;
        bt.first->block_until.invalidate();
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
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    bool hpt = false;
    auto tnow = clock_cur();

    if(signal_all)
    {
        for(auto &bt : waiting_threads)
        {
            bool timedout = false;
            if(bt.second.tout.is_valid() &&
                tnow > bt.second.tout)
            {
                timedout = 1;
            }

            if(!timedout)
            {
                if(bt.second.signalled)
                    *bt.second.signalled = true;
                
                bt.first->is_blocking = false;
                bt.first->blocking_on = nullptr;
                bt.first->block_until.invalidate();
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
            if(tp.tout.is_valid() &&
                tnow > tp.tout)
            {
                timedout = 1;
            }

            if(!timedout)
            {
                if(tp.signalled)
                    *tp.signalled = true;
                
                iter->first->is_blocking = false;
                iter->first->blocking_on = nullptr;
                iter->first->block_until.invalidate();
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

Mutex::Mutex(bool recursive, bool error_check) : 
    owner(nullptr),
    is_recursive(recursive),
    echeck(error_check),
    lockcount(0)
{}

void Mutex::lock()
{
    int reason;
    while(!try_lock(&reason))
    {
        if(reason != EBUSY)
        {
            __asm__ volatile("bkpt \n" ::: "memory");
        }
    }
}

bool Mutex::try_lock(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    if(owner == nullptr || (is_recursive && owner == t))
    {
        owner = t;
        if(is_recursive)
            lockcount++;
        add_sync_primitive(this, t->locked_mutexes, t);
        return true;
    }
    else if(owner == t)
    {
        if(reason) *reason = EDEADLK;
        if(echeck)
        {
            return false;
        }
        else
        {
            // non-error checking non-recursive mutex already owned - deadlock
            t->is_blocking = true;
            t->blocking_on = nullptr;
            Yield();
            return false;
        }
    }
    else
    {
        if(reason) *reason = EBUSY;

        if(block)
        {
            t->is_blocking = true;
            t->blocking_on = owner;
            waiting_threads.insert(t);

            if(tout.is_valid())
                t->block_until = tout;
            Yield();
        }
        return false;
    }
}

bool Mutex::unlock(int *reason)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    if(owner != t)
    {
        if(reason) *reason = EPERM;
        return false;
    }
    if(!is_recursive || --lockcount == 0)
    {
        for(auto wt : waiting_threads)
        {
            wt->is_blocking = false;
            wt->block_until.invalidate();
            wt->blocking_on = nullptr;

            signal_thread_woken(wt);
        }
        waiting_threads.clear();
        owner = nullptr;
    }

    delete_sync_primitive(this, t->locked_mutexes, t);

    return true;
}

bool Mutex::try_delete(int *reason)
{
    CriticalGuard cg;
    if(owner == nullptr || owner == GetCurrentThreadForCore())
    {
        for(auto wt : waiting_threads)
        {
            wt->is_blocking = false;
            wt->block_until.invalidate();
            wt->blocking_on = nullptr;
        }
        waiting_threads.clear();
        return true;
    }
    if(reason) *reason = EBUSY;
    return false;
}

bool RwLock::try_rdlock(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    if(wrowner)
    {
        if(wrowner == t)
        {
            if(reason) *reason = EDEADLK;
            return false;
        }
        else
        {
            if(reason) *reason = EBUSY;
            if(block)
            {
                t->is_blocking = true;
                t->blocking_on = wrowner;
                waiting_threads.insert(t);

                if(tout.is_valid())
                    t->block_until = tout;
                Yield();
            }
            return false;
        }
    }
    rdowners.insert(t);
    add_sync_primitive(this, t->locked_rwlocks, t);
    return true;
}

bool RwLock::try_wrlock(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    if(wrowner)
    {
        if(wrowner == t)
        {
            if(reason) *reason = EDEADLK;
            return false;
        }
        else
        {
            if(reason) *reason = EBUSY;
            if(block)
            {
                t->is_blocking = true;
                t->blocking_on = wrowner;
                waiting_threads.insert(t);

                if(tout.is_valid())
                    t->block_until = tout;
                Yield();
            }
            return false;
        }
    }
    if(!rdowners.empty())
    {
        if(rdowners.find(t) != rdowners.end())
        {
            if(reason) *reason = EDEADLK;
            return false;
        }
        else
        {
            if(reason) *reason = EBUSY;
            if(block)
            {
                t->is_blocking = true;
                t->blocking_on = *rdowners.begin();
                waiting_threads.insert(t);

                if(tout.is_valid())
                    t->block_until = tout;
                Yield();
            }
            return false;
        }
    }
    wrowner = t;
    add_sync_primitive(this, t->locked_rwlocks, t);
    return true;
}

bool RwLock::unlock(int *reason)
{
    CriticalGuard cg;
    auto t = GetCurrentThreadForCore();
    if(wrowner)
    {
        if(wrowner == t)
        {
            for(auto wt : waiting_threads)
            {
                wt->is_blocking = false;
                wt->block_until.invalidate();
                wt->blocking_on = nullptr;

                signal_thread_woken(wt);
            }
            waiting_threads.clear();
            wrowner = nullptr;

            delete_sync_primitive(this, t->locked_rwlocks, t);

            return true;
        }
        else
        {
            if(reason) *reason = EPERM;
            return false;
        }
    }
    if(rdowners.empty())
    {
        if(reason) *reason = EPERM;
        return false;
    }
    auto iter = rdowners.find(t);
    if(iter == rdowners.end())
    {
        if(reason) *reason = EPERM;
        return false;
    }
    rdowners.erase(iter);

    if(rdowners.empty())
    {
        for(auto wt : waiting_threads)
        {
            wt->is_blocking = false;
            wt->block_until.invalidate();
            wt->blocking_on = nullptr;

            signal_thread_woken(wt);
        }
        waiting_threads.clear();
    }
    delete_sync_primitive(this, t->locked_rwlocks, t);
    return true;
}

bool RwLock::try_delete(int *reason)
{
    CriticalGuard cg;
    if(wrowner || !rdowners.empty())
    {
        if(reason) *reason = EBUSY;
        return false;
    }
    return true;
}

bool UserspaceSemaphore::try_wait(int *reason, bool block, kernel_time tout)
{
    CriticalGuard cg;
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
            t->is_blocking = true;
            t->blocking_on = BLOCKING_ON_CONDITION(this);
            waiting_threads.insert(t);

            if(tout.is_valid())
                t->block_until = tout;
            Yield();
        }
        return false;
    }
}

void UserspaceSemaphore::post(int n, bool add)
{
    CriticalGuard cg;
    if(val == 0 && n > 0)
    {
        for(auto wt : waiting_threads)
        {
            wt->is_blocking = false;
            wt->block_until.invalidate();
            wt->blocking_on = 0;

            signal_thread_woken(wt);
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
    CriticalGuard cg;
    return val;
}
