#include "scheduler.h"
#include "syscalls.h"

Scheduler::Scheduler()
{
    RCC->AHB4ENR |= RCC_AHB4ENR_HSEMEN;
    
    for(int i = 0; i < npriorities; i++)
    {
        tlist[i] = LockedIndexedThreadVector();
    }
    dummy_thread.name = "dummy";
    dummy_thread.is_dummy = true;
    dummy_thread.base_priority = 0;
    for(int i = 0; i < ncores; i++)
    {
        CriticalGuard(current_thread[i].m);
        current_thread[i].v = &dummy_thread;
    }
}

void Scheduler::Schedule(Thread *t)
{
    if(!t || t->is_dummy)
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

    int cur_prio;
    {
        CriticalGuard cg(current_thread[ncore].m);
        cur_prio = current_thread[ncore].v->is_blocking ? 0 :
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
                    return cval;
                }
            }
        }

        // Now try ones marked PreferOther
        for(int basei = 0; basei < vsize; basei++)
        {
            auto iter = (basei + cur_idx + 1) % vsize;
            auto cval = tlist[i].v.v[iter];
            {
                CriticalGuard cg2(cval->sl);
                if(cval->running_on_core || cval->chosen_for_core)
                    continue;
                
                if(cval->is_blocking == false &&
                    (cval->affinity == PreferOther))
                {
                    tlist[i].v.index = iter;
                    cval->chosen_for_core = ncore + 1;
                    return cval;
                }
            }
        }
    }

    {
        // We didn't find any valid thread with equal or higher priority than the current one
        CriticalGuard cg(current_thread[ncore].m);
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
