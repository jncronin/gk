#ifndef THREAD_H
#define THREAD_H

#include <cstdint>
#include <osmutex.h>
#include "region_allocator.h"
#include "memblk.h"
#include "ostypes.h"

struct mpu_saved_state
{
    uint32_t rbar;
    uint32_t rasr;
};

struct thread_saved_state
{
    uint32_t psp;       /* All threads, regardless of privilege, use PSP */
    uint32_t control;   /* used to decide privileged vs unprivileged mode */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t lr;

    uint32_t fpuregs[16];
    mpu_saved_state cm7_mpu0, cm4_mpu0; /* MSP - varies depending on which core is running */
    mpu_saved_state mpuss[7];
};

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

        typedef void (*threadstart_t)(void *p);
        static Thread *Create(std::string name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            CPUAffinity affinity = CPUAffinity::Either,
            size_t stack_size = 4096);
};

Thread *GetCurrentThreadForCore(int coreid = -1);
Thread *GetNextThreadForCore(int coreid = -1);
int GetCoreID();

#endif
