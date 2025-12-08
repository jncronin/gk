#include "thread.h"
#include "clocks.h"
#include "process.h"

bool Thread::blocking_t::is_blocking(kernel_time *tout, PThread *bt)
{
    CriticalGuard cg(sl);
    auto bt_locked = b_thread.lock();
    if(kernel_time_is_valid(b_until) && b_until <= clock_cur())
        b_until = kernel_time_invalid();
    auto isb = b_indefinite ||
        kernel_time_is_valid(b_until) ||
        bt_locked ||
#if GK_DEBUG_BLOCKING
        b_condition ||
        b_ss ||
        b_uss ||
        b_rwl ||
        b_queue;
#else
        b_prim;
#endif
    if(isb)
    {
        if(tout) *tout = b_until;
        if(bt) *bt = bt_locked;
    }
    return isb;
}

void Thread::blocking_t::unblock()
{
    CriticalGuard cg(sl);
    b_until = kernel_time_invalid();
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = false;
#endif
}

void Thread::blocking_t::block(PThread t, kernel_time tout)
{
    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = t;
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = false;
#endif
}

void Thread::blocking_t::block(Condition *c, kernel_time tout)
{
    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = c;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = true;
#endif
}

void Thread::blocking_t::block(SimpleSignal *ss, kernel_time tout)
{
    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = ss;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = true;
#endif
}

void Thread::blocking_t::block(UserspaceSemaphore *uss, kernel_time tout)
{
    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = uss;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = true;
#endif
}

void Thread::blocking_t::block(RwLock *rwl, kernel_time tout)
{
    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = rwl;
    b_queue = nullptr;
#else
    b_prim = true;
#endif
}

void Thread::blocking_t::block(void *q, kernel_time tout)
{
    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = q;
#else
    b_prim = true;
#endif
}

void Thread::blocking_t::block_indefinite()
{
    klog("block: block_indefinite called: %s:%s\n",
        GetCurrentProcessForCore()->name.c_str(),
        GetCurrentThreadForCore()->name.c_str());

    CriticalGuard cg(sl);
    b_until = kernel_time_invalid();
    b_indefinite = true;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = false;
#endif
}

void Thread::blocking_t::block(kernel_time tout)
{
    if(kernel_time_is_valid(tout) == false || tout <= clock_cur())
    {
        unblock();
        return;
    }

    CriticalGuard cg(sl);
    b_until = tout;
    b_indefinite = false;
    b_thread = WPThread{};
#if GK_DEBUG_BLOCKING
    b_condition = nullptr;
    b_ss = nullptr;
    b_uss = nullptr;
    b_rwl = nullptr;
    b_queue = nullptr;
#else
    b_prim = false;
#endif
}
