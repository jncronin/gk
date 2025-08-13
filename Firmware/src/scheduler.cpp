#include "scheduler.h"
#include "syscalls.h"
#include "SEGGER_RTT.h"
#include "clocks.h"
#include "gk_conf.h"

#define DEBUG_SCHEDULER 0

extern Process kernel_proc;
Process *focus_process = &kernel_proc;     // which process has access to screen and inputs
SRAM4_DATA Thread dt(kernel_proc);

#if GK_DUAL_CORE
const int gk_ncores = 2;
const bool gk_is_smp = true;
#elif GK_DUAL_CORE_AMP
const int gk_ncores = 2;
const bool gk_is_smp = false;
#else
const int gk_ncores = 1;
const bool gk_is_smp = true;
#endif

Scheduler::Scheduler()
{
    for(int i = 0; i < npriorities; i++)
    {
        new (&tlist[i]) IndexedThreadVector();
    }
    for(int i = 0; i < ncores; i++)
    {
        new (&current_thread[i]) Thread*;
    }
    dt.name = "dummy";
    dt.base_priority = 0;

    for(int i = 0; i < ncores; i++)
    {
        CriticalGuard cg;
        current_thread[i] = &dt;
        scheduler_running[i] = false;
    }
}

void Schedule(Thread *t)
{
#if GK_DUAL_CORE_AMP
    if(t->tss.affinity == M7Only)
        scheds[0].Schedule(t);
    else
        scheds[1].Schedule(t);
#else
    sched.Schedule(t);
#endif
}

void Scheduler::Schedule(Thread *t)
{
    if(!t || t == &dt)
    {
        return;
    }

    auto prio = t->base_priority;
    if(prio >= npriorities)
    {
        prio = npriorities - 1;
    }
    if(prio < 0)
    {
        prio = 0;
    }

    {
        CriticalGuard cg;
        tlist[prio].v.push_back(t);

#if 0
        klog("scheduler: thread added (%s), current threads:\n", t->name.c_str());
        for(int i = 0; i < npriorities; i++)
        {
            for(const auto &ct : tlist[i].v)
            {
                klog("scheduler: prio %d: %s\n", i, ct->name.c_str());
            }
        }
#else
        klog("scheduler: thread added (%s)\n", t->name.c_str());
#endif
    }
}

inline void Scheduler::set_timeout(const Thread *new_t)
{
    // Get earliest timeout in anything with higher priority than new_t
    auto new_p = new_t->base_priority;
    kernel_time earliest_blocker;
    for(int i = new_p + 1; i < npriorities; i++)
    {
        if(earliest_blockers[i].is_valid())
        {
            if(!earliest_blocker.is_valid() || earliest_blockers[i] < earliest_blocker)
            {
                earliest_blocker = earliest_blockers[i];
            }
        }
    }

#if GK_OVERCLOCK
    unsigned const int sysclk = 656 / 8;
#else
    unsigned const int sysclk = 600 / 8;
#endif
    unsigned int reload = 0;

    if(earliest_blocker.is_valid())
    {
        // set a timer for then
        auto now = clock_cur();
        if(now >= earliest_blocker)
        {
            // re-run scheduler
            Yield();
            return;
        }
        else
        {
            auto tdiff = earliest_blocker - now;
            
            if(tdiff < kernel_time::from_us(GK_MAXTIMESLICE_US))
            {
                // core_clk/reload = 1/tdiff
                // reload = core * tdiff(s)
                reload = (unsigned int)(sysclk * tdiff.to_us());
            }
        }
    }
    if(reload == 0)
    {
        reload = sysclk * GK_MAXTIMESLICE_US;
    }
    reload--;

    // Max 24 bit systick timer
    if(reload > 0xffffffUL)
        reload = 0xffffffUL;

    SysTick->CTRL = 0UL;
    SysTick->LOAD = reload;
    SysTick->VAL = 0UL;
    SysTick->CTRL = 3UL;
}

Thread *Scheduler::GetNextThread(uint32_t ncore)
{
    CPUAffinity OnlyMe = (ncore == 0) ? CPUAffinity::M7Only : CPUAffinity::M4Only;
    CPUAffinity PreferMe = (ncore == 0) ? CPUAffinity::PreferM7 : CPUAffinity::PreferM4;

#if GK_DUAL_CORE
    CPUAffinity PreferOther = (ncore == 0) ? CPUAffinity::PreferM4 : CPUAffinity::PreferM7;
    int ct_ncore = ncore;
#else
    int ct_ncore = 0;
#endif

    Thread *cur_t;
    int cur_prio;
    {
        CriticalGuard cg;
        cur_t = current_thread[ct_ncore];
        cur_prio = (cur_t->get_is_blocking() || cur_t->for_deletion || 
            (cur_t->tss.running_on_core && (cur_t->tss.running_on_core != ((int)ncore + 1))) ||
            (cur_t->tss.pinned_on_core && (cur_t->tss.pinned_on_core != ((int)ncore + 1))) ||
            cur_t->tss.deschedule_from_core || cur_t->tss.chosen_for_core) ? 0 :
            current_thread[ct_ncore]->base_priority;
        cur_t->tss.deschedule_from_core = ncore + 1;
    }

#if GK_DYNAMIC_SYSTICK
    // Get earliest blocker at each priority level
    for(int i = 0; i < npriorities; i++)
    {
        kernel_time cur_earliest_blocker;
#if GK_DUAL_CORE
        CriticalGuard cg(tlist[i].m);
#endif

        for(const auto &tthread : tlist[i].v)
        {
            if(tthread->get_is_blocking() && tthread->block_until.is_valid())
            {
                if(tthread->block_until <= clock_cur())
                {
#if DEBUG_SCHEDULER
                    klog("sched: unblock %s due to timeout (%u >= %u)\n",
                        tthread->name.c_str(),
                        (uint32_t)clock_cur_us(),
                        (uint32_t)tthread->block_until.to_us());
#endif
                    tthread->set_is_blocking(false, true);
                    tthread->block_until = 0ULL;
                }
                else if(!cur_earliest_blocker.is_valid() || tthread->block_until < cur_earliest_blocker)
                {
                    cur_earliest_blocker = tthread->block_until;
                }
            }
        }

        earliest_blockers[i] = cur_earliest_blocker;
    }
#endif

    for(int i = npriorities-1; i >= cur_prio; i--)
    {
#if GK_DUAL_CORE
        CriticalGuard cg(tlist[i].m);
#endif

        if(tlist[i].v.empty())
        {
            continue;
        }

        auto vsize = static_cast<int>(tlist[i].v.size());

        // First get the front-most marked Either, OnlyMe or PreferMe
        auto cur_idx = tlist[i].index;

        for(int basei = 0; basei < vsize; basei++)
        {
            auto iter = (basei + cur_idx + 1) % vsize;
            auto cval = tlist[i].v[iter];
            cval = get_blocker(cval);
            {
                CriticalGuard cg2;
                if(cval->tss.running_on_core || cval->tss.chosen_for_core || cval->tss.deschedule_from_core ||
                    cval->for_deletion)
                {
                    continue;
                }
                if(cval->tss.pinned_on_core && cval->tss.pinned_on_core != ((int)ncore + 1))
                {
                    continue;
                }
                
                if(cval->get_is_blocking() == false &&
                    (cval->tss.affinity == CPUAffinity::Either ||
                    cval->tss.affinity == OnlyMe ||
                    cval->tss.affinity == PreferMe))
                {
                    tlist[i].index = iter;
                    cval->tss.chosen_for_core = ncore + 1;
#if DEBUG_SCHEDULER
                    report_chosen(cur_t, cval);
#endif
#if GK_DYNAMIC_SYSTICK
                    set_timeout(cval);
#endif
                    return cval;
                }
            }
        }

#if GK_DUAL_CORE
        // Now try ones marked PreferOther
        for(int basei = 0; basei < vsize; basei++)
        {
            auto iter = (basei + cur_idx + 1) % vsize;
            auto cval = tlist[i].v.v[iter];
            cval = get_blocker(cval);
            {
                CriticalGuard cg2(cval->sl);
                if(cval->tss.running_on_core || cval->tss.chosen_for_core || cval->tss.deschedule_from_core ||
                    cval->for_deletion)
                {
                    continue;
                }
                if(cval->tss.pinned_on_core && cval->tss.pinned_on_core != ((int)ncore + 1))
                {
                    continue;
                }
                
                if(cval->is_blocking == false &&
                    (cval->tss.affinity == PreferOther))
                {
                    tlist[i].v.index = iter;
                    cval->tss.chosen_for_core = ncore + 1;
#if DEBUG_SCHEDULER
                    report_chosen(cur_t, cval);
#endif
                    set_timeout(cval);
                    return cval;
                }
            }
        }
#endif
    }

    {
        // We didn't find any valid thread with equal or higher priority than the current one
        CriticalGuard cg;
#if DEBUG_SCHEDULER
        report_chosen(cur_t, current_thread[ct_ncore]);
#endif
        current_thread[ct_ncore]->tss.deschedule_from_core = 0;
#if GK_DYNAMIC_SYSTICK
        set_timeout(current_thread[ct_ncore]);
#endif
        return current_thread[ct_ncore];
    }
}

extern char _ecm4_stack;
extern char _ecm7_stack;

void Scheduler::StartForCurrentCore [[noreturn]] ()
{
    // Get MSP top (so it doesn't look like we're always running in ResetHandler forever)
    uint32_t msp_top = (uint32_t)(uintptr_t)&_ecm7_stack;
    
    __enable_irq();

    // #switch to first thread by triggering SVC which then triggers pendsv
    register unsigned int sno asm("r0") = syscall_no::StartFirstThread;
    __asm volatile
    (
        "msr msp, %1            \n"
        "svc #0                 \n"
        :: "r"(sno), "r"(msp_top)
    );

    // shouldn't get here
    while(true);
}

Thread *Scheduler::get_blocker(Thread *t)
{
    int iter = 0;

    while(true)
    {
        {
            CriticalGuard cg;
            unblock_delayer(t);
            if(t->blocking_on == nullptr || ((uint32_t)(uintptr_t)t->blocking_on) & 0x3U)
                return t;
            t = t->blocking_on;
        }
        if(iter++ > 256)
        {
            __asm__ volatile ("bkpt \n" ::: "memory");
        }
    }
}

void Scheduler::unblock_delayer(Thread *t)
{
    if(t->get_is_blocking() && t->block_until.is_valid() && clock_cur() >= t->block_until)
    {
#if DEBUG_SCHEDULER
        {
            klog("sched: awaken sleeping thread %s (%u -> %u)\n",
                t->name.c_str(),
                (uint32_t)clock_cur_us(),
                (uint32_t)t->block_until.to_us()
            );
        }
#endif
        t->set_is_blocking(false, true);
        t->block_until.invalidate();
        t->blocking_on = nullptr;
    }
    if(t->get_is_blocking() && (((uint32_t)(uintptr_t)t->blocking_on) & 0x3U) == 0x1U &&
        ((SimpleSignal *)(((uint32_t)(uintptr_t)t->blocking_on) & ~0x3U))->waiting_thread != t)
    {
        {
            klog("scheduler: spurious blocking on already triggered SimpleSignal for thread %s\n",
                t->name.c_str());
        }
        t->set_is_blocking(false, true);
        t->block_until.invalidate();
        t->blocking_on = nullptr;
    }
}

void Scheduler::report_chosen(Thread *old_t, Thread *new_t)
{
    CriticalGuard cg;
    klog("sched core %d: %s (%d) to %s (%d)\n",
        GetCoreID(),
        old_t->name.c_str(), old_t->base_priority,
        new_t->name.c_str(), new_t->base_priority);
}

void Block(kernel_time until, Thread *block_on)
{
    auto t = GetCurrentThreadForCore();
    {
        CriticalGuard cg;
        t->set_is_blocking(true);
        t->blocking_on = block_on;
        t->block_until = until;
    }
    Yield();
}

void Scheduler::Unschedule(Thread *t)
{
    for(int i = 0; i < npriorities; i++)
    {
        CriticalGuard cg;

        auto &v = tlist[i].v;

        auto iter = v.begin();
        while(iter != v.end())
        {
            if(*iter == t)
            {
                iter = v.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }
}

void Scheduler::ChangePriority(Thread *t, int old_p, int new_p)
{
    if(old_p < 0 || old_p >= npriorities)
        return;
    if(new_p < 0 || new_p >= npriorities)
        return;
    
    // Simultaneously lock thread and old/new priority lists
    CriticalGuard cg;

    auto &old_v = tlist[old_p].v;

    for(auto iter = old_v.begin(); iter != old_v.end(); iter++)
    {
        if(*iter == t)
        {
            // We found the thread in this scheduler instance, remove from old list, add to new
            old_v.erase(iter);
            auto &new_v = tlist[new_p].v;
            new_v.push_back(t);

            // update base_priority
            t->base_priority = new_p;

            // if currently running, and has lowered priority, need to yield
            if(new_p < old_p && t->tss.running_on_core)
            {
                #if GK_DUAL_CORE | GK_DUAL_CORE_AMP
                if(t->tss.running_on_core != GetCoreID() + 1)
                {
                    auto other_core = 1U - GetCoreID();
                    ipi_messages[other_core].Write({ ipi_message::ThreadUnblocked, nullptr, .t = t });
                    __SEV();
                }
                else
                #endif
                Yield();    // single core or dual core and running on this
            }

            return;
        }
    }
}

void Thread::set_is_blocking(bool val, bool is_sched)
{
    CriticalGuard cg;
#if DEBUG_SCHEDULER
    if(val)
    {
        klog("sched: thread %s blocked by %s\n", name.c_str(),
            is_sched ? "scheduler" :
            GetCurrentThreadForCore()->name.c_str());
    }
    else
    {
        klog("sched: thread %s unblocked by %s\n", name.c_str(),
            is_sched ? "scheduler" :
            GetCurrentThreadForCore()->name.c_str());
    }
#endif
    is_blocking = val;
}

bool Thread::get_is_blocking() const
{
    return is_blocking;
}
