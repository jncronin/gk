#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "thread.h"
#include "osmutex.h"
#include <vector>

class Scheduler
{
    public:
        constexpr static const int npriorities = GK_NPRIORITIES;
#if GK_DUAL_CORE
        constexpr static const int ncores = 2;
#else
        constexpr static const int ncores = 1;
#endif

    protected:
        template <class T> struct locked_val
        {
            T v;
            Spinlock m;
        };
        struct IndexedThreadVector
        {
            std::vector<PThread> v;
            int index = -1;
        };
        using LockedIndexedThreadVector = locked_val<IndexedThreadVector>;
        using LockedThreadVector = locked_val<std::vector<PThread>>;

        LockedIndexedThreadVector tlist[npriorities];
        LockedThreadVector sleeping_tasks;
        LockedThreadVector blocking_tasks;

        /* Follows the chain of 'blocking_on' to allow priority escalation */
        PThread get_blocker(PThread t);

        /* Unblocks threads waiting on a certain delay */
        void unblock_delayer(PThread t);

        /* Report chosen thread */
        void report_chosen(PThread &old_t, PThread &new_t);

    public:
        Scheduler();
        Scheduler(const Scheduler &) = delete;

        void Schedule(PThread t);
        void TimerTick(uint32_t nticks);
        void Unsleep(PThread t);
        void Unblock(PThread t);
        void Block(PThread t);
        void Sleep(PThread t, uint32_t nticks);
        PThread GetNextThread(uint32_t ncore);

        void StartForCurrentCore [[noreturn]] ();

        using LockedThread = locked_val<PThread>;
        LockedThread current_thread[ncores];

        bool scheduler_running[ncores];
};

// Called from PendSV therefore not mangled
extern "C" {
    Thread *GetCurrentThreadForCore_int(int coreid = -1);
    Thread *GetNextThreadForCore(int coreid = -1);
    int GetCoreID();
    void SetNextThreadForCore(Thread *t, int coreid = -1);
    void ScheduleThread(Thread *t);
}

PThread GetCurrentThreadForCore(int coreid = -1);

void Schedule(PThread t);

#if GK_DUAL_CORE_AMP
extern Scheduler scheds[2];
inline Scheduler &s() { return scheds[GetCoreID()]; }
inline bool &scheduler_running() { return s().scheduler_running[0]; }
inline Scheduler::LockedThread &current_thread() { return s().current_thread[0]; }
inline Scheduler::LockedThread &current_thread(int coreid) { return scheds[coreid].current_thread[0]; }
#else
extern Scheduler sched;
inline Scheduler &s() { return sched; }
inline bool &scheduler_running() { return s().scheduler_running[GetCoreID()]; }
inline Scheduler::LockedThread &current_thread() { return s().current_thread[GetCoreID()]; }
inline Scheduler::LockedThread &current_thread(int coreid) { return sched.current_thread[coreid]; }
#endif

static inline void Yield()
{
    if(scheduler_running())
    {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

void Block(uint64_t until = UINT64_MAX, PThread block_on = nullptr);

#endif
