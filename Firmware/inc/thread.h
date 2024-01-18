#ifndef THREAD_H
#define THREAD_H

#include <cstdint>
#include <osmutex.h>
#include "region_allocator.h"
#include "memblk.h"
#include "ostypes.h"

#include "mpuregions.h"

class Thread
{
    public:

    public:
        thread_saved_state tss;     /* always first member */

        bool is_privileged;
        SRAM4String name;
        CPUAffinity affinity;

        int base_priority;

        Mutex m_blocking_on;
        SRAM4Vector<Thread *> blocking_on;
        uint32_t delta_sleep;       /* for sleeping task delta-queue */

        bool for_deletion;          /* do we need to delete the thread at next task switch? */

        MemRegion stack;

        bool is_dummy = false;

        typedef void (*threadstart_t)(void *p);
        static Thread *Create(std::string name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            CPUAffinity affinity = CPUAffinity::Either,
            size_t stack_size = 4096,
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
