#ifndef THREAD_H
#define THREAD_H

#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <osmutex.h>
#include "memblk.h"
#include "ostypes.h"
#include "osmutex.h"

#include "mpuregions.h"

#include "syscalls.h"

static constexpr uint32_t thread_signal_lwext = 0x1;

#define GK_PRIORITY_IDLE    0
#define GK_PRIORITY_LOW     1
#define GK_PRIORITY_NORMAL  2
#define GK_PRIORITY_GAME    GK_PRIORITY_NORMAL
#define GK_PRIORITY_APP     GK_PRIORITY_NORMAL
#define GK_PRIORITY_HIGH    3
#define GK_PRIORITY_VHIGH   4
#define GK_PRIORITY_VERYHIGH    GK_PRIORITY_VHIGH

#define GK_NPRIORITIES      (GK_PRIORITY_VHIGH + 1)

class Process;
class Thread;

using PThread = std::shared_ptr<Thread>;
using PProcess = std::shared_ptr<Process>;

#define BLOCKING_ON_SS(x)           ((void *)(((uint32_t)(uintptr_t)(x)) | 0x1U))
#define BLOCKING_ON_QUEUE(x)        ((void *)(((uint32_t)(uintptr_t)(x)) | 0x2U))
#define BLOCKING_ON_CONDITION(x)    ((void *)(((uint32_t)(uintptr_t)(x)) | 0x3U))


class Thread
{
    public:

    public:
        thread_saved_state tss;         /* always first member */

        bool is_privileged;
        std::string name;

        int base_priority;

        bool for_deletion = 0;          /* do we need to delete the thread at next task switch? */
        void *retval;

        MemRegion stack;

        bool is_blocking = false;
        PThread blocking_on = nullptr;
        void *blocking_on_primitive = nullptr;
        uint64_t block_until = 0;

        Spinlock sl;

        PProcess p;

        /* Used for waiting on inter-process RPC returns */
        SimpleSignal ss;
        WaitSimpleSignal_params ss_p;

        /* pthread TLS data */
        std::map<pthread_key_t, void *> tls_data;

        /* pthread_join waiting thread */
        PThread join_thread = nullptr;
        void **join_thread_retval;

        /* thread times */
        uint64_t cur_timeslice_start = 0ULL;
        uint64_t total_s_time = 0ULL;
        uint64_t total_us_time = 0ULL;

        Thread(PProcess owning_process);
        ~Thread();

        void Cleanup(void *tretval);

        typedef void *(*threadstart_t)(void *p);
        static PThread Create(std::string name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            PProcess owning_process,
            CPUAffinity affinity = CPUAffinity::Either,
            MemRegion stack = InvalidMemregion(),
            const mpu_saved_state *mpu_setup = mpu_default);
};

#endif
