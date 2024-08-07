#ifndef GK_CACHE_H
#define GK_CACHE_H

#include <cstdint>

enum CacheType_t { Data, Instruction, Both };
void GKOS_FUNC(InvalidateM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype, bool override_sram_check = false);
void GKOS_FUNC(CleanM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype);
void GKOS_FUNC(CleanAndInvalidateM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype);
void GKOS_FUNC(CleanOrInvalidateM7Cache)(uint32_t base, uint32_t length, CacheType_t ctype);

#endif
