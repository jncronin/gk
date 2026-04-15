#ifndef PERSISTENT_H
#define PERSISTENT_H

#include "vmem.h"

#define PERSISTENT_BASE PMEM_TO_VMEM_DEVICE(0x46010100UL)
#define persistent ((volatile uint32_t *)PERSISTENT_BASE)

#define PERSISTENT_ID_REBOOT_FLAGS      0
#define PERSISTENT_ID_VOLUME            1
#define PERSISTENT_ID_BRIGHTNESS        2

/*
    RAII-style access to the persistent memory registers in the backup domain.
    Read access does not require this because the registers are readable by default anyway.

    On construction:
    - Acquires a mutex to prevent race conditions
    - Enables write access to backup domain

    On destruction:
    - Disables write access to backup domain
    - Releases the mutex
*/
class PersistentMemoryWriteGuard
{
    public:
        PersistentMemoryWriteGuard();
        ~PersistentMemoryWriteGuard();

        PersistentMemoryWriteGuard(const PersistentMemoryWriteGuard &) = delete;
        PersistentMemoryWriteGuard(PersistentMemoryWriteGuard &&) = delete;
};

void persistent_reboot_flags_set(unsigned int flag);
void persistent_reboot_flags_clear(unsigned int flag);

#endif
