#ifndef VMEM_H
#define VMEM_H

#include "gkos_vmem.h"
#include "ostypes.h"

#include <cstddef>

#define PMEM_TO_VMEM(a) ((a) + UH_START)

int vmem_map(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec);
int vmem_map(const VMemBlock &vaddr, const PMemBlock &paddr);

/* Quick clear/copy, assumes multiples of 64 bytes, 16 byte alignment */
extern "C" void quick_clear_64(void *dest, size_t n = 65536);
extern "C" void quick_copy_64(void *dest, const void *src, size_t n = 65536);

#endif
