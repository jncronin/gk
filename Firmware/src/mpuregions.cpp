#include "mpuregions.h"

extern int _srtt, _ertt, _scm7_stack, _ecm7_stack_align, _eitcm;
const uint32_t _srttp = (uint32_t)&_srtt;
const uint32_t _erttp = (uint32_t)&_ertt;
const uint32_t _scm7_stackp = (uint32_t)&_scm7_stack;
const uint32_t _ecm7_stack_alignp = (uint32_t)&_ecm7_stack_align;
const uint32_t _eitcmp = (uint32_t)&_eitcm;

static const constexpr mpu_saved_state mpu_devs = MPUGenerate(0x40000000, 0x20000000, 0, false, RW, RO, DEV_S);
static mpu_saved_state mpu_rtt = MPUGenerate(_srttp, _erttp - _srttp, 1, false, RW, NoAccess, N_NC_NS);
// xspi1 space is wt by default
static const constexpr mpu_saved_state mpu_xspi = MPUGenerate(0x90000000, 128*1024*1024U, 2, false, RW, NoAccess, WBWA_NS);
static const constexpr mpu_saved_state mpu_fb0 = MPUGenerate(0x90000000, 0x400000, 3, false, RW, RW, WT_NS);
// disable access to start and end of msp stack space
static mpu_saved_state mpu_msp = MPUGenerate(_scm7_stackp, _ecm7_stack_alignp - _scm7_stackp, 4, false, NoAccess, NoAccess, WBWA_NS, 0x7eU);
// read only access to kernel itcm, includes vtors
static mpu_saved_state mpu_itcm = MPUGenerate(0, _eitcmp, 5, true, RO, NoAccess, WBWA_NS);

mpu_saved_state mpu_default[16] =
{
    mpu_devs,
    mpu_rtt,
    mpu_xspi,
    mpu_fb0,
    mpu_msp,
    mpu_itcm,
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

uint32_t mpu_saved_state::slot() const
{
    return rbar & 0xfU;
}

void mpu_saved_state::set_slot(unsigned int slot_id)
{
    slot_id &= 0xfU;
    rbar = (rbar & ~0xfU) | slot_id;
}
