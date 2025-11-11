#ifndef VBLOCK_H
#define VBLOCK_H

#include <cstdint>
#include "buddy.h"

void init_vblock();
BuddyEntry vblock_alloc(size_t size);

#define VBLOCK_64k      65536ULL
#define VBLOCK_4M       (4ULL*1024*1024)
#define VBLOCK_512M     (512ULL*1024*1024)

#endif
