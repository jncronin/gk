#include "osmutex.h"
#include "clocks.h"
#include "thread.h"
#include "ipi.h"
#include "scheduler.h"
#include "gk_conf.h"

extern BinarySemaphore klog_updated;

SimpleSignal::SimpleSignal(uint32_t v, uint32_t max_val) : signal_value(v), max_value(max_val)
{}

uint32_t SimpleSignal::WaitOnce(SignalOperation op, uint32_t vop, kernel_time tout)
{
    CriticalGuard cg(sl);
    auto t = GetCurrentThreadForCore();
    {
        if(waiting_thread && waiting_thread != t->id)
            return false;
    }
    if(signal_value)
    {
        auto ret = signal_value;
        do_op(op, vop);
        waiting_thread = 0;
#if GK_PROFILE_SS
        if(this != (SimpleSignal *)&klog_updated)
            klog("ss: %p ret\n", this);
#endif
        return ret;
    }
    waiting_thread = t->id;

    t->blocking.block(this, tout);
    Yield();
    return 0;  
}

uint32_t SimpleSignal::Wait(SignalOperation op, uint32_t vop, kernel_time tout)
{
#if GK_PROFILE_SS
    if(this != (SimpleSignal *)&klog_updated)
        klog("ss: %p wait\n", this);
#endif
    while(true)
    {
        auto sv = WaitOnce(op, vop, tout);
        if(sv)
            return sv;
        else if(kernel_time_is_valid(tout) && clock_cur() >= tout)
            return 0;
    }
}

void SimpleSignal::Signal(SignalOperation op, uint32_t val)
{
#if GK_PROFILE_SS
    if(this != (SimpleSignal *)&klog_updated)
        klog("ss: %p signal\n", this);
#endif
    CriticalGuard cg(sl, ThreadList.sl);
    do_op(op, val);
    auto pwt = ThreadList._get(waiting_thread).v;
    cg.unlock();
    if(pwt)
    {
        pwt->blocking.unblock();
        signal_thread_woken(pwt);
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
        case SignalOperation::AddIfLessThanMax:
            if(signal_value < max_value)
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

