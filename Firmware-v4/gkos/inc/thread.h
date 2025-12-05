#ifndef THREAD_H
#define THREAD_H

#include <memory>
#include <map>
#include <vector>
#include "ostypes.h"
#include "osmutex.h"
#include "kernel_time.h"
#include "syscalls.h"
#include "sync_primitive_locks.h"

static constexpr uint32_t thread_signal_lwext = 0x1;

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
        inline void set_is_blocking(bool val, bool = false)
        {
            is_blocking = val;
            blocking_on_prim = nullptr;
            blocking_on_thread = WPThread{};
            block_until = kernel_time_invalid();
        }

        std::shared_ptr<Process> p;
        std::string name;
        id_t id;

        int base_priority;
        bool is_privileged;

        VMemBlock mr_kernel_thread;
        VMemBlock mr_user_thread;

        void *thread_retval;

        /* Used for waiting on inter-process RPC returns */
        SimpleSignal ss;
        WaitSimpleSignal_params ss_p;

        /* Userpsace sync primitives locked by this thread */
        locked_sync_list<Mutex> locked_mutexes;
        locked_sync_list<RwLock> locked_rwlocks;

        /* pthread TLS data */
        Spinlock sl_pthread_tls;
        std::map<pthread_key_t, void *> tls_data;
        /* pthread cleanup handlers - not fully implemented - need to run in process userspace context */
        struct cleanup_handler
        {
            void (*routine)(void *);
            void *arg;
        };
        std::vector<cleanup_handler> cleanup_list;

        /* return value, or pointers to a waiting thread and where it wants the retval placed */
        void *retval;
        WPThread join_thread{};
        void **join_thread_retval;

        /* cleanup stuff */
        bool for_deletion = false;

        /* Stores the thread to which we are temporarily assuming the lower half */
        PThread lower_half_user_thread = nullptr;

        typedef void *(*threadstart_t)(void *p);
        static std::shared_ptr<Thread> Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process);

        /* determine if a given address range is within the memory accessible by
            this thread */
        bool addr_is_valid(const void *buf, size_t len, bool for_write = false) const;

        /* If privileged thread, assume the same lower half as the specified user thread */
        int assume_user_thread_lower_half(PThread user_thread);

        /* If privileged thread, release user thread lower half */
        int release_user_thread_lower_half();
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
