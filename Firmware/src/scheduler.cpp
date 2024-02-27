#include "scheduler.h"
#include "syscalls.h"
#include "SEGGER_RTT.h"
#include "clocks.h"

#define DEBUG_SCHEDULER 0

extern Process kernel_proc;
__attribute__((section(".sram4"))) Thread dt(kernel_proc);

Scheduler::Scheduler()
{
    RCC->AHB4ENR |= RCC_AHB4ENR_HSEMEN;
    
    for(int i = 0; i < npriorities; i++)
    {
        tlist[i] = LockedIndexedThreadVector();
    }
    dt.name = "dummy";
    dt.base_priority = 0;

    SEGGER_RTT_printf(0, "&dt: %x\n", (uint32_t)(uintptr_t)&dt);
    SEGGER_RTT_printf(0, "&dt.tss: %x\n", (uint32_t)(uintptr_t)&dt.tss);

    for(int i = 0; i < ncores; i++)
    {
        CriticalGuard(current_thread[i].m);
        current_thread[i].v = &dt;
        scheduler_running[i] = false;
    }
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
        CriticalGuard cg(tlist[prio].m);
        tlist[prio].v.v.push_back(t);
    }
}

Thread *Scheduler::GetNextThread(uint32_t ncore)
{
    CPUAffinity OnlyMe = (ncore == 0) ? CPUAffinity::M7Only : CPUAffinity::M4Only;
    CPUAffinity PreferMe = (ncore == 0) ? CPUAffinity::PreferM7 : CPUAffinity::PreferM4;
    CPUAffinity PreferOther = (ncore == 0) ? CPUAffinity::PreferM4 : CPUAffinity::PreferM7;

    Thread *cur_t;
    int cur_prio;
    {
        CriticalGuard cg(current_thread[ncore].m);
        cur_t = current_thread[ncore].v;
        cur_prio = cur_t->is_blocking ? 0 :
            current_thread[ncore].v->base_priority;
    }

    for(int i = npriorities-1; i >= cur_prio; i--)
    {
        CriticalGuard cg(tlist[i].m);

        if(tlist[i].v.v.empty())
        {
            continue;
        }

        auto vsize = static_cast<int>(tlist[i].v.v.size());

        // First get the front-most marked Either, OnlyMe or PreferMe
        auto cur_idx = tlist[i].v.index;

        for(int basei = 0; basei < vsize; basei++)
        {
            auto iter = (basei + cur_idx + 1) % vsize;
            auto cval = tlist[i].v.v[iter];
            cval = get_blocker(cval);
            {
                CriticalGuard cg2(cval->sl);
                if(cval->running_on_core || cval->chosen_for_core)
                    continue;
                
                if(cval->is_blocking == false &&
                    (cval->affinity == CPUAffinity::Either ||
                    cval->affinity == OnlyMe ||
                    cval->affinity == PreferMe))
                {
                    tlist[i].v.index = iter;
                    cval->chosen_for_core = ncore + 1;
#if DEBUG_SCHEDULER
                    report_chosen(cur_t, cval);
#endif
                    return cval;
                }
            }
        }

        // Now try ones marked PreferOther
        for(int basei = 0; basei < vsize; basei++)
        {
            auto iter = (basei + cur_idx + 1) % vsize;
            auto cval = tlist[i].v.v[iter];
            cval = get_blocker(cval);
            {
                CriticalGuard cg2(cval->sl);
                if(cval->running_on_core || cval->chosen_for_core)
                    continue;
                
                if(cval->is_blocking == false &&
                    (cval->affinity == PreferOther))
                {
                    tlist[i].v.index = iter;
                    cval->chosen_for_core = ncore + 1;
#if DEBUG_SCHEDULER
                    report_chosen(cur_t, cval);
#endif
                    return cval;
                }
            }
        }
    }

    {
        // We didn't find any valid thread with equal or higher priority than the current one
        CriticalGuard cg(current_thread[ncore].m);
#if DEBUG_SCHEDULER
        report_chosen(cur_t, current_thread[ncore].v);
#endif
        return current_thread[ncore].v;
    }
}

void Scheduler::StartForCurrentCore [[noreturn]] ()
{
    SCB_CleanInvalidateDCache();
    __enable_irq();

    // #switch to first thread by triggering SVC which then triggers pendsv
    register unsigned int sno asm("r0") = syscall_no::StartFirstThread;
    __asm volatile
    (
        "svc #0                 \n"
        :: "r"(sno)
    );

    // shouldn't get here
    while(true);
}

Thread *Scheduler::get_blocker(Thread *t)
{
    while(true)
    {
        CriticalGuard cg(t->sl);
        unblock_delayer(t);
        if(t->blocking_on == nullptr)
            return t;
        t = t->blocking_on;
    }
}

extern Spinlock s_rtt;

void Scheduler::unblock_delayer(Thread *t)
{
    if(t->is_blocking && t->block_until && clock_cur_ms() >= t->block_until)
    {
#if DEBUG_SCHEDULER
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "%d: sched awaken sleeping thread %s\n",
                (uint32_t)clock_cur_ms(),
                t->name.c_str());
        }
#endif
        t->is_blocking = false;
        t->block_until = 0;
        t->blocking_on = nullptr;
    }
}

void Scheduler::report_chosen(Thread *old_t, Thread *new_t)
{
    CriticalGuard cg(s_rtt);
    SEGGER_RTT_printf(0, "%d: sched: %s (%d) to %s (%d)\n", (uint32_t)clock_cur_ms(),
        old_t->name.c_str(), old_t->base_priority,
        new_t->name.c_str(), new_t->base_priority);
}
