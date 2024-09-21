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
uint32_t cr0 = 0;
uint32_t cr1 = 0;

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
        |
        XSPI_CR_DMM;    // TODO: add timeout
    XSPI1->DCR1 = (5UL << XSPI_DCR1_MTYP_Pos) |
        (1UL << XSPI_DCR1_CSHT_Pos) |
        (26UL << XSPI_DCR1_DEVSIZE_Pos);
    XSPI1->DCR3 = (25UL << XSPI_DCR3_CSBOUND_Pos);      // cannot wrap > 1/2 of each chip (2 dies per chip)
    XSPI1->DCR4 = 0;
    XSPI1->DCR2 = (5UL << XSPI_DCR2_WRAPSIZE_Pos) |     // TODO: burst
        (1UL << XSPI_DCR2_PRESCALER_Pos); 
    //while(XSPI1->SR & XSPI_SR_BUSY);
    //XSPI1->CR |= XSPI_CR_DMM;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CCR = 
        (4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_CCR_DMODE_Pos) |
        //(4UL << XSPI_CCR_ABMODE_Pos) |
        //(4UL << XSPI_CCR_IMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_IDTR | XSPI_CCR_ADDTR | XSPI_CCR_ABDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WCCR = XSPI_WCCR_DQSE |
        (4UL << XSPI_WCCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_WCCR_DMODE_Pos) |
        //(4UL << XSPI_WCCR_ABMODE_Pos) |
        //(4UL << XSPI_WCCR_IMODE_Pos) |
        XSPI_WCCR_DDTR | XSPI_WCCR_IDTR | XSPI_WCCR_ADDTR | XSPI_WCCR_ABDTR;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->WPCCR =
        (4UL << XSPI_WPCCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_WPCCR_DMODE_Pos) |
        //(4UL << XSPI_WCCR_ABMODE_Pos) |
        //(4UL << XSPI_WCCR_IMODE_Pos) |
        XSPI_WPCCR_DDTR | XSPI_WPCCR_IDTR | XSPI_WPCCR_ADDTR | XSPI_WPCCR_ABDTR;
    //XSPI1->CCR = XSPI_CCR_DQSE;
    //XSPI1->WCCR = XSPI_WCCR_DQSE | XSPI_WCCR_DDTR |
    //    (4UL << XSPI_WCCR_DMODE_Pos);
    //XSPI1->TCR = XSPI_TCR_DHQC;
    //XSPI1->WTCR = XSPI_TCR_DHQC;
    //XSPI1->WPTCR = XSPI_TCR_DHQC;
    //XSPI1->CR |= XSPI_CR_EN;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->HLCR = (7UL << XSPI_HLCR_TRWR_Pos) |
        (7UL << XSPI_HLCR_TACC_Pos) |
        XSPI_HLCR_LM;

    // Do some indirect register reads to prove we're connected
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DLR = 7; // 2 bytes per register per die per chip
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CCR = 
        (4UL << XSPI_CCR_ADMODE_Pos) | // 8 address lines
        (4UL << XSPI_CCR_DMODE_Pos) |
        //(4UL << XSPI_CCR_ABMODE_Pos) |
        //(4UL << XSPI_CCR_IMODE_Pos) |
        XSPI_CCR_DDTR | XSPI_CCR_IDTR | XSPI_CCR_ADDTR | XSPI_CCR_ABDTR;

    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->AR = 0;  // ID reg 0
    int i = 0;
    while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
    {
        while(XSPI1->SR & XSPI_SR_FLEVEL)
        {
            id0 = *(volatile uint32_t *)&XSPI1->DR;
            SEGGER_RTT_printf(0, "id0 (%d): %x\n", i++, id0);
        }
    }
    XSPI1->FCR = XSPI_FCR_CTCF;

    if(id0 != 0x860F860F)
    {
        // try again
        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->AR = 0;  // ID reg 0
        i = 0;
        while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
        {
            while(XSPI1->SR & XSPI_SR_FLEVEL)
            {
                id0 = *(volatile uint32_t *)&XSPI1->DR;
                SEGGER_RTT_printf(0, "id0 (%d): %x\n", i++, id0);
            }
        }
        XSPI1->FCR = XSPI_FCR_CTCF;

        if(id0 != 0x860F860F)
        {
            __asm__ volatile("bkpt \n" ::: "memory");
        }
    }

    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->AR = 4;  // ID reg 1
    i = 0;
    while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
    {
        while(XSPI1->SR & XSPI_SR_FLEVEL)
        {
            id1 = *(volatile uint32_t *)&XSPI1->DR;
            SEGGER_RTT_printf(0, "id1 (%d): %x\n", i++, id1);
        }
    }
    XSPI1->FCR = XSPI_FCR_CTCF;

    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->AR = 0x800*4;  // control reg 0
    i = 0;
    while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
    {
        while(XSPI1->SR & XSPI_SR_FLEVEL)
        {
            cr0 = *(volatile uint32_t *)&XSPI1->DR;
            SEGGER_RTT_printf(0, "cr0 (%d): %x\n", i++, cr0);
        }
    }
    XSPI1->FCR = XSPI_FCR_CTCF;

    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->AR = 0x801*4;  // control reg 1
    i = 0;
    while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
    {
        while(XSPI1->SR & XSPI_SR_FLEVEL)
        {
            cr1 = *(volatile uint32_t *)&XSPI1->DR;
            SEGGER_RTT_printf(0, "cr1 (%d): %x\n", i++, cr1);
        }
    }
    XSPI1->FCR = XSPI_FCR_CTCF;

    // Try and enable differential clock...
    uint32_t singleclk = 0x40004000;
    if(cr1 & singleclk)
    {
        SEGGER_RTT_printf(0, "xspi: enabling differential clk\n");

        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->CR &= ~XSPI_CR_FMODE;        // indirect write mode

        while(XSPI1->SR & XSPI_SR_BUSY);
        // don't use any latency in register write mode
        XSPI1->HLCR |= XSPI_HLCR_WZL;

        // don't use RWDS in register write mode
        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->WCCR &= ~XSPI_WCCR_DQSE;

        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->AR = 0x801U*4;
        *(volatile uint32_t *)&XSPI1->DR = 0xff81ff81;  // register writes are big endian
        *(volatile uint32_t *)&XSPI1->DR = 0xff81ff81;  // write to all 4 dies (2 per chip)
        while((XSPI1->SR & XSPI_SR_TCF) == 0);
        XSPI1->FCR = XSPI_FCR_CTCF;

        // enable hybrid burst 128 bytes -- TODO put this outside the singleclk check
        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->AR = 0x800U*4;
        *(volatile uint32_t *)&XSPI1->DR = 0x8f288f28;  // register writes are big endian
        *(volatile uint32_t *)&XSPI1->DR = 0x8f288f28;  // write to all 4 dies (2 per chip)
        while((XSPI1->SR & XSPI_SR_TCF) == 0);
        XSPI1->FCR = XSPI_FCR_CTCF;


        // Now confirm it is enabled
        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->CR |= 1U << XSPI_CR_FMODE_Pos;        // indirect read mode

        while(XSPI1->SR & XSPI_SR_BUSY);
        // reenable latency in write mode
        XSPI1->HLCR &= ~XSPI_HLCR_WZL;

        // reenable RWDS in write mode
        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->WCCR |= XSPI_WCCR_DQSE;

        while(XSPI1->SR & XSPI_SR_BUSY);
        XSPI1->AR = 0x801U*4;  // control reg 1
        i = 0;
        while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
        {
            while(XSPI1->SR & XSPI_SR_FLEVEL)
            {
                cr1 = *(volatile uint32_t *)&XSPI1->DR;
                SEGGER_RTT_printf(0, "cr1 (%d): %x\n", i++, cr1);
            }
        }
        XSPI1->FCR = XSPI_FCR_CTCF;

        if(cr1 == 0)
        {
            while(XSPI1->SR & XSPI_SR_BUSY);
            XSPI1->AR = 0x801*4;  // control reg 1
            i = 0;
            while((XSPI1->SR & XSPI_SR_TCF) == 0 || (XSPI1->SR & XSPI_SR_FLEVEL))
            {
                while(XSPI1->SR & XSPI_SR_FLEVEL)
                {
                    cr1 = *(volatile uint32_t *)&XSPI1->DR;
                    SEGGER_RTT_printf(0, "cr1 (%d): %x\n", i++, cr1);
                }
            }
            XSPI1->FCR = XSPI_FCR_CTCF;
        }
    }

    // set XSPI1 to memory mapped mode
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->CR |= 3U << XSPI_CR_FMODE_Pos;
    while(XSPI1->SR & XSPI_SR_BUSY);
    XSPI1->DCR1 = (XSPI1->DCR1 & ~XSPI_DCR1_MTYP_Msk) | (4U << XSPI_DCR1_MTYP_Pos);
    while(XSPI1->SR & XSPI_SR_BUSY);




    /*XSPI1->AR = 2;  // ID reg 1
    while((XSPI1->SR & XSPI_SR_TCF) == 0);
    XSPI1->FCR = XSPI_FCR_CTCF;
    i = 0;
    while(XSPI1->SR & XSPI_SR_FLEVEL)
    {
        id1 = *(volatile uint16_t *)&XSPI1->DR;
        SEGGER_RTT_printf(0, "id1 (%d): %x\n", i++, id1);
    }*/

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