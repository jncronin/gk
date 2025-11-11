#ifndef VBLOCK_H
#define VBLOCK_H

#include <cstdint>
#include "buddy.h"

#define VBLOCK_TAG_USER     0x1
#define VBLOCK_TAG_WRITE    0x2
#define VBLOCK_TAG_EXEC     0x4

void init_vblock();
BuddyEntry vblock_alloc(size_t size, uint32_t tag = 0);
std::pair<bool, uint32_t> vblock_valid(uintptr_t addr);

#define VBLOCK_64k      65536ULL
#define VBLOCK_4M       (4ULL*1024*1024)
#define VBLOCK_512M     (512ULL*1024*1024)

#endif
