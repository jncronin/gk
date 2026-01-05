#include "pmic.h"
#include "smc.h"
#include "logger.h"

int smc_set_power(SMC_Power_Target t, unsigned int v)
{
    auto id = pmic_read_register(0);
    if(id != 0x22)
    {
        klog("smc_set_power: invalid id: %x\n", id);
        return -1;
    }

    int ldo_id = -1;
    
    switch(t)
    {
        case SMC_Power_Target::SDCard:
            ldo_id = 7;
            break;
        case SMC_Power_Target::SDCard_IO:
            ldo_id = 8;
            break;
        case SMC_Power_Target::SDIO_IO:
            ldo_id = 5;
            break;
        default:
            klog("smc_set_power: unsupported %d\n", t);
            return -1;
    }

    pmic_vreg ldo { pmic_vreg::LDO, ldo_id, v != 0, (int)v, pmic_vreg::Normal };
    pmic_set(ldo);
    return v;
}
