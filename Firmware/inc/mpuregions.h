#ifndef MPUREGIONS_H
#define MPUREGIONS_H

/* 
    Revision of MPU allocations:
    We allow privileged code to use the default map, so it already has access to FLASH and peripherals

    Then:
        0: SRAM4
        1: Framebuffer
        2: Code/data
        3: Heap
        4: Stack
        5: 3x mmap regions
*/

#include <cstdint>
#include "ostypes.h"
#include "gk_conf.h"

constexpr const uint32_t WBWA_S =       0b001111;
constexpr const uint32_t WBWA_NS =      0b001011;
constexpr const uint32_t WT_S =         0b000110;
constexpr const uint32_t WT_NS =        0b000010;
//constexpr const uint32_t NC_S =         0b100100;
//constexpr const uint32_t NC_NS =        0b100000;
constexpr const uint32_t DEV_S =        0b000001;
constexpr const uint32_t N_NC_NS =      0b001000;
constexpr const uint32_t N_NC_S =       0b001100;
constexpr const uint32_t SO_S =         0b000000;

static constexpr uint32_t align_up(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

constexpr mpu_saved_state MPUGenerate(uint32_t base_addr,
    uint32_t length,
    uint32_t reg_id,
    bool executable,
    MemRegionAccess privilege_access,
    MemRegionAccess unprivilege_access,
    uint32_t tex_scb)
{
    mpu_saved_state ret { 0, 0 };

    if(length < 32) length = 32;

    // base_addr must be aligned on length
    uint32_t end = base_addr + length;
    length = align_up(length);
    base_addr &= ~(length - 1);
    length = align_up(end - base_addr);
    base_addr &= ~(length - 1);             // Recheck alignment as length may have increased in previous step
    length = align_up(end - base_addr);

    ret.rbar = (reg_id & 0x7UL) | (1UL << 4) | (base_addr & ~0x1fUL);

    // encode size
    uint32_t cur_size = 32;
    uint32_t cur_i = 4;
    for(; cur_i < 32; cur_i++, cur_size *= 2)
    {
        if(cur_size >= length)
            break;
    }

    // encode AP
    uint32_t ap = 0UL;
    if(privilege_access == MemRegionAccess::RW)
    {
        switch(unprivilege_access)
        {
            case MemRegionAccess::RW:
                ap = 0b011;
                break;
            case MemRegionAccess::RO:
                ap = 0b010;
                break;
            case MemRegionAccess::NoAccess:
                ap = 0b001;
                break;
        }
    }
    else if(privilege_access == MemRegionAccess::RO)
    {
        switch(unprivilege_access)
        {
            case MemRegionAccess::RW:   /* can't have unpriv RW with priv RO */
            case MemRegionAccess::RO:
                ap = 0b110;
                break;
            case MemRegionAccess::NoAccess:
                ap = 0b101;
                break;
        }
    }

    ret.rasr = 1UL | (cur_i << 1UL) | ((tex_scb & 0x3f) << 16) | (ap << 24) | (executable ? 0UL : (1UL << 28));

    return ret;
}

constexpr mpu_saved_state MPUGenerateNonValid(uint32_t reg_id)
{
    mpu_saved_state ret { 0, 0 };
    ret.rbar = (reg_id & 0x7UL) | (1UL << 4);
    ret.rasr = 0UL;
    return ret;
}

constexpr mpu_saved_state mpu_sram4 = MPUGenerate(0x38000000, 0x10000, 0, false, RW, RO, N_NC_S);
constexpr mpu_saved_state mpu_fb0 = MPUGenerate(0x60000000, 0x400000, 1, false, RW, RW, WT_NS);

constexpr mpu_saved_state mpu_default[8] =
{
    mpu_sram4,
    mpu_fb0,
    MPUGenerateNonValid(2),
    MPUGenerateNonValid(3),
    MPUGenerateNonValid(4),
    MPUGenerateNonValid(5),
    MPUGenerateNonValid(6),
    MPUGenerateNonValid(7)
};

#endif
