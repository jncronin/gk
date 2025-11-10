#ifndef PMEM_H
#define PMEM_H

#include "buddy.h"

using PmemBlock = BuddyEntry;
using PmemAllocator = BuddyAllocator<65536, 0x20000000, 0x80000000>;
extern PmemAllocator Pmem;

void init_pmem(uint64_t ddr_start, uint64_t ddr_end);


#endif
