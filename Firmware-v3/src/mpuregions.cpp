#include "mpuregions.h"

extern int _srtt, _ertt;
const uint32_t _srttp = (uint32_t)&_srtt;
const uint32_t _erttp = (uint32_t)&_ertt;
static mpu_saved_state mpu_rtt = MPUGenerate(_srttp, _erttp - _srttp, 3, false, RW, NoAccess, WT_NS);

mpu_saved_state mpu_default[16] =
{
    mpu_fb0,
    mpu_lptim1,
    fast_access,
    mpu_rtt,
    MPUGenerateNonValid(4),
    MPUGenerateNonValid(5),
    MPUGenerateNonValid(6),
    MPUGenerateNonValid(7),
    MPUGenerateNonValid(8),
    MPUGenerateNonValid(9),
    MPUGenerateNonValid(10),
    MPUGenerateNonValid(11),
    MPUGenerateNonValid(12),
    MPUGenerateNonValid(13),
    MPUGenerateNonValid(14),
    MPUGenerateNonValid(15)
};

bool mpu_saved_state::is_enabled() const
{
    return (rasr & 1UL) == 1UL;
}

uint32_t mpu_saved_state::base_addr() const
{
    return rbar & ~0x1fU;
}

uint32_t mpu_saved_state::length() const
{
    auto size = (rasr >> 1) & 0x1fU;
    return 1U << (size + 1);
}

MemRegionAccess mpu_saved_state::priv_access() const
{
    auto ap = (rasr >> 24) & 0x7U;
    switch(ap)
    {
        case 0b011:
        case 0b010:
        case 0b001:
            return MemRegionAccess::RW;
        case 0b110:
        case 0b101:
            return MemRegionAccess::RO;
        default:
            return MemRegionAccess::NoAccess;
    }
}

MemRegionAccess mpu_saved_state::unpriv_access() const
{
    auto ap = (rasr >> 24) & 0x7U;
    switch(ap)
    {
        case 0b011:
            return MemRegionAccess::RW;
        case 0b010:
        case 0b110:
            return MemRegionAccess::RO;
        default:
            return MemRegionAccess::NoAccess;
    }
}

uint32_t mpu_saved_state::tex_scb() const
{
    return (rasr >> 16) & 0x3f;
}
