#ifndef THREAD_H
#define THREAD_H

#include <memory>
#include "ostypes.h"
#include "osmutex.h"
#include "kernel_time.h"
#include "syscalls.h"

class Process;

class Thread
{
    public:
        thread_saved_state tss;

        Spinlock sl_blocking;
        bool is_blocking = false;
        WPThread blocking_on_thread;
        void *blocking_on_prim = nullptr;
        kernel_time block_until;

        inline bool get_is_blocking() const { return is_blocking; }
        inline void set_is_blocking(bool val, bool = false) { is_blocking = val; }

        std::shared_ptr<Process> p;
        std::string name;

        int base_priority;

        VMemBlock mr_kernel_thread;
        VMemBlock mr_user_thread;

        void *thread_retval;

        /* Used for waiting on inter-process RPC returns */
        SimpleSignal ss;
        WaitSimpleSignal_params ss_p;

        typedef void *(*threadstart_t)(void *p);
        static std::shared_ptr<Thread> Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process);
};

static inline Thread *GetCurrentKernelThreadForCore()
{
    Thread *ret;
    __asm__ volatile("mrs %[ret], tpidr_el1\n" : [ret] "=r" (ret));
    return ret;
}

static inline Thread *GetCurrentUserThreadForCore()
{
    Thread *ret;
    __asm__ volatile("mrs %[ret], tpidr_el0\n" : [ret] "=r" (ret));
    return ret;
}

static inline Thread *GetCurrentThreadForCore()
{
    return GetCurrentKernelThreadForCore();
}

#endif
