#ifndef GKOS_BOOT_INTERFACE_H
#define GKOS_BOOT_INTERFACE_H

#include <cstdint>

struct gkos_boot_interface
{
    uint64_t ddr_start;
    uint64_t ddr_end;
};

constexpr uint64_t gkos_magic = (uint64_t)'G' |
    ((uint64_t)'K') << 8 |
    ((uint64_t)'O') << 16 |
    ((uint64_t)'S') << 24 |
    ((uint64_t)'S') << 32 |
    ((uint64_t)'S') << 40 |
    ((uint64_t)'B') << 48 |
    ((uint64_t)'L') << 56;

#endif
