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
