#include "smc.h"
#include "logger.h"
#include "pmic.h"

static int smc_set_power_gk(SMC_Power_Target target, unsigned int voltage_mv);
static int smc_set_power_ev1(SMC_Power_Target target, unsigned int voltage_mv);
static int smc_set_power_target(pmic_vreg::_type type, unsigned int id, unsigned int voltage_mv,
    unsigned int voltage_mv_min, unsigned int voltage_mv_max, SMC_Power_Target target);

int smc_set_power(SMC_Power_Target target, unsigned int voltage_mv)
{
    // get PMIC version and use to deduce setup
    auto pmic_prod_id = pmic_read_register(0);

    if(pmic_prod_id == 0x22)
    {
        return smc_set_power_gk(target, voltage_mv);
    }
    else if(pmic_prod_id == 0x20)
    {
        return smc_set_power_ev1(target, voltage_mv);
    }
    else
    {
        klog("SM: smc_set_power: unknown product id: %x\n", pmic_prod_id);
        return -1;
    }
}

int smc_set_power_gk(SMC_Power_Target target, unsigned int voltage_mv)
{
    switch(target)
    {
        case SMC_Power_Target::Core:
            return smc_set_power_target(pmic_vreg::Buck, 2, voltage_mv, 670, 842, target);

        case SMC_Power_Target::CPU:
            return smc_set_power_target(pmic_vreg::Buck, 1, voltage_mv, 640, 935, target);

        case SMC_Power_Target::GPU:
            return smc_set_power_target(pmic_vreg::Buck, 3, voltage_mv, 760, 961, target);

        case SMC_Power_Target::SDCard:
            return smc_set_power_target(pmic_vreg::LDO, 7, voltage_mv, 1800, 3300, target);

        case SMC_Power_Target::SDCard_IO:
            return smc_set_power_target(pmic_vreg::LDO, 8, voltage_mv, 1800, 3300, target);

        case SMC_Power_Target::SDIO_IO:
            return smc_set_power_target(pmic_vreg::LDO, 5, voltage_mv, 1800, 3300, target);

        case SMC_Power_Target::Flash:
            return smc_set_power_target(pmic_vreg::LDO, 2, voltage_mv, 1200, 3300, target);

        case SMC_Power_Target::Audio:
            return smc_set_power_target(pmic_vreg::LDO, 6, voltage_mv, 1200, 3300, target);

        case SMC_Power_Target::USB:
            return smc_set_power_target(pmic_vreg::LDO, 4, voltage_mv, 3300, 3300, target);
    }
    return -1;
}

int smc_set_power_ev1(SMC_Power_Target target, unsigned int voltage_mv)
{
    switch(target)
    {
        case SMC_Power_Target::Core:
            return smc_set_power_target(pmic_vreg::Buck, 2, voltage_mv, 670, 842, target);

        case SMC_Power_Target::CPU:
            return smc_set_power_target(pmic_vreg::Buck, 1, voltage_mv, 640, 935, target);

        case SMC_Power_Target::GPU:
            return smc_set_power_target(pmic_vreg::Buck, 3, voltage_mv, 760, 961, target);

        case SMC_Power_Target::SDCard:
            return smc_set_power_target(pmic_vreg::LDO, 7, voltage_mv, 1800, 3300, target);

        case SMC_Power_Target::SDCard_IO:
            return smc_set_power_target(pmic_vreg::LDO, 8, voltage_mv, 1800, 3300, target);

        case SMC_Power_Target::USB:
            return smc_set_power_target(pmic_vreg::LDO, 4, voltage_mv, 3300, 3300, target);

        default:
            klog("SM: set_power: target %u not supported on EV1\n", target);
            return -1;
    }
}

int smc_set_power_target(pmic_vreg::_type type, unsigned int id, unsigned int voltage_mv,
    unsigned int voltage_mv_min, unsigned int voltage_mv_max, SMC_Power_Target target)
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
