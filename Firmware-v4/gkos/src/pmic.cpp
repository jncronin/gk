#include "i2c.h"
#include "pmic.h"
#include <cstdio>
#include "logger.h"

#define I2C_ADDR 0x33

uint8_t pmic_read_register(uint8_t addr)
{
    uint8_t ret;
    auto &i2c7 = i2c(7);
    if(i2c7.RegisterRead(I2C_ADDR, addr, &ret, 1) == 1)
        return ret;
    return 0;
}

void pmic_write_register(uint8_t addr, uint8_t val)
{
    auto &i2c7 = i2c(7);
    i2c7.RegisterWrite(I2C_ADDR, addr, &val, 1);
}

void pmic_dump()
{
    for(int i = 1; i <= 7; i++)
    {
        pmic_dump(pmic_get_buck(i));
        pmic_dump(pmic_get_buck(i, true));
    }
    for(int i = 1; i <= 8; i++)
    {
        pmic_dump(pmic_get_ldo(i));
        pmic_dump(pmic_get_ldo(i, true));
    }
    pmic_dump(pmic_get_refddr());
    pmic_dump(pmic_get_refddr(true));
}

void pmic_dump(const pmic_vreg &v)
{
    switch(v.type)
    {
        case pmic_vreg::Buck:
            klog("BUCK%d  ", v.id);
            break;
        case pmic_vreg::LDO:
            klog("LDO%d   ", v.id);
            break;
        case pmic_vreg::RefDDR:
            klog("REFDDR ");
            break;
    }

    if(v.is_alt)
    {
        klog("ALT:  ");
    }
    else
    {
        klog("MAIN: ");
    }

    if(v.is_enabled)
        klog("ENABLED,  ");
    else
        klog("disabled, ");

    if(v.mode == pmic_vreg::Bypass)
        klog("        ");
    else
        klog("%4d mV ", v.mv);
    
    switch(v.mode)
    {
        case pmic_vreg::HP:
            klog("HP");
            break;
        case pmic_vreg::LP:
            klog("LP");
            break;
        case pmic_vreg::CCM:
            klog("CCM");
            break;
        case pmic_vreg::SinkSource:
            klog("SinkSource");
            break;
        case pmic_vreg::Bypass:
            klog("BYPASS");
            break;
        case pmic_vreg::Normal:
            klog("NORMAL");
        default:
            break;
    }

    klog("\n");
}

pmic_vreg pmic_get_buck(int id, bool alt)
{
    if(id < 1 || id > 7)
        return pmic_vreg{};
    
    pmic_vreg ret;
    ret.type = pmic_vreg::Buck;
    ret.id = id;
    ret.is_alt = alt;

    const uint8_t reg_bases[] = { 0x20, 0x25, 0x2a, 0x2f, 0x34, 0x39, 0x3e };

    uint8_t reg = reg_bases[id - 1];
    if(alt)
        reg += 2;

    auto cr1 = pmic_read_register(reg);
    auto cr2 = pmic_read_register(reg + 1);

    switch(id)
    {
        case 1:
        case 2:
        case 3:
        case 6:
            cr1 &= 0x7fU;
            if(cr1 >= 100)
                ret.mv = 1500;
            else
                ret.mv = 500 + (int)cr1 * 10;
            break;
        default:
            cr1 &= 0x7fU;
            if(cr1 <= 100)
                ret.mv = 1500;
            else
                ret.mv = 1500 + (int)(cr1 - 100) * 100;
            break;
    }

    if(cr2 & 0x1)
        ret.is_enabled = true;
    else
        ret.is_enabled = false;
    
    switch((cr2 >> 1) & 0x3)
    {
        case 0:
            ret.mode = pmic_vreg::HP;
            break;
        case 1:
            ret.mode = pmic_vreg::LP;
            break;
        case 2:
            ret.mode = pmic_vreg::CCM;
            break;
        default:
            break;
    }

    return ret;
}

pmic_vreg pmic_get_ldo(int id, bool alt)
{
    if(id < 1 || id > 8)
        return pmic_vreg{};
    
    pmic_vreg ret;
    ret.type = pmic_vreg::LDO;
    ret.id = id;
    ret.is_alt = alt;

    const uint8_t reg_bases[] = { 0x4c, 0x4f, 0x52, 0x55, 0x58, 0x5b, 0x5e, 0x61 };

    uint8_t reg = reg_bases[id - 1];
    if(alt)
        reg += 1;

    auto cr1 = pmic_read_register(reg);

    switch(id)
    {
        case 1:
            ret.mv = 1800;
            break;
        case 4:
            ret.mv = 3300;
            break;
        default:
            {
                auto vout = (cr1 >> 1) & 0x1f;
                ret.mv = 900 + (int)vout * 100;
            }
            break;
    }

    switch(id)
    {
        case 1:
            ret.mode = pmic_vreg::Normal;
            break;
        case 3:
            if(cr1 & 0x80)
                ret.mode = pmic_vreg::SinkSource;
            else if(cr1 & 0x40)
                ret.mode = pmic_vreg::Bypass;
            else
                ret.mode = pmic_vreg::Normal;
            break;
        case 4:
            ret.mode = pmic_vreg::Normal;
            break;
        default:
            if(cr1 & 0x40)
                ret.mode = pmic_vreg::Bypass;
            else
                ret.mode = pmic_vreg::Normal;
            break;
    }

    if(ret.id == 3 && ret.mode == pmic_vreg::SinkSource)
    {
        // always follows vout6/2
        auto vout6 = pmic_get_buck(6);
        ret.mv = vout6.is_enabled ? (vout6.mv / 2) : 0;
    }

    if(cr1 & 0x1)
        ret.is_enabled = true;
    else
        ret.is_enabled = false;

    return ret;
}

pmic_vreg pmic_get_refddr(bool alt)
{
    pmic_vreg ret;
    ret.type = pmic_vreg::RefDDR;
    ret.id = 0;
    ret.is_alt = alt;

    uint8_t reg = 0x64;
    if(alt)
        reg += 1;

    auto cr1 = pmic_read_register(reg);

    auto vout6 = pmic_get_buck(6);

    ret.mv = (vout6.is_enabled) ? (vout6.mv / 2) : 0;
    ret.mode = pmic_vreg::Normal;

    if(cr1 & 0x1)
        ret.is_enabled = true;
    else
        ret.is_enabled = false;
    
    return ret;
}

void pmic_set_buck(const pmic_vreg &v)
{
    const uint8_t reg_bases[] = { 0x20, 0x25, 0x2a, 0x2f, 0x34, 0x39, 0x3e };

    uint8_t reg = reg_bases[v.id - 1];
    if(v.is_alt)
        reg += 2;
    
    uint8_t cr1 = 0;
    if(v.mv < 1500)
        cr1 = (v.mv - 500) / 10;
    else
        cr1 = (v.mv - 1500) / 100 + 100;
    
    uint8_t cr2 = 0;
    if(v.is_enabled)
        cr2 |= 0x1;
    switch(v.mode)
    {
        case pmic_vreg::HP:
            break;
        case pmic_vreg::LP:
            cr2 |= (1U << 1);
            break;
        case pmic_vreg::CCM:
            cr2 |= (2U << 1);
            break;
        default:
            break;
    }

    pmic_write_register(reg, cr1);
    pmic_write_register(reg + 1, cr2);
}

void pmic_set_ldoref(const pmic_vreg &v)
{
    const uint8_t reg_bases[] = { 0x64, 0x4c, 0x4f, 0x52, 0x55, 0x58, 0x5b, 0x5e, 0x61 };

    uint8_t reg = reg_bases[(v.type == pmic_vreg::RefDDR) ? 0 : v.id];
    if(v.is_alt)
        reg += 1;

    uint8_t cr1 = 0;

    if(v.is_enabled)
        cr1 |= 0x1;
    switch(v.mode)
    {
        case pmic_vreg::Bypass:
            cr1 |= 0x40;
            break;
        case pmic_vreg::SinkSource:
            if(v.id == 3)
                cr1 |= 0x80;
            break;
        default:
            break;
    }

    switch(v.id)
    {
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
            {
                uint8_t vout = (v.mv - 900) / 100;
                cr1 |= vout << 1;
            }
            break;
    }

    pmic_write_register(reg, cr1);
}

void pmic_set(const pmic_vreg &v)
{
    switch(v.type)
    {
        case pmic_vreg::Buck:
            pmic_set_buck(v);
            break;
        default:
            pmic_set_ldoref(v);
            break;
    }
}

static int pmic_set_power_gk(PMIC_Power_Target target, unsigned int voltage_mv);
static int pmic_set_power_ev1(PMIC_Power_Target target, unsigned int voltage_mv);
static int pmic_set_power_target(pmic_vreg::_type type, unsigned int id, unsigned int voltage_mv,
    unsigned int voltage_mv_min, unsigned int voltage_mv_max, PMIC_Power_Target target);

int pmic_set_power(PMIC_Power_Target target, unsigned int voltage_mv)
{
    // get PMIC version and use to deduce setup
    auto pmic_prod_id = pmic_read_register(0);

    if(pmic_prod_id == 0x22)
    {
        return pmic_set_power_gk(target, voltage_mv);
    }
    else if(pmic_prod_id == 0x20)
    {
        return pmic_set_power_ev1(target, voltage_mv);
    }
    else
    {
        klog("SM: pmic_set_power: unknown product id: %x\n", pmic_prod_id);
        return -1;
    }
}

int pmic_set_power_gk(PMIC_Power_Target target, unsigned int voltage_mv)
{
    switch(target)
    {
        case PMIC_Power_Target::Core:
            return pmic_set_power_target(pmic_vreg::Buck, 2, voltage_mv, 670, 842, target);

        case PMIC_Power_Target::CPU:
            return pmic_set_power_target(pmic_vreg::Buck, 1, voltage_mv, 640, 935, target);

        case PMIC_Power_Target::GPU:
            return pmic_set_power_target(pmic_vreg::Buck, 3, voltage_mv, 760, 961, target);

        case PMIC_Power_Target::SDCard:
            return pmic_set_power_target(pmic_vreg::LDO, 7, voltage_mv, 1800, 3300, target);

        case PMIC_Power_Target::SDCard_IO:
            return pmic_set_power_target(pmic_vreg::LDO, 8, voltage_mv, 1800, 3300, target);

        case PMIC_Power_Target::SDIO_IO:
            return pmic_set_power_target(pmic_vreg::LDO, 5, voltage_mv, 1800, 3300, target);

        case PMIC_Power_Target::Flash:
            return pmic_set_power_target(pmic_vreg::LDO, 2, voltage_mv, 1200, 3300, target);

        case PMIC_Power_Target::Audio:
            return pmic_set_power_target(pmic_vreg::LDO, 6, voltage_mv, 1200, 3300, target);

        case PMIC_Power_Target::USB:
            return pmic_set_power_target(pmic_vreg::LDO, 4, voltage_mv, 3300, 3300, target);
    }
    return -1;
}

int pmic_set_power_ev1(PMIC_Power_Target target, unsigned int voltage_mv)
{
    switch(target)
    {
        case PMIC_Power_Target::Core:
            return pmic_set_power_target(pmic_vreg::Buck, 2, voltage_mv, 670, 842, target);

        case PMIC_Power_Target::CPU:
            return pmic_set_power_target(pmic_vreg::Buck, 1, voltage_mv, 640, 935, target);

        case PMIC_Power_Target::GPU:
            return pmic_set_power_target(pmic_vreg::Buck, 3, voltage_mv, 760, 961, target);

        case PMIC_Power_Target::SDCard:
            return pmic_set_power_target(pmic_vreg::LDO, 7, voltage_mv, 1800, 3300, target);

        case PMIC_Power_Target::SDCard_IO:
            return pmic_set_power_target(pmic_vreg::LDO, 8, voltage_mv, 1800, 3300, target);

        case PMIC_Power_Target::USB:
            return pmic_set_power_target(pmic_vreg::LDO, 4, voltage_mv, 3300, 3300, target);

        default:
            klog("SM: set_power: target %u not supported on EV1\n", target);
            return -1;
    }
}

int pmic_set_power_target(pmic_vreg::_type type, unsigned int id, unsigned int voltage_mv,
    unsigned int voltage_mv_min, unsigned int voltage_mv_max, PMIC_Power_Target target)
{
    if(voltage_mv && (voltage_mv < voltage_mv_min))
    {
        klog("SM: set_power: voltage %u too low for target %d (%u - %u mV)\n", voltage_mv,
            (int)target, voltage_mv_min, voltage_mv_max, target);
        return -1;
    }
    if(voltage_mv > voltage_mv_max)
    {
        klog("SM: set_power: voltage %u too high for target %d (%u - %u mV)\n", voltage_mv,
            (int)target, voltage_mv_min, voltage_mv_max, target);
        return -1;
    }

    pmic_vreg pv { .type = type, .id = (int)id, .is_enabled = (voltage_mv != 0),
        .mv = (int)voltage_mv, .mode = pmic_vreg::Normal };
    pmic_set(pv);

    switch(type)
    {
        case pmic_vreg::Buck:
            {
                auto pmic_readback = pmic_get_buck(id);
                pmic_dump(pmic_readback);
                return pmic_readback.is_enabled ? pmic_readback.mv : 0;
            }
            break;

        case pmic_vreg::LDO:
            {
                auto pmic_readback = pmic_get_ldo(id);
                pmic_dump(pmic_readback);
                return pmic_readback.is_enabled ? pmic_readback.mv : 0;
            }
            break;

        default:
            return -1;
    }
}
