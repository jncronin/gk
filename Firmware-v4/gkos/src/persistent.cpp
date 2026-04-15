#include "persistent.h"
#include "osmutex.h"
#include "vmem.h"
#include <stm32mp2xx.h>

#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))

static Mutex m_pmg;

PersistentMemoryWriteGuard::PersistentMemoryWriteGuard()
{
    m_pmg.lock();
    PWR_VMEM->BDCR1 |= PWR_BDCR1_DBD3P;
}

PersistentMemoryWriteGuard::~PersistentMemoryWriteGuard()
{
    PWR_VMEM->BDCR1 &= ~PWR_BDCR1_DBD3P;
    m_pmg.unlock();
}

void persistent_reboot_flags_set(unsigned int flag)
{
    if(persistent[PERSISTENT_ID_REBOOT_FLAGS] & flag)
        return;
    PersistentMemoryWriteGuard pmg;
    persistent[PERSISTENT_ID_REBOOT_FLAGS] =
        persistent[PERSISTENT_ID_REBOOT_FLAGS] | flag;
}

void persistent_reboot_flags_clear(unsigned int flag)
{
    if(!(persistent[PERSISTENT_ID_REBOOT_FLAGS] & flag))
        return;
    PersistentMemoryWriteGuard pmg;
    persistent[PERSISTENT_ID_REBOOT_FLAGS] =
        persistent[PERSISTENT_ID_REBOOT_FLAGS] & ~flag;
}
