#include <stm32h7rsxx.h>
#include "pins.h"

static const constexpr pin XSPI_PINS[] =
{
    { GPION, 0, 9 },
    { GPION, 1, 9 },
    { GPION, 2, 9 },
    { GPION, 3, 9 },
    { GPION, 4, 9 },
    { GPION, 5, 9 },
    { GPION, 6, 9 },
    { GPION, 7, 9 },
    { GPION, 8, 9 },
    { GPION, 9, 9 },
    { GPION, 10, 9 },
    { GPION, 11, 9 },
    { GPIOO, 0, 9 },
    { GPIOO, 2, 9 },
    { GPIOO, 3, 9 },
    { GPIOO, 4, 9 },
    { GPIOO, 5, 9 },
    { GPIOP, 0, 9 },
    { GPIOP, 1, 9 },
    { GPIOP, 2, 9 },
    { GPIOP, 3, 9 },
    { GPIOP, 4, 9 },
    { GPIOP, 5, 9 },
    { GPIOP, 6, 9 },
    { GPIOP, 7, 9 },
    { GPIOP, 8, 9 },
    { GPIOP, 9, 9 },
    { GPIOP, 10, 9 },
    { GPIOP, 11, 9 },
    { GPIOP, 12, 9 },
    { GPIOP, 13, 9 },
    { GPIOP, 14, 9 },
    { GPIOP, 15, 9 },
};

static const constexpr pin XSPI1_RESET { GPIOD, 1 };
static const constexpr pin XSPI2_RESET { GPIOD, 0 };

extern "C" int init_xspi()
{
    // pin setup
    for(const auto &p : XSPI_PINS)
    {
        p.set_as_af();
    }
    XSPI1_RESET.clear();
    XSPI2_RESET.clear();
    XSPI1_RESET.set_as_output();
    XSPI2_RESET.set_as_output();

    for(int i = 0; i < 10000; i++)
    {
        __DMB();
    }
    XSPI1_RESET.set();
    XSPI2_RESET.set();

    // Init XSPI controller
    RCC->AHB5ENR |= RCC_AHB5ENR_XSPI1EN | RCC_AHB5ENR_XSPI2EN | RCC_AHB5ENR_XSPIMEN;
    (void)RCC->AHB5ENR;

    XSPIM->CR = 0;  // direct mode

    /* XSPI1 - dual-octal HyperBus 2x 64MByte, 166 MHz */
    XSPI1->CR = (3UL << XSPI_CR_FMODE_Pos) |
        XSPI_CR_DMM;    // TODO: add timeout
    XSPI1->DCR1 = (4UL << XSPI_DCR1_MTYP_Pos) |
        (26UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI1->DCR2 =                           // TODO: burst
        (1UL << XSPI_DCR2_PRESCALER_Pos); // TODO: use PLL2, 166 MHz and passthrough
    XSPI1->DCR3 = 0;
    XSPI1->DCR4 = 0;
    XSPI1->CR |= XSPI_CR_EN;

    /* XSPI2 - octal HyperBus 64 MByte, 166 MHz */
    XSPI2->CR = (3UL << XSPI_CR_FMODE_Pos);    // TODO: add timeout
    XSPI2->DCR1 = (4UL << XSPI_DCR1_MTYP_Pos) |
        (25UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI2->DCR2 =                           // TODO: burst
        (1UL << XSPI_DCR2_PRESCALER_Pos); // TODO: use PLL2, 166 MHz and passthrough
    XSPI2->DCR3 = 0;
    XSPI2->DCR4 = 0;
    XSPI2->CR |= XSPI_CR_EN;

    return 0;
}