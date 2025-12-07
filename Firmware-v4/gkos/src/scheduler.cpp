#include "scheduler.h"
#include "clocks.h"
#include "gk_conf.h"
#include "kernel_time.h"
#include "cpu.h"

#define DEBUG_SCHEDULER 0

Scheduler sched;

Scheduler::Scheduler()
{
    for(int i = 0; i < npriorities; i++)
    {
        new (&tlist[i]) IndexedThreadVector();
    }
    for(unsigned int i = 0; i < ncores; i++)
    {
        new (&current_thread[i]) PThread;
        new (&next_thread[i]) PThread;
        new (&idle_threads[i]) PThread;
        scheduler_running[i] = false;
    }
}

void Schedule(PThread t)
{
    sched.Schedule(t);
}

void Scheduler::Schedule(PThread t)
{
    if(!t)
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
        CriticalGuard cg(tlist[prio].sl);
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

inline void Scheduler::set_timeout(const PThread new_t)
{
    const uint32_t unmask_val = 0x1;

    // Get earliest timeout in anything with higher priority than new_t
    auto new_p = new_t->base_priority;
    kernel_time earliest_blocker = kernel_time_invalid();
    for(int i = new_p + 1; i < npriorities; i++)
    {
        if(kernel_time_is_valid(earliest_blockers[i]))
        {
            if(!kernel_time_is_valid(earliest_blocker) || earliest_blockers[i] < earliest_blocker)
            {
                earliest_blocker = earliest_blockers[i];
            }
        }
    }

    const unsigned int sysclk = 64;   // 1 us = 64 ticks @ 64 MHz
    unsigned int reload = 0;

    if(kernel_time_is_valid(earliest_blocker))
    {
        // set a timer for then
        auto now = kernel_time(clock_cur());
        if(now >= earliest_blocker)
        {
            // re-run scheduler
            // set a timer just in case
            __asm__ volatile(
                "msr cntp_tval_el0, %[delay]\n" 
                "msr cntp_ctl_el0, %[unmask_timer]\n" : :
                [delay] "r" (sysclk * GK_MAXTIMESLICE_US),
                [unmask_timer] "r" (unmask_val) : "memory");
            Yield();
            return;
        }
        else
        {
            auto tdiff = earliest_blocker - now;
            
            if(tdiff < kernel_time_from_us(GK_MAXTIMESLICE_US))
            {
                // core_clk/reload = 1/tdiff
                // reload = core * tdiff(s)
                reload = (unsigned int)(sysclk * kernel_time_to_us(tdiff));
            }
        }
    }
    if(reload == 0)
    {
        reload = sysclk * GK_MAXTIMESLICE_US;
    }

#if DEBUG_SCHEDULER
    if(unmask_val == 0x1)
        klog("sched: setting delay for %llu ticks\n", reload);
#endif
    __asm__ volatile(
        "msr cntp_tval_el0, %[delay]\n" 
        "msr cntp_ctl_el0, %[unmask_timer]\n" : :
        [delay] "r" (reload),
        [unmask_timer] "r" (unmask_val) : "memory");
}

Thread *Scheduler::GetNextThread(uint32_t ncore)
{
    //CPUAffinity OnlyMe = (ncore == 0) ? CPUAffinity::M7Only : CPUAffinity::M4Only;
    //CPUAffinity PreferMe = (ncore == 0) ? CPUAffinity::PreferM7 : CPUAffinity::PreferM4;

    PThread cur_t;
    int cur_prio;
    bool cur_blocking;

    // Get the priority of the currently running thread, or 0 if it is blocking
    {
        CriticalGuard cg(sl_cur_next);
        cur_t = current_thread[ncore];
        cur_blocking = (cur_t == nullptr) ? true : cur_t->blocking.is_blocking();
        cur_prio = cur_blocking ? 0 : cur_t->base_priority;
    }

#if GK_DYNAMIC_SYSTICK
    // Get earliest blocker at each priority level
    for(int i = 0; i < npriorities; i++)
    {
        kernel_time cur_earliest_blocker = kernel_time_invalid();
        CriticalGuard cg(tlist[i].sl);

        for(auto tthread : tlist[i].v)
        {
            kernel_time tthread_tout;
            if(tthread->blocking.is_blocking(&tthread_tout))
            {
                if(kernel_time_is_valid(tthread_tout))
                {
                    if(!kernel_time_is_valid(cur_earliest_blocker) ||
                        tthread_tout < cur_earliest_blocker)
                    {
                        cur_earliest_blocker = tthread_tout;
                    }                
                }
            }
        }

        earliest_blockers[i] = cur_earliest_blocker;
    }
#endif

    // now select a thread of higher priority than we currently have
    for(int i = npriorities-1; i >= cur_prio; i--)
    {
        CriticalGuard cg(tlist[i].sl);

        if(tlist[i].v.empty())
        {
            continue;
        }

        auto vsize = static_cast<int>(tlist[i].v.size());

        // Get the front most thread at highest priority
        auto cur_idx = tlist[i].index;

        for(int basei = 0; basei < vsize; basei++)
        {
            auto iter = (basei + cur_idx + 1) % vsize;
            auto cval = tlist[i].v[iter];

            bool is_blocking;
            std::tie(cval, is_blocking) = get_blocker(cval);

            if(!is_blocking)
            {
                bool can_pick = true;
                {
                    CriticalGuard cg2(sl_cur_next);
                    // check we are not scheduling something already chosen for another core
                    for(unsigned int core = 0U; core < ncores; core++)
                    {
                        if(current_thread[core] == cval || next_thread[core] == cval)
                        {
                            can_pick = false;
                            break;
                        }
                    }

                    if(can_pick)
                    {
                        next_thread[ncore] = cval;

                        tlist[i].index = iter;
#if GK_DYNAMIC_SYSTICK
                        set_timeout(cval);
#else
                        // default timeslice
                        __asm__ volatile(
                            "msr cntp_tval_el0, %[delay]\n" 
                            "msr cntp_ctl_el0, %[unmask_timer]\n" : :
                            [delay] "r" (64 * GK_MAXTIMESLICE_US),
                            [unmask_timer] "r" (0x1) : "memory");
#endif
                        return cval.get();
                    }
                }
            }
        }
    }

    // We didn't find any valid thread with equal or higher priority than the current one

    // If current is blocking return the idle thread, else return the current one again
    auto new_t = cur_blocking ? idle_threads[ncore] : cur_t;

    if(new_t != cur_t)
    {
        CriticalGuard cg(sl_cur_next);
        next_thread[ncore] = new_t;
    }

#if GK_DYNAMIC_SYSTICK
    set_timeout(new_t);
#else
    // default timeslice
    __asm__ volatile(
        "msr cntp_tval_el0, %[delay]\n" 
        "msr cntp_ctl_el0, %[unmask_timer]\n" : :
        [delay] "r" (64 * GK_MAXTIMESLICE_US),
        [unmask_timer] "r" (0x1) : "memory");
#endif
    return new_t.get();
}

extern char _ecm4_stack;
extern char _ecm7_stack;

static void *idle_thread(void *)
{
    while(true)
    {
        __asm__ volatile("wfi \n" ::: "memory");
    }
}

void Scheduler::StartForCurrentCore [[noreturn]] ()
{
    auto core = GetCoreID();

    // Create the idle thread for the current core
    extern PProcess p_kernel;
    idle_threads[core] = Thread::Create("idle_" + std::to_string(core),
        idle_thread, nullptr, true, GK_PRIORITY_IDLE, p_kernel);

    scheduler_running[core] = true;

    // Set up the appropriate virtual memory layout for the core
    cpu_setup_vmem();
    cpu_start_local_timer();

    // switch to first thread by triggering SVC1
    __asm__ volatile(
        "msr tpidr_el1, xzr\n"
        "msr tpidr_el0, xzr\n"
        "msr daifclr, #0xf\n"
        "svc #1\n" ::: "memory");

    // shouldn't get here
    while(true);
}

std::pair<PThread, bool> Scheduler::get_blocker(PThread t)
{
    int iter = 0;

    while(true)
    {
        PThread next_t;
        auto is_b = t->blocking.is_blocking(nullptr, &next_t);
        if(!is_b)
        {
            return std::make_pair(t, false);
        }
        else
        {
            // is blocking.  Is there a thread chain to follow?
            if(next_t)
            {
                // continue
                t = next_t;
            }
            else
            {
                // blocking on something else - fail
                return std::make_pair(t, true);
            }
        }
        
        if(iter++ > 256)
        {
            __asm__ volatile ("brk #255\n" ::: "memory");
        }
    }
}

#if 0
void Scheduler::report_chosen(Thread *old_t, Thread *new_t)
{
    CriticalGuard cg;
    klog("sched core %d: %s (%d) to %s (%d)\n",
        GetCoreID(),
        old_t->name.c_str(), old_t->base_priority,
        new_t->name.c_str(), new_t->base_priority);
}
#endif

void Block(kernel_time until, PThread block_on)
{
    auto t = GetCurrentThreadForCore();
    if(kernel_time_is_valid(until) == false && !block_on)
    {
        t->blocking.block_indefinite();
    }
    else if(block_on)
    {
        t->blocking.block(block_on, until);
    }
    else
    {
        t->blocking.block(until);
    }
    Yield();
}

void Scheduler::Unschedule(PThread t)
{
    for(int i = 0; i < npriorities; i++)
    {
        CriticalGuard cg(tlist[i].sl);

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

void Scheduler::ChangePriority(PThread t, int old_p, int new_p)
{
    if(old_p < 0 || old_p >= npriorities)
        return;
    if(new_p < 0 || new_p >= npriorities)
        return;
    if(old_p == new_p)
        return;
    
    // Simultaneously lock thread and old/new priority lists
    CriticalGuard cg(tlist[old_p].sl, tlist[new_p].sl);

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

            // if currently running, and has lowered priority, need to yield.  TODO pass message to other core if necessary
            if(new_p < old_p && t.get() == GetCurrentThreadForCore())
            {
                #if 0
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

void Scheduler::SetNextThread(uint32_t ncore, Thread *t)
{
    CriticalGuard cg(sl_cur_next);
    if(t != next_thread[ncore].get())
    {
        // shouldn't get here
        klog("sched: next thread inconsistency\n");
        __asm__ ("brk #254\n" ::: "memory");
        return;
    }
    current_thread[ncore] = next_thread[ncore];
    next_thread[ncore] = nullptr;
}

Thread *GetNextThreadForCore(uint32_t iar, void *, uint32_t irq)
{
    auto ret = sched.GetNextThread(GetCoreID());
#if DEBUG_SCHEDULER
    klog("sched: get_next_thread_for_core(%u): switch due to %x returning thread: %llx (%s), sp_el1: %llx\n, tss: %llx\n",
        GetCoreID(), iar,
        (uint64_t)ret, ret->name.c_str(), ret->tss.sp_el1, (uint64_t)&ret->tss);
#endif
    return ret;
}

void SetNextThreadForCore(Thread *t)
{
    sched.SetNextThread(GetCoreID(), t);
}

PThread &Scheduler::GetCurThread(uint32_t ncore)
{
    CriticalGuard cg(sl_cur_next);
    return current_thread[ncore];
}
