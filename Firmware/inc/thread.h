#ifndef THREAD_H
#define THREAD_H

#include <cstdint>
#include <cstddef>
#include <osmutex.h>
#include "region_allocator.h"
#include "memblk.h"
#include "ostypes.h"
#include "osmutex.h"

#include "mpuregions.h"

#include "process.h"

class Thread
{
    public:

    public:
        thread_saved_state tss;     /* always first member */

        bool is_privileged;
        SRAM4String name;
        CPUAffinity affinity;

        int base_priority;

        bool for_deletion;          /* do we need to delete the thread at next task switch? */

        MemRegion stack;

        bool is_blocking = false;

        Spinlock sl;
        int running_on_core = 0;
        int chosen_for_core = 0;

        Process &p;

        Thread(Process &owning_process);

        typedef void (*threadstart_t)(void *p);
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
