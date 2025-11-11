#ifndef VBLOCK_H
#define VBLOCK_H

#include <cstdint>
#include "buddy.h"

#define VBLOCK_TAG_USER                     0x1
#define VBLOCK_TAG_WRITE                    0x2
#define VBLOCK_TAG_EXEC                     0x4
#define VBLOCK_TAG_GUARD_MASK               (0xfULL << 3)
#define GUARD_BITS_64k                      0x1
#define GUARD_BITS_128k                     0x2
#define GUARD_BITS_256k                     0x3
#define GUARD_BITS_512k                     0x4
#define VBLOCK_TAG_GUARD_LOWER_POS          3
#define VBLOCK_TAG_GUARD_UPPER_POS          5
#define VBLOCK_TAG_GUARD(lower, upper)      (((lower) << VBLOCK_TAG_GUARD_LOWER_POS) | ((upper) << VBLOCK_TAG_GUARD_UPPER_POS))

void init_vblock();
BuddyEntry vblock_alloc(size_t size, uint32_t tag = 0);
std::pair<BuddyEntry, uint32_t> vblock_valid(uintptr_t addr);

#define VBLOCK_64k      65536ULL
#define VBLOCK_4M       (4ULL*1024*1024)
#define VBLOCK_512M     (512ULL*1024*1024)

#endif
