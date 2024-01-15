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
        using ThreadVector = locked_val<SRAM4Vector<Thread *>>;

        ThreadVector tlist[npriorities];
        ThreadVector sleeping_tasks;
        ThreadVector blocking_tasks;

        Thread dummy_thread;        // used to ensure the first task switch writes saved values somewhere

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

        void StartForCurrentCore();

        using LockedThread = locked_val<Thread *>;
        LockedThread current_thread[ncores];
};


#endif
