#include "smc.h"

int smc_set_power(SMC_Power_Target target, unsigned int voltage_mv)
{
    int ret;
    __asm__ volatile(
        "mov x0, %[target]\n"
        "mov x1, %[voltage_mv]\n"
        "smc %[smc_id]\n"
        "mov %[ret], x0\n" :
        [ret] "=r" (ret) :
        [target] "r" (target),
        [voltage_mv] "r" (voltage_mv),
        [smc_id] "i" ((int)SMC_Call::SetPower) :
        "x0", "x1"
    );
    return ret;
}
