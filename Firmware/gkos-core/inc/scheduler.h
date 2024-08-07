#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "gk_conf.h"
#include "region_allocator.h"
#include "thread.h"
#include "osmutex.h"

class GKOS_FUNC(Scheduler)
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
            GKOS_FUNC(Spinlock) m;
        };
        struct IndexedThreadVector
        {
            GKOS_FUNC(SRAM4Vector)<Thread *> v;
            int index = -1;
        };
        using LockedIndexedThreadVector = GKOS_FUNC(locked_val)<IndexedThreadVector>;
        using LockedThreadVector = GKOS_FUNC(locked_val)<SRAM4Vector<Thread *>>;

        LockedIndexedThreadVector tlist[npriorities];

        /* Follows the chain of 'blocking_on' to allow priority escalation */
        GKOS_FUNC(Thread) *get_blocker(GKOS_FUNC(Thread) *t);

        /* Unblocks threads waiting on a certain delay */
        void unblock_delayer(GKOS_FUNC(Thread) *t);

        /* Report chosen thread */
        void report_chosen(GKOS_FUNC(Thread) *old_t, GKOS_FUNC(Thread) *new_t);

    public:
        GKOS_FUNC(Scheduler)();
        GKOS_FUNC(Scheduler)(const GKOS_FUNC(Scheduler) &) = delete;

        void Schedule(GKOS_FUNC(Thread) *t);
        void Unschedule(GKOS_FUNC(Thread) *t);
        void TimerTick(uint32_t nticks);
        void Unsleep(GKOS_FUNC(Thread) *t);
        void Unblock(GKOS_FUNC(Thread) *t);
        void Block(GKOS_FUNC(Thread) *t);
        void Sleep(GKOS_FUNC(Thread) *t, uint32_t nticks);
        GKOS_FUNC(Thread) *GetNextThread(uint32_t ncore);

        void StartForCurrentCore [[noreturn]] ();

        using LockedThread = locked_val<GKOS_FUNC(Thread) *>;
        LockedThread current_thread[ncores];

        bool scheduler_running[ncores];

#if GK_TICKLESS
        volatile uint64_t block_until = 0ULL;
#endif
};

void Schedule(GKOS_FUNC(Thread) *t);

#if IS_CM7_ONLY
#define GetCoreID() 0
#elif IS_CM4_ONLY
#define GetCoreID() 1
#else
#define GetCoreID() GKOS_FUNC(GetCoreID)()
#endif

// Called from PendSV therefore not mangled
extern "C" {
    GKOS_FUNC(Thread) *GKOS_FUNC(GetCurrentThreadForCore)(int coreid = -1);
    GKOS_FUNC(Thread) *GKOS_FUNC(GetNextThreadForCore)(int coreid = -1);
    int GKOS_FUNC(GetCoreID)();
    void GKOS_FUNC(SetNextThreadForCore)(GKOS_FUNC(Thread) *t, int coreid = -1);
    void GKOS_FUNC(ScheduleThread)(GKOS_FUNC(Thread) *t);
}

#if GK_DUAL_CORE_AMP
extern GKOS_FUNC(Scheduler) scheds[2];
inline GKOS_FUNC(Scheduler) &s() { return scheds[GetCoreID()]; }
inline bool &scheduler_running() { return s().scheduler_running[0]; }
inline GKOS_FUNC(Scheduler)::LockedThread &current_thread() { return s().current_thread[0]; }
inline GKOS_FUNC(Scheduler)::LockedThread &current_thread(int coreid) { return scheds[coreid].current_thread[0]; }
#else
extern GKOS_FUNC(Scheduler) sched;
inline GKOS_FUNC(Scheduler) &s() { return sched; }
inline bool &scheduler_running() { return s().scheduler_running[GetCoreID()]; }
inline GKOS_FUNC(Scheduler)::LockedThread &current_thread() { return s().current_thread[GetCoreID()]; }
inline GKOS_FUNC(Scheduler)::LockedThread &current_thread(int coreid) { return sched.current_thread[coreid]; }
#endif

static inline void Yield()
{
    if(scheduler_running())
    {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

void GKOS_FUNC(Block)(kernel_time until = kernel_time(), GKOS_FUNC(Thread) *block_on = nullptr);

#endif
