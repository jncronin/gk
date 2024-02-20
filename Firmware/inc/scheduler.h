#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "region_allocator.h"
#include "thread.h"
#include "osmutex.h"

class Scheduler
{
    public:
        constexpr static const int npriorities = 10;
        constexpr static const int ncores = 2;

    protected:
        template <class T> struct locked_val
        {
            T v;
            Spinlock m;
        };
        struct IndexedThreadVector
        {
            SRAM4Vector<Thread *> v;
            int index = -1;
        };
        using LockedIndexedThreadVector = locked_val<IndexedThreadVector>;
        using LockedThreadVector = locked_val<SRAM4Vector<Thread *>>;

        LockedIndexedThreadVector tlist[npriorities];
        LockedThreadVector sleeping_tasks;
        LockedThreadVector blocking_tasks;

        /* Follows the chain of 'blocking_on' to allow priority escalation */
        Thread *get_blocker(Thread *t);

    public:
        Scheduler();
        Scheduler(const Scheduler &) = delete;

        void Schedule(Thread *t);
        void TimerTick(uint32_t nticks);
        void Unsleep(Thread *t);
        void Unblock(Thread *t);
        void Block(Thread *t);
        void Sleep(Thread *t, uint32_t nticks);
        Thread *GetNextThread(uint32_t ncore);

        void StartForCurrentCore [[noreturn]] ();

        using LockedThread = locked_val<Thread *>;
        LockedThread current_thread[ncores];

        bool scheduler_running[ncores];
};

extern Scheduler s;
static inline void Yield()
{
    if(s.scheduler_running[GetCoreID()])
    {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}


#endif
