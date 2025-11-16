#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "gk_conf.h"
#include "thread.h"
#include "osmutex.h"
#include "kernel_time.h"

#include <vector>

class Scheduler
{
    public:
        constexpr static const int npriorities = GK_NPRIORITIES;
        constexpr static const unsigned int ncores = GK_NUM_CORES;

    protected:
        struct IndexedThreadVector
        {
            std::vector<PThread> v;
            int index = -1;
            Spinlock sl;
        };

        PThread idle_threads[ncores];

        IndexedThreadVector tlist[npriorities];

        /* Follows the chain of 'blocking_on' to allow priority escalation */
        std::pair<PThread, bool> get_blocker(PThread unlocked_t);

        /* Unblocks threads waiting on a certain delay */
        void unblock_delayer(Thread *locked_t);

        /* Report chosen thread */
        void report_chosen(PThread old_t, PThread new_t);

        /* Set new timeout value based upon high priority threads which will unblock before the current
            scheduling interval */
        kernel_time earliest_blockers[npriorities];
        inline void set_timeout(const PThread new_t);

    public:
        Scheduler();
        Scheduler(const Scheduler &) = delete;

        void Schedule(PThread t);
        void Unschedule(PThread t);
        Thread *GetNextThread(uint32_t ncore);
        void SetNextThread(uint32_t ncore, Thread *);

        void StartForCurrentCore [[noreturn]] ();

        void ChangePriority(PThread t, int old_priority, int new_priority);

        Spinlock sl_cur_next;
        PThread current_thread[ncores];
        PThread next_thread[ncores];

        bool scheduler_running[ncores];

#if GK_TICKLESS
        volatile uint64_t block_until = 0ULL;
#endif
};

void Schedule(PThread t);

// Called from PendSV therefore not mangled
extern "C" {
    Thread *GetNextThreadForCore();
    void SetNextThreadForCore(Thread *t);
}

extern Scheduler sched;
inline Scheduler &s() { return sched; }

static inline unsigned int GetCoreID()
{
    uint64_t mpidr_el1;
    __asm__ volatile("mrs %[mpidr_el1], mpidr_el1\n" : [mpidr_el1] "=r" (mpidr_el1));
    return (unsigned int)(mpidr_el1 & 0xffU);
}

inline bool &scheduler_running() { return s().scheduler_running[GetCoreID()]; }
inline PThread current_thread() { return s().current_thread[GetCoreID()]; }
inline PThread current_thread(int coreid) { return sched.current_thread[coreid]; }

static inline void Yield()
{
    if(scheduler_running())
    {
        __asm__ volatile("svc #1\n" ::: "memory");
    }
}

void Block(kernel_time until = kernel_time(), Thread *block_on = nullptr);

#endif
