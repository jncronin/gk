#include <stm32mp2xx.h>

#include "pins.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

void init_clocks();

// This points to the boot_api_context_t structure which we do not use.
//  Pass it on to the ssbl-a
uint32_t _bootrom_val;

static const constexpr pin QSPI_PINS[]
{
    { GPIOD, 3, 10 },
    { GPIOD, 0, 10 },
    { GPIOD, 4, 10 },
    { GPIOD, 5, 10 },
    { GPIOD, 6, 10 },
    { GPIOD, 7, 10 }
};

int main(uint32_t bootrom_val)
{
    _bootrom_val = bootrom_val;

    // Set up clocks so that we can get a nice fast clock for QSPI
    init_clocks();

    // Enable QSPI XIP
    for(const auto &p : QSPI_PINS)
    {
        p.set_as_af();
    }
    RCC->OSPIIOMCFGR |= RCC_OSPIIOMCFGR_OSPIIOMEN;
    RCC->OSPI1CFGR |= RCC_OSPI1CFGR_OSPI1EN;
    (void)RCC->OSPI1CFGR;

    OCTOSPIM->CR = 0;
    OCTOSPI1->CR = 0;
    OCTOSPI1->DCR1 = (2U << OCTOSPI_DCR1_MTYP_Pos) |
        (21U << OCTOSPI_DCR1_DEVSIZE_Pos) |     // 4 MBytes/32 Mb - we use W25Q32JV in production
        (0x3fU << OCTOSPI_DCR1_CSHT_Pos) |
        OCTOSPI_DCR1_DLYBYP;
    OCTOSPI1->DCR2 = (1U << OCTOSPI_DCR2_PRESCALER_Pos);        // 100 MHz/2 => 50 MHz
    OCTOSPI1->DCR3 = 0;
    OCTOSPI1->DCR4 = 0;
    OCTOSPI1->FCR = 0xdU;

    OCTOSPI1->CCR = (1U << OCTOSPI_CCR_DMODE_Pos) |
        (0U << OCTOSPI_CCR_ABMODE_Pos) |
        (2U << OCTOSPI_CCR_ADSIZE_Pos) |
        (1U << OCTOSPI_CCR_ADMODE_Pos) |
        (1U << OCTOSPI_CCR_IMODE_Pos);
    OCTOSPI1->TCR = 0U;
    OCTOSPI1->IR = 0x03U;


    OCTOSPI1->CR = (3U << OCTOSPI_CR_FMODE_Pos) |
        OCTOSPI_CR_EN;

    
    EV_ORANGE.set_as_output();

    // say hi
    for(int n = 0; n < 10; n++)
    {
        EV_ORANGE.set();
        for(int i = 0; i < 2500000; i++);
        EV_ORANGE.clear();
        for(int i = 0; i < 2500000; i++);
    }

    void (*ssbl)(uint32_t bootrom_val) = (void (*)(uint32_t))0x60100000;
    ssbl(bootrom_val);

    return 0;
}
