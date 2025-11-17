#ifndef GK_CACHE_H
#define GK_CACHE_H

#include <cstdint>

#define CACHE_LINE_SIZE     64ULL

enum CacheType_t { Data, Instruction, Both };
void InvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);
void CleanA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);
void CleanAndInvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);
//void CleanOrInvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);

#endif
