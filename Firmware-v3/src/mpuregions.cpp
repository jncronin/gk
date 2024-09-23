#include "mpuregions.h"

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
