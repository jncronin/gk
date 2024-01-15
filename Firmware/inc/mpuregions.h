#ifndef MPUREGIONS_H
#define MPUREGIONS_H

/* We have 8 MPU regions available.
    In case of overlapping, higher IDs take priority.

    0 - MSP - in AXISRAM for CM7 stacks and in SRAM for CM4 stacks.
        64 kb to allow interrupt stacking.  Privilege RW, unprivilege no access.
    1 - FLASH - Privilege RX, unprivilege no access.
    2 - Peripherals - Privilege RW, unprivilege no access.
    3 - PSP - both rw.  Somewhere in AXISRAM.
    4 - SRAM4 - 64 kb for control structures (TCB, queues, semaphores etc).  Shared uncacheable.
        Privilege rw, unprivilege ro.
    5 - SDRAM - Normal.  RWX for both privileged and non-privileged.
    6 - Allocatable to user mode program (e.g. peripheral/SRAM access)
    7 - Allocatable to user mode program (e.g. peripheral/SRAM access)
*/

#include "ostypes.h"

constexpr const uint32_t WBWA_S =       0b101101;
constexpr const uint32_t WBWA_NS =      0b101001;
constexpr const uint32_t WT_S =         0b110110;
constexpr const uint32_t WT_NS =        0b110010;
constexpr const uint32_t NC_S =         0b100100;
constexpr const uint32_t NC_NS =        0b100000;
constexpr const uint32_t DEV_S =        0b000001;

constexpr mpu_saved_state MPUGenerate(uint32_t base_addr,
    uint32_t length,
    uint32_t reg_id,
    bool executable,
    MemRegionAccess privilege_access,
    MemRegionAccess unprivilege_access,
    uint32_t tex_scb)
{
    mpu_saved_state ret { 0, 0 };
    ret.rbar = (reg_id & 0x7UL) | (1UL << 4) | (base_addr & ~0x1fUL);

    // encode size
    uint32_t cur_size = 32;
    uint32_t cur_i = 4;
    for(; cur_i < 32; cur_i++, cur_size *= 2)
    {
        if(cur_size < length)
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

    ret.rasr = 1UL | (cur_i << 1UL) | ((tex_scb & 0x3f) << 16) | (ap << 24);

    return ret;
}

constexpr mpu_saved_state MPUGenerateNonValid(uint32_t reg_id)
{
    mpu_saved_state ret { 0, 0 };
    ret.rbar = (reg_id & 0x7UL) | (1UL << 4);
    ret.rasr = 0UL;
    return ret;
}

// TODO: make first 64 kB of DTCM and SRAM usuable as stacks
constexpr mpu_saved_state mpu_msp_cm7 = MPUGenerate(0x20000000, 65536, 0, false,
    MemRegionAccess::RW, MemRegionAccess::NoAccess, WBWA_NS);
constexpr mpu_saved_state mpu_msp_cm4 = MPUGenerate(0x30000000, 65536, 0, false,
    MemRegionAccess::RW, MemRegionAccess::NoAccess, WBWA_NS);
constexpr mpu_saved_state mpu_flash = MPUGenerate(0x08000000, 0x200000, 1, true,
    MemRegionAccess::RO, MemRegionAccess::NoAccess, WT_NS);
constexpr mpu_saved_state mpu_periph = MPUGenerate(0x40000000, 0x20000000, 2, false,
    MemRegionAccess::RW, MemRegionAccess::NoAccess, DEV_S);
constexpr mpu_saved_state mpu_sram4 = MPUGenerate(0x38000000, 0x10000, 4, false,
    MemRegionAccess::RW, MemRegionAccess::RO, NC_S);
constexpr mpu_saved_state mpu_sdram = MPUGenerate(0xc0000000, 0x04000000, 5, true,
    MemRegionAccess::RW, MemRegionAccess::RW, WBWA_NS);

#endif
