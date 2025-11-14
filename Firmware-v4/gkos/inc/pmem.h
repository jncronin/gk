#ifndef PMEM_H
#define PMEM_H

#include "ostypes.h"
#include "buddy.h"

using PmemAllocator = BuddyAllocator<65536, 0x20000000, 0x80000000, PMemBlock>;
extern PmemAllocator Pmem;

void init_pmem(uint64_t ddr_start, uint64_t ddr_end);


#endif
