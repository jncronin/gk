#ifndef GK_CACHE_H
#define GK_CACHE_H

#include <cstdint>

enum CacheType_t { Data, Instruction, Both };
void InvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype, bool override_sram_check = false);
void CleanM7Cache(uint32_t base, uint32_t length, CacheType_t ctype);
void CleanAndInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype);
void CleanOrInvalidateM7Cache(uint32_t base, uint32_t length, CacheType_t ctype);

#endif
