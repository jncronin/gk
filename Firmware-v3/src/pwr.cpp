#include "stm32h7rsxx.h"

extern "C" int pwr_disable_regulators()
{
    // PWR init
    auto pwr_csr2 = PWR->CSR2;
    pwr_csr2 &= ~0xffU;
    pwr_csr2 |= PWR_CSR2_BYPASS;
    PWR->CSR2 = pwr_csr2;

    return 0;
}
