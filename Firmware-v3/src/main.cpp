#include <stm32h7rsxx.h>
#include <cstring>
#include "pins.h"
#include "i2c.h"
#include "SEGGER_RTT.h"

uint32_t test_val;

uint32_t test_range[256];

static const constexpr pin CTP_NRESET { GPIOC, 0 };
extern "C" void init_xspi();

int main()
{
    // enable CSI for compensation cell
    RCC->CR |= RCC_CR_CSION;
    while(!(RCC->CR & RCC_CR_CSIRDY));

    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;

    SBS->CCCSR |= SBS_CCCSR_COMP_EN | SBS_CCCSR_XSPI1_COMP_EN |
        SBS_CCCSR_XSPI2_COMP_EN;

    init_xspi();

    SCB_InvalidateDCache();
    SCB_InvalidateICache();
    SCB_EnableICache();
    SCB_EnableDCache();
    
    *(volatile uint32_t *)0x90000000 = 0xdeadbeef;
    *(volatile uint32_t *)0x90000004 = 0xaabbccdd;
    *(volatile uint32_t *)0x90000008 = 0x11223344;
    test_val = *(volatile uint32_t *)0x90000000;

    memcpy(test_range, (void *)0x90000000, 4*256);
    CTP_NRESET.set_as_output();
    CTP_NRESET.clear();
    for(int i = 0; i < 100000; i++) __DMB();

    //i2c_test();

    uint8_t dvb4a = 0;
    auto i2c_ret = i2c_register_read(0x78 >> 1, (uint8_t)0x0a, &dvb4a, 1);
    SEGGER_RTT_printf(0, "pwr: ret: %d, dvb4a: %d\n", i2c_ret, dvb4a);

    if(i2c_ret == 1)
    {
        dvb4a &= ~0x1fU;
        dvb4a |= 0x19U;     // set VCCA to default

        i2c_ret = i2c_register_write(0x78 >> 1, (uint8_t)0x0a, &dvb4a, 1);
        SEGGER_RTT_printf(0, "pwr: ret: %d, dvb4a: %d\n", i2c_ret, dvb4a);
    }

    // check current vcore
    for(int i = 0; i < 1000000; i++) __DMB();
    SEGGER_RTT_printf(0, "PWR->SR1: %x\n", PWR->SR1);

    // boost VCORE
    uint8_t dvb2a = 0;
    i2c_ret = i2c_register_read(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1);
    if(i2c_ret == 1)
    {
        dvb2a &= ~0x1fU;
        dvb2a |= 0x1fU;

        i2c_register_write(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1);

        // check for success
        PWR->CSR4 |= PWR_CSR4_VOS;
        for(int i = 0; i < 1000000; i++) __DMB();
        SEGGER_RTT_printf(0, "PWR->SR1: %x\n", PWR->SR1);
        SEGGER_RTT_printf(0, "PWR->CSR2: %x\n", PWR->CSR2);
    }

    while(true);

    return 0;
}
