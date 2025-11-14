#ifndef VBLOCK_H
#define VBLOCK_H

#include <cstdint>
#include "buddy.h"
#include "ostypes.h"

#define VBLOCK_TAG_USER                     0x1
#define VBLOCK_TAG_WRITE                    0x2
#define VBLOCK_TAG_EXEC                     0x4
#define VBLOCK_TAG_GUARD_MASK               (0xfULL << 3)
#define VBLOCK_TAG_GUARD_LOWER_POS          3
#define VBLOCK_TAG_GUARD_UPPER_POS          5
#define VBLOCK_TAG_GUARD(lower, upper)      (((lower) << VBLOCK_TAG_GUARD_LOWER_POS) | ((upper) << VBLOCK_TAG_GUARD_UPPER_POS))

void init_vblock();
VMemBlock vblock_alloc(size_t size, bool user, bool write, bool exec,
    unsigned int lower_guard = 0, unsigned int upper_guard = 0);
VMemBlock vblock_valid(uintptr_t addr);

#define VBLOCK_64k      65536ULL
#define VBLOCK_4M       (4ULL*1024*1024)
#define VBLOCK_512M     (512ULL*1024*1024)

#endif
