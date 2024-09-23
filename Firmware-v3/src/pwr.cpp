#include "stm32h7rsxx.h"
#include "i2c.h"
#include "gk_conf.h"

extern "C" INTFLASH_FUNCTION int pwr_disable_regulators()
{
    // PWR init
    auto pwr_csr2 = PWR->CSR2;
    pwr_csr2 &= ~0xffU;
    pwr_csr2 |= PWR_CSR2_BYPASS;
    PWR->CSR2 = pwr_csr2;

    return 0;
}

INTFLASH_FUNCTION int pwr_set_vos_high()
{
    // boost VCORE
    uint8_t dvb2a = 0;
    auto i2c_ret = i2c_register_read(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1);
    if(i2c_ret == 1)
    {
        dvb2a &= ~0x1fU;
        dvb2a |= 0x1fU;

        i2c_register_write(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1);

        // check for success
        PWR->CSR4 |= PWR_CSR4_VOS;

        if(i2c_register_read(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1) == 1 &&
            ((dvb2a & 0x1fU) == 0x1fU))
        {
            return 0;
        }
    }

    return -1;
}
