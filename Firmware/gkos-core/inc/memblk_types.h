#ifndef MEMBLK_TYPES_H
#define MEMBLK_TYPES_H

#include <stdint.h>

enum MemRegionType
{
    AXISRAM = 0,
    SRAM = 1,
    DTCM = 2,
    SDRAM = 3
};

struct MemRegion
{
    uint32_t address;
    uint32_t length;
    enum MemRegionType rt;
    bool valid;

    bool is_cacheable() const;
};

#endif
