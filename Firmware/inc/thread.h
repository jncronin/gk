#ifndef THREAD_H
#define THREAD_H

#include <cstdint>
#include <cstddef>
#include <map>
#include <osmutex.h>
#include "region_allocator.h"
#include "memblk.h"
#include "ostypes.h"
#include "osmutex.h"

#include "mpuregions.h"

#include "process.h"
#include "syscalls.h"

static constexpr uint32_t thread_signal_lwext = 0x1;
#define GK_NPRIORITIES      10

class Thread
{
    public:

    public:
        thread_saved_state tss;         /* always first member */
        CPUAffinity affinity;           /* always second member, offset 180 */
        int running_on_core = 0;        /* offset 184 */
        int chosen_for_core = 0;        /* offset 188 */
        int deschedule_from_core = 0;   /* offset 192 */

        bool is_privileged;
        SRAM4String name;

        int base_priority;

        bool for_deletion = 0;          /* do we need to delete the thread at next task switch? */
        void *retval;

        MemRegion stack;

        bool is_blocking = false;
        Thread *blocking_on = nullptr;
        uint64_t block_until = 0;

        Spinlock sl;

        Process &p;

        /* Used for waiting on inter-process RPC returns */
        SimpleSignal ss;
        WaitSimpleSignal_params ss_p;

        /* pthread TLS data */
        std::map<pthread_key_t, void *> tls_data;

        /* pthread_join waiting thread */
        Thread *join_thread = nullptr;
        void **join_thread_retval;

        Thread(Process &owning_process);

        void Cleanup(void *tretval);

        typedef void *(*threadstart_t)(void *p);
        static Thread *Create(std::string name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            Process &owning_process,
            CPUAffinity affinity = CPUAffinity::Either,
            MemRegion stack = InvalidMemregion(),
            mpu_saved_state extra_permissions = MPUGenerateNonValid(6),
            mpu_saved_state extra_permissions2 = MPUGenerateNonValid(7));
};

// Called from PendSV therefore not mangled
extern "C" {
    Thread *GetCurrentThreadForCore(int coreid = -1);
    Thread *GetNextThreadForCore(int coreid = -1);
    int GetCoreID();
    void SetNextThreadForCore(Thread *t, int coreid = -1);
    void ScheduleThread(Thread *t);
}

#endif
