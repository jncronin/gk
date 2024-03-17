#ifndef GK_CACHE_H
#define GK_CACHE_H

#include <cstdint>

enum CacheType_t { Data, Instruction, Both };
void InvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype);
void CleanM7Cache(uint32_t base, uint32_t length, CacheType_t ctype);
void CleanInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype);

#endif
