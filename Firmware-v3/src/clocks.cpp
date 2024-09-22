#include <stm32h7rsxx.h>
#include "pwr.h"

extern uint64_t _cur_ms;
extern struct timespec toffset;

static void enable_backup_domain();

extern "C" void init_clocks()
{
    /* Clock syscfg/sbs */
    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;

    /* Set up the clock system
        We have a 24 MHz HSE crystal oscillator and 64 MHz HSI */
    
    // Enable HSE
    auto cr = RCC->CR;
    cr |= RCC_CR_HSEON | RCC_CR_HSECSSON;
    cr &= ~RCC_CR_HSEBYP;
    RCC->CR = cr;
    while(!(RCC->CR & RCC_CR_HSERDY));

    enable_backup_domain();

    _cur_ms = 0ULL;

    /* Configure PLLs from 24 MHz HSE:
        PLL1 = HSE24 / M3 * 75 = 600
            /P1 = 600 -> SYSCLK 
            
    */

    // Ensure sysclk is HSI64 and PLLs disabled
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    while(RCC->CFGR & RCC_CFGR_SWS);
    RCC->CR &= ~(RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(RCC->CR & (RCC_CR_PLL1RDY | RCC_CR_PLL2RDY | RCC_CR_PLL3RDY));
    RCC->CR &= ~RCC_CR_HSIDIV;

    // Set vreg to output higher VCORE

    // for I2C set clk to HSI64
    // boost VCORE
    bool vos = pwr_set_vos_high() == 0;

    // set PLL M dividers
    RCC->PLLCKSELR = (3U << RCC_PLLCKSELR_DIVM3_Pos) |
        (3U << RCC_PLLCKSELR_DIVM2_Pos) |
        (3U << RCC_PLLCKSELR_DIVM1_Pos) |
        (2U << RCC_PLLCKSELR_PLLSRC_Pos);
    
    RCC->PLLCFGR = 0U;
    RCC->PLL1FRACR = 0U;
    RCC->PLL2FRACR = 0U;
    RCC->PLL3FRACR = 0U;

    /* PLL1 -> CPU at 600 (VOS high -> 8*75/1) or 400 (VOS low -> 8*100/2) */
    RCC->PLL1DIVR1 = (1U << RCC_PLL1DIVR1_DIVQ_Pos) |
        ((vos ? 0U : 1U) << RCC_PLL1DIVR1_DIVP_Pos) |
        ((vos ? 74U : 99U) << RCC_PLL1DIVR1_DIVN_Pos);
    RCC->PLL1DIVR2 = (1U << RCC_PLL1DIVR2_DIVS_Pos);

    /* PLL2:
        P -> SPI2,3 @ 200 MHz
        Q -> SPI4,5,6 @ 200 MHz
        S -> XSPI1 @ 266 MHz (then prescaled/2 - max XSPI without DQS is 135 MHz)
        T -> XSPI2 and SD @ 160 MHz */
    RCC->PLL2DIVR1 = (1U << RCC_PLL2DIVR1_DIVR_Pos) |
        (3U << RCC_PLL2DIVR1_DIVQ_Pos) |
        (3U << RCC_PLL2DIVR1_DIVP_Pos) |
        (99U << RCC_PLL2DIVR1_DIVN_Pos);
    RCC->PLL2DIVR2 = (4U << RCC_PLL2DIVR2_DIVT_Pos) |
        (2U << RCC_PLL2DIVR2_DIVS_Pos);

    /* PLL3:
        R -> LTDC @24 MHz (60 Hz * 800 * 500) */
    RCC->PLL3DIVR1 = (19U << RCC_PLL3DIVR1_DIVR_Pos) |
        (1U << RCC_PLL3DIVR1_DIVQ_Pos) |
        (1U << RCC_PLL3DIVR1_DIVP_Pos) |
        (59U << RCC_PLL3DIVR1_DIVN_Pos);
    RCC->PLL3DIVR2 = (1U << RCC_PLL3DIVR2_DIVS_Pos);

    /* Enable the requested outputs */
    RCC->PLLCFGR = RCC_PLLCFGR_PLL3REN |
        RCC_PLLCFGR_PLL2TEN |
        RCC_PLLCFGR_PLL2QEN |
        RCC_PLLCFGR_PLL2PEN |
        RCC_PLLCFGR_PLL2SEN |
        RCC_PLLCFGR_PLL1PEN |
        (3U << RCC_PLLCFGR_PLL3RGE_Pos) |
        (3U << RCC_PLLCFGR_PLL2RGE_Pos) |
        (3U << RCC_PLLCFGR_PLL1RGE_Pos);

    RCC->CR |= (RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(!(RCC->CR & RCC_CR_PLL1RDY));
    while(!(RCC->CR & RCC_CR_PLL2RDY));
    while(!(RCC->CR & RCC_CR_PLL3RDY));

    /* Prepare for ssyclk switch.
        Sysclk will be 600 (or 400) MHz
        /CPRE = 1 -> CPUCLK 600
        /BMPRE = 2 -> HCLK  300
        /PPRE[1,2,4,5] = 2 -> APB clocks 150 */
    RCC->CDCFGR = 0U << RCC_CDCFGR_CPRE_Pos;
    RCC->BMCFGR = 8U << RCC_BMCFGR_BMPRE_Pos;
    RCC->APBCFGR = (4U << RCC_APBCFGR_PPRE5_Pos) |
        (4U << RCC_APBCFGR_PPRE4_Pos) |
        (4U << RCC_APBCFGR_PPRE2_Pos) |
        (4U << RCC_APBCFGR_PPRE1_Pos);

    /* Set up internal flash wait states */
    if(vos)
    {
        FLASH->ACR = (3U << FLASH_ACR_WRHIGHFREQ_Pos) |
            (7U << FLASH_ACR_LATENCY_Pos);
    }
    else
    {
        FLASH->ACR = (2U << FLASH_ACR_WRHIGHFREQ_Pos) |
            (5U << FLASH_ACR_LATENCY_Pos);
    }

    /* Set sysclk to use PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) |
        (3U << RCC_CFGR_SW_Pos);
    while((RCC->CFGR & RCC_CFGR_SWS_Msk) != (3U << RCC_CFGR_SWS_Pos));

    /* Configure peripheral clocks */
    RCC->CCIPR1 = (2U << RCC_CCIPR1_CKPERSEL_Pos) |     // PERCLK = HSE24
        (2U << RCC_CCIPR1_ADCSEL_Pos) |                 // ADC = HSE24
        (3U << RCC_CCIPR1_OTGFSSEL_Pos) |               // OTGFS from USBPHYC
        (0U << RCC_CCIPR1_USBPHYCSEL_Pos) |             // USBPHY from HSE
        (0xaU << RCC_CCIPR1_USBREFCKSEL_Pos) |          // USBPHY is 24 MHz
        (2U << RCC_CCIPR1_XSPI2SEL_Pos) |               // XSPI2 from PLL2T=160MHz
        (1U << RCC_CCIPR1_XSPI1SEL_Pos) |               // XSPI1 from PLL2S=400MHz
        (1U << RCC_CCIPR1_SDMMC12SEL_Pos);              // SDMMC from PLL2T=160MHz
    RCC->CCIPR2 = (5U << RCC_CCIPR2_LPTIM1SEL_Pos) |    // LPTIM1 = HSE24
        (2U << RCC_CCIPR2_I2C1_I3C1SEL_Pos) |           // I2C1 = HSI64
        (2U << RCC_CCIPR2_I2C23SEL_Pos) |               // I2C2/3 = HSI64
        (1U << RCC_CCIPR2_SPI23SEL_Pos) |               // SPI2/3 = PLL2P=200
        (3U << RCC_CCIPR2_UART234578SEL_Pos);           // UARTs = HSI64
    RCC->CCIPR3 = 0;    // TODO: SAI needs I2S_CKIN to be running before selecting it
    RCC->CCIPR4 = (5U << RCC_CCIPR4_LPTIM45SEL_Pos) |   // LPTIM4,5 = HSE24
        (5U << RCC_CCIPR4_LPTIM23SEL_Pos) |             // LPTIM2,3 = HSE24
        (1U << RCC_CCIPR4_SPI6SEL_Pos) |                // SPI6  = PLL2Q=200
        (3U << RCC_CCIPR4_LPUART1SEL_Pos);              // LPUART = HSI64

}

void enable_backup_domain()
{

}