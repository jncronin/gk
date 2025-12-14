#include "osmutex.h"
#include "clocks.h"
#include "thread.h"
#include "ipi.h"
#include "scheduler.h"

SimpleSignal::SimpleSignal(uint32_t v) : signal_value(v)
{}

uint32_t SimpleSignal::WaitOnce(SignalOperation op, uint32_t vop, kernel_time tout)
{
    CriticalGuard cg(sl, ThreadList.sl);
    auto t = GetCurrentThreadForCore();
    {
        auto pwt = ThreadList._get(waiting_thread);
        if(pwt && t != pwt.get())
            return false;
    }
    if(signal_value)
    {
        auto ret = signal_value;
        do_op(op, vop);
        waiting_thread = 0;
        return ret;
    }
    waiting_thread = t->id;
    t->blocking.block(this, tout);
    Yield();
    if(signal_value)
    {
        auto ret = signal_value;
        do_op(op, vop);
        waiting_thread = 0;
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
        else if(kernel_time_is_valid(tout) && clock_cur() >= tout)
            return 0;
    }
}

void SimpleSignal::Signal(SignalOperation op, uint32_t val)
{
    CriticalGuard cg(sl, ThreadList.sl);
    do_op(op, val);
    auto pwt = ThreadList._get(waiting_thread);
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

