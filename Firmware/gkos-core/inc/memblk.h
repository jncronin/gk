#ifndef MEMBLK_H
#define MEMBLK_H

#include "ostypes.h"
#include <cstddef>
#include <cstdint>

#include "memblk_types.h"

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

extern "C" void GKOS_FUNC(init_memblk)();
MemRegion GKOS_FUNC(memblk_allocate)(size_t n, MemRegionType rtype);
MemRegion GKOS_FUNC(memblk_allocate_for_stack)(size_t n, CPUAffinity affinity);
void GKOS_FUNC(memblk_deallocate)(struct MemRegion &r);

#endif
