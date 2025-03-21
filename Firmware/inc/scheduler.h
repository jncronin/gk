#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "gk_conf.h"
#include "region_allocator.h"
#include "thread.h"
#include "osmutex.h"

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
        struct IndexedThreadVector
        {
            std::vector<Thread *> v;
            int index = -1;
        };

        IndexedThreadVector tlist[npriorities];

        /* Follows the chain of 'blocking_on' to allow priority escalation */
        Thread *get_blocker(Thread *t);

        /* Unblocks threads waiting on a certain delay */
        void unblock_delayer(Thread *t);

        /* Report chosen thread */
        void report_chosen(Thread *old_t, Thread *new_t);

        /* Set new timeout value based upon high priority threads which will unblock before the current
            scheduling interval */
        kernel_time earliest_blockers[npriorities];
        inline void set_timeout(const Thread *new_t);

    public:
        Scheduler();
        Scheduler(const Scheduler &) = delete;

        void Schedule(Thread *t);
        void Unschedule(Thread *t);
        void TimerTick(uint32_t nticks);
        void Unsleep(Thread *t);
        void Unblock(Thread *t);
        void Block(Thread *t);
        void Sleep(Thread *t, uint32_t nticks);
        Thread *GetNextThread(uint32_t ncore);

        void StartForCurrentCore [[noreturn]] ();

        void ChangePriority(Thread *t, int old_priority, int new_priority);

        Thread *current_thread[ncores];

        bool scheduler_running[ncores];

#if GK_TICKLESS
        volatile uint64_t block_until = 0ULL;
#endif
};

void Schedule(Thread *t);

// Called from PendSV therefore not mangled
extern "C" {
    Thread *GetCurrentThreadForCore(int coreid = -1);
    Thread *GetNextThreadForCore(int coreid = -1);
    int GetCoreID();
    void SetNextThreadForCore(Thread *t, int coreid = -1);
    void ScheduleThread(Thread *t);
}

#if GK_DUAL_CORE_AMP
extern Scheduler scheds[2];
inline Scheduler &s() { return scheds[GetCoreID()]; }
inline bool &scheduler_running() { return s().scheduler_running[0]; }
inline Scheduler::LockedThread &current_thread() { return s().current_thread[0]; }
inline Scheduler::LockedThread &current_thread(int coreid) { return scheds[coreid].current_thread[0]; }
#else
extern Scheduler sched;
inline Scheduler &s() { return sched; }

#if GK_DUAL_CORE
inline bool &scheduler_running() { return s().scheduler_running[GetCoreID()]; }
inline Scheduler::LockedThread &current_thread() { return s().current_thread[GetCoreID()]; }
inline Scheduler::LockedThread &current_thread(int coreid) { return sched.current_thread[coreid]; }
#else
inline bool &scheduler_running() { return s().scheduler_running[0]; }
inline Thread * &current_thread() { return s().current_thread[0]; }
inline Thread * &current_thread(int coreid) { return s().current_thread[0]; }
#endif

#endif

static inline void Yield()
{
    if(scheduler_running())
    {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

void Block(kernel_time until = kernel_time(), Thread *block_on = nullptr);

#endif
