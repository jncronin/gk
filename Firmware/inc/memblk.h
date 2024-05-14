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

bool operator==(const MemRegion &a, const MemRegion &b);
bool operator!=(const MemRegion &a, const MemRegion &b);

static constexpr MemRegion InvalidMemregion()
{
    MemRegion ret = {};
    ret.valid = false;
    ret.address = 0;
    ret.length = 0;
    return ret;
}

void init_memblk();
MemRegion memblk_allocate(size_t n, MemRegionType rtype);
MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity);
void memblk_deallocate(struct MemRegion &r);

#endif
