#include "smc.h"
#include "ap.h"
#include "logger.h"
#include "pmic.h"

#define GIC_BASE             0x4AC00000UL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000UL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000UL)

static void smc_set_power(exception_regs *regs);
static void smc_set_power_gk(SMC_Power_Target target, unsigned int voltage_mv, exception_regs *regs);
static void smc_set_power_ev1(SMC_Power_Target target, unsigned int voltage_mv, exception_regs *regs);
static void smc_set_power_target(pmic_vreg::_type type, unsigned int id, unsigned int voltage_mv,
    unsigned int voltage_mv_min, unsigned int voltage_mv_max, exception_regs *regs);

void smc_handler(SMC_Call smc_id, exception_regs *regs)
{
    switch(smc_id)
    {
        case SMC_Call::StartupAP:
            if(regs->x0 < ncores)
            {
                auto coreid = (unsigned int)regs->x0;
                aps[coreid].epoint = (void (*)(void *, void *))regs->x1;
                aps[coreid].p0 = (volatile void *)regs->x2;
                aps[coreid].p1 = (volatile void *)regs->x3;
                aps[coreid].el1_stack = (uintptr_t)regs->x4;
                aps[coreid].ttbr1 = (uintptr_t)regs->x5;
                aps[coreid].vbar = (uintptr_t)regs->x6;
                aps[coreid].ready = true;

                // ping core
                *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0xf00) = (0x8ULL) |
                    (1ULL << (16 + coreid));

                klog("SM: start ap %u\n", coreid);
            }
            break;

        case SMC_Call::SetPower:
            smc_set_power(regs);
            break;
    }
}

void smc_set_power(exception_regs *regs)
{
    auto target = (SMC_Power_Target)regs->x0;
    auto voltage_mv = regs->x1;

    // get PMIC version and use to deduce setup
    auto pmic_prod_id = pmic_read_register(0);

    if(pmic_prod_id == 0x22)
    {
        smc_set_power_gk(target, voltage_mv, regs);
    }
    else if(pmic_prod_id == 0x20)
    {
        smc_set_power_ev1(target, voltage_mv, regs);
    }
    else
    {
        klog("SM: smc_set_power: unknown product id: %x\n", pmic_prod_id);
        regs->x0 = ~0ULL;
    }
}

void smc_set_power_gk(SMC_Power_Target target, unsigned int voltage_mv, exception_regs *regs)
{
    switch(target)
    {
        case SMC_Power_Target::Core:
            smc_set_power_target(pmic_vreg::Buck, 2, voltage_mv, 670, 842, regs);
            break;

        case SMC_Power_Target::CPU:
            smc_set_power_target(pmic_vreg::Buck, 1, voltage_mv, 640, 935, regs);
            break;

        case SMC_Power_Target::GPU:
            smc_set_power_target(pmic_vreg::Buck, 3, voltage_mv, 760, 961, regs);
            break;

        case SMC_Power_Target::SDCard:
            smc_set_power_target(pmic_vreg::LDO, 7, voltage_mv, 1800, 3300, regs);
            break;

        case SMC_Power_Target::SDCard_IO:
            smc_set_power_target(pmic_vreg::LDO, 8, voltage_mv, 1800, 3300, regs);
            break;

        case SMC_Power_Target::SDIO_IO:
            smc_set_power_target(pmic_vreg::LDO, 5, voltage_mv, 1800, 3300, regs);
            break;

        case SMC_Power_Target::Flash:
            smc_set_power_target(pmic_vreg::LDO, 2, voltage_mv, 1200, 3300, regs);
            break;

        case SMC_Power_Target::Audio:
            smc_set_power_target(pmic_vreg::LDO, 6, voltage_mv, 1200, 3300, regs);
            break;

        case SMC_Power_Target::USB:
            smc_set_power_target(pmic_vreg::LDO, 4, voltage_mv, 3300, 3300, regs);
            break;
    }
}

void smc_set_power_ev1(SMC_Power_Target target, unsigned int voltage_mv, exception_regs *regs)
{
    switch(target)
    {
        case SMC_Power_Target::Core:
            smc_set_power_target(pmic_vreg::Buck, 2, voltage_mv, 670, 842, regs);
            break;

        case SMC_Power_Target::CPU:
            smc_set_power_target(pmic_vreg::Buck, 1, voltage_mv, 640, 935, regs);
            break;

        case SMC_Power_Target::GPU:
            smc_set_power_target(pmic_vreg::Buck, 3, voltage_mv, 760, 961, regs);
            break;

        case SMC_Power_Target::SDCard:
            smc_set_power_target(pmic_vreg::LDO, 7, voltage_mv, 1800, 3300, regs);
            break;

        case SMC_Power_Target::SDCard_IO:
            smc_set_power_target(pmic_vreg::LDO, 8, voltage_mv, 1800, 3300, regs);
            break;

        case SMC_Power_Target::USB:
            smc_set_power_target(pmic_vreg::LDO, 4, voltage_mv, 3300, 3300, regs);
            break;

        default:
            klog("SM: set_power: target %u not supported on EV1\n", target);
            regs->x0 = ~0ULL;
            break;
    }
}

void smc_set_power_target(pmic_vreg::_type type, unsigned int id, unsigned int voltage_mv,
    unsigned int voltage_mv_min, unsigned int voltage_mv_max, exception_regs *regs)
{
    if(voltage_mv && (voltage_mv < voltage_mv_min))
    {
        klog("SM: set_power: voltage %u too low for target %llu (%u - %u mV)\n", voltage_mv,
            regs->x0, voltage_mv_min, voltage_mv_max);
        regs->x0 = ~0ULL - 1ULL;
        return;
    }
    if(voltage_mv > voltage_mv_max)
    {
        klog("SM: set_power: voltage %u too high for target %llu (%u - %u mV)\n", voltage_mv,
            regs->x0, voltage_mv_min, voltage_mv_max);
        regs->x0 = ~0ULL - 2ULL;
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
                regs->x0 = pmic_readback.is_enabled ? pmic_readback.mv : 0;
            }
            break;

        case pmic_vreg::LDO:
            {
                auto pmic_readback = pmic_get_ldo(id);
                pmic_dump(pmic_readback);
                regs->x0 = pmic_readback.is_enabled ? pmic_readback.mv : 0;
            }
            break;

        default:
            regs->x0 = ~0ULL - 3ULL;
    }
}
