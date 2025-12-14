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
#include "gk_conf.h"

static constexpr uint32_t thread_signal_lwext = 0x1;

class Process;

class Thread
{
    public:
        thread_saved_state tss;

        class blocking_t
        {
            public:
                Spinlock sl{};
                bool b_indefinite = false;
                kernel_time b_until = kernel_time_invalid();
                id_t b_thread = 0;
#if GK_DEBUG_BLOCKING
                Condition *b_condition = nullptr;
                SimpleSignal *b_ss = nullptr;
                UserspaceSemaphore *b_uss = nullptr;
                RwLock *b_rwl = nullptr;
                void *b_queue = nullptr;
#else
                bool b_prim = false;
#endif

                bool is_blocking(kernel_time *tout = nullptr,
                    PThread *t = nullptr);
                void unblock();
                void block(PThread t, kernel_time tout = kernel_time_invalid());
                void block(Condition *c, kernel_time tout = kernel_time_invalid());
                void block(SimpleSignal *ss, kernel_time tout = kernel_time_invalid());
                void block(UserspaceSemaphore *uss, kernel_time tout = kernel_time_invalid());
                void block(RwLock *uss, kernel_time tout = kernel_time_invalid());
                void block(void *q, kernel_time tout = kernel_time_invalid());
                void block(kernel_time tout);
                void block_indefinite();
        };

        blocking_t blocking;

        // Generic spinlock for everything else (name, for_deletion etc)
        Spinlock sl{};

        std::shared_ptr<Process> p;
        std::string name;
        id_t id;

        int base_priority;
        bool is_privileged;

        VMemBlock mr_kernel_thread = InvalidVMemBlock();
        VMemBlock mr_user_thread = InvalidVMemBlock();
        VMemBlock mr_elf_tls = InvalidVMemBlock();

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
        id_t join_thread = 0;
        void **join_thread_retval;

        /* cleanup stuff */
        bool for_deletion = false;

        /* Stores the thread to which we are temporarily assuming the lower half */
        Spinlock sl_lower_half_user_thread{};
        PThread lower_half_user_thread = nullptr;

        typedef void *(*threadstart_t)(void *p);
        static std::shared_ptr<Thread> Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process,
            void *p2 = nullptr);

        /* determine if a given address range is within the memory accessible by
            this thread */
        bool addr_is_valid(const void *buf, size_t len, bool for_write = false) const;

        /* If privileged thread, assume the same lower half as the specified user thread */
        int assume_user_thread_lower_half(PThread user_thread);

        /* If privileged thread, release user thread lower half */
        int release_user_thread_lower_half();

        ~Thread();
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

/* Grants the current thread escalated privileges for the duration of the guards existance */
class ThreadPrivilegeEscalationGuard
{
    protected:
        bool old_priv;

    public:
        ThreadPrivilegeEscalationGuard();
        ~ThreadPrivilegeEscalationGuard();

        ThreadPrivilegeEscalationGuard(const ThreadPrivilegeEscalationGuard &) = delete;
};

#endif
