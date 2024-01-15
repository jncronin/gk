#ifndef MEMBLK_H
#define MEMBLK_H

#include "ostypes.h"
#include <cstddef>
#include <cstdint>

struct MemRegion
{
    uint32_t address;
    uint32_t length;
    MemRegionType rt;
    bool valid;
};

void init_memblk();
MemRegion memblk_allocate(size_t n, MemRegionType rtype);
MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity);
void memblk_deallocate(struct MemRegion &r);

#endif
