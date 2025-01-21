#ifndef MEMBLK_H
#define MEMBLK_H

#include "gk_conf.h"
#include "ostypes.h"
#include <cstddef>
#include <cstdint>
#include <string>

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

extern "C" void init_memblk();
#if GK_MEMBLK_STATS
// usage required for stats prior to heap becoming available
#define GK_MEMBLK_USAGE_KERNEL_HEAP     0

#define GK_MEMBLK_USAGE_MAX             1

MemRegion memblk_allocate(size_t n, MemRegionType rtype, const std::string &usage);
MemRegion memblk_allocate(size_t n, MemRegionType rtype, int usage);
MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity, const std::string &usage,
    int pref = -1);
void memblk_stats();
#else
MemRegion memblk_allocate(size_t n, MemRegionType rtype);
MemRegion memblk_allocate_for_stack(size_t n, CPUAffinity affinity, int pref = -1);
#endif
void memblk_deallocate(struct MemRegion &r);

#endif
