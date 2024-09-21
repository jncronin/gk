#include <stm32h7rsxx.h>
#include "pins.h"

#include "SEGGER_RTT.h"

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

uint32_t id0 = 0;
uint32_t id1 = 0;

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

    for(int i = 0; i < 1000000; i++)
    {
        __DMB();
    }
    XSPI1_RESET.set();
    XSPI2_RESET.set();
    for(int i = 0; i < 1000000; i++)
    {
        __DMB();
    }

    // Init XSPI controller
    RCC->AHB5ENR |= RCC_AHB5ENR_XSPI1EN | RCC_AHB5ENR_XSPI2EN | RCC_AHB5ENR_XSPIMEN;
    (void)RCC->AHB5ENR;
    RCC->AHB5RSTR = RCC_AHB5RSTR_XSPI1RST | RCC_AHB5RSTR_XSPI2RST | RCC_AHB5RSTR_XSPIMRST;
    (void)RCC->AHB5RSTR;
    RCC->AHB5RSTR = 0;
    (void)RCC->AHB5RSTR;

    // Power to XSPI pins
    PWR->CSR2 |= PWR_CSR2_EN_XSPIM1 | PWR_CSR2_EN_XSPIM2;

    XSPIM->CR = 0;  // direct mode

    /* XSPI1 - dual-octal HyperBus 2x 64MByte, 200 MHz
        Default:
            - initial latency 7 clk
            - fixed latency - 2 times initial
            - legacy wrapped burst
            - burst length 32 bytes
     */
    XSPI1->CR = (1UL << XSPI_CR_FMODE_Pos) | XSPI_CR_EN
        /*|
        XSPI_CR_DMM*/;    // TODO: add timeout
    XSPI1->DCR1 = (5UL << XSPI_DCR1_MTYP_Pos) |
        (1UL << XSPI_DCR1_CSHT_Pos) |
        (26UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI1->DCR3 = (25UL << XSPI_DCR3_CSBOUND_Pos);      // cannot wrap > 1/2 of each chip (2 dies per chip)
    XSPI1->DCR4 = 0;
    XSPI1->DCR2 = (0UL << XSPI_DCR2_WRAPSIZE_Pos) |     // TODO: burst
        (1UL << XSPI_DCR2_PRESCALER_Pos); // TODO: use PLL2, 200 MHz and passthrough
    while(XSPI1->SR & XSPI_SR_BUSY);
    //XSPI1->CR |= XSPI_CR_DMM;
    //XSPI1->CCR = XSPI_CCR_DDTR |
    //    (4UL << XSPI_CCR_DMODE_Pos);
    XSPI1->WCCR = XSPI_WCCR_DQSE;
    //XSPI1->CCR = XSPI_CCR_DQSE;
    //XSPI1->WCCR = XSPI_WCCR_DQSE | XSPI_WCCR_DDTR |
    //    (4UL << XSPI_WCCR_DMODE_Pos);
    //XSPI1->TCR = XSPI_TCR_DHQC;
    XSPI1->CR |= XSPI_CR_EN;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->HLCR = (7UL << XSPI_HLCR_TRWR_Pos) |
        (7UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_LM;

    // Do some indirect register reads to prove we're connected
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DLR = 1; // 2 bytes per register
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CCR = 
        (4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_CCR_DMODE_Pos) |
        (4UL << XSPI_CCR_ABMODE_Pos) |
        (4UL << XSPI_CCR_IMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_IDTR | XSPI_CCR_ADDTR | XSPI_CCR_ABDTR;   
    while(XSPI1->SR & XSPI_SR_BUSY);

    XSPI1->AR = 0;  // ID reg 0
    while((XSPI1->SR & XSPI_SR_TCF) == 0);
    XSPI1->FCR = XSPI_FCR_CTCF;
    int i = 0;
    while(XSPI1->SR & XSPI_SR_FLEVEL)
    {
        id0 = *(volatile uint16_t *)&XSPI1->DR;
        SEGGER_RTT_printf(0, "id0 (%d): %x\n", i++, id0);
    }

    XSPI1->AR = 2;  // ID reg 1
    while((XSPI1->SR & XSPI_SR_TCF) == 0);
    XSPI1->FCR = XSPI_FCR_CTCF;
    i = 0;
    while(XSPI1->SR & XSPI_SR_FLEVEL)
    {
        id1 = *(volatile uint16_t *)&XSPI1->DR;
        SEGGER_RTT_printf(0, "id1 (%d): %x\n", i++, id1);
    }

    //__asm__ volatile("bkpt \n" ::: "memory");


    /* XSPI2 - octal HyperBus 64 MByte, 166 MHz */
    XSPI2->CR = (3UL << XSPI_CR_FMODE_Pos);    // TODO: add timeout
    XSPI2->DCR1 = (4UL << XSPI_DCR1_MTYP_Pos) |
        (25UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI2->DCR2 =                           // TODO: burst
        (1UL << XSPI_DCR2_PRESCALER_Pos); // TODO: use PLL2, 166 MHz and passthrough
    XSPI2->DCR3 = 0;
    XSPI2->DCR4 = 0;
    XSPI2->CCR = XSPI_CCR_DQSE;
    XSPI2->CR |= XSPI_CR_EN;

    return 0;
}