#include <stm32h7xx.h>

#include "clocks.h"

void init_clocks()
{
    /* Enable clock to the SYSCFG registers */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    /* Set up the clock system
        We have a 16 MHz HSE oscillator

        Use this as the input to all 3 PLLs (PLLM - 180 Mhz for main, PLLSAI - 48 MHz for SD/USB, PLL12S - 180 MHz for I2S)
    */

    // Enable RTC to use HSE/16
    RCC->CFGR &= ~RCC_CFGR_RTCPRE_Msk;
    RCC->CFGR |= (16UL << RCC_CFGR_RTCPRE_Pos);

    // First reset backup domain and allow write access
    RCC->BDCR = RCC_BDCR_BDRST;
    (void)RCC->BDCR;
    RCC->BDCR = 0;
    (void)RCC->BDCR;
    PWR->CR1 |= PWR_CR1_DBP;

    RCC->BDCR = RCC_BDCR_RTCEN |
        (3UL << RCC_BDCR_RTCSEL_Pos);

    // Enable HSE
    auto cr = RCC->CR;
    cr |= RCC_CR_HSEON;
    cr &= ~RCC_CR_HSEBYP;
    RCC->CR = cr;
    while(!(RCC->CR & RCC_CR_HSERDY));

    /* Configure PLLs
        PLL1 = HSE16 / M8 * N384
            /P2 = 384 MHz -> SYSCLK 
            /Q8 = 96 MHz -> SDMMC1 and SPI1
            /R80 = 9.6 MHz -> unused (only able to use for TRACECLK)
            
        PLL2 = HSE16 / M8 * N172 . frac263
            /P7 = 49.152 MHz -> SAI1
            /Q2 = 172 MHz -> unused
            /R8 = 43 MHz -> unused
            
        PLL3 = HSE16 / M8 * N384
            /P7 = 109 MHz -> unused
            /Q16 = 48 MHz -> USB and SPI5
            /R64 = 12 MHz -> LTDC (30 Hz refresh), LPTIM1 and I2C4

        Peripheral clock PER_CLK is HSE16
    */
    
    // Ensure sysclk is HSI and PLLs disabled
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    while(RCC->CFGR & RCC_CFGR_SWS);
    RCC->CR &= ~(RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(RCC->CR & (RCC_CR_PLL1RDY | RCC_CR_PLL2RDY | RCC_CR_PLL3RDY));

    // Set voltage scaling 1
    PWR->D3CR = (3UL << PWR_D3CR_VOS_Pos);
    while(!(PWR->D3CR & PWR_D3CR_VOSRDY));

    // Set up PLLs as above
    RCC->PLLCKSELR = (8UL << RCC_PLLCKSELR_DIVM3_Pos) |
        (8UL << RCC_PLLCKSELR_DIVM2_Pos) |
        (8UL << RCC_PLLCKSELR_DIVM1_Pos) |
        (2UL << RCC_PLLCKSELR_PLLSRC);

    RCC->PLL2FRACR = 263UL;
    RCC->PLLCFGR = RCC_PLLCFGR_DIVR3EN |
        RCC_PLLCFGR_DIVQ3EN |
        RCC_PLLCFGR_DIVP2EN |
        RCC_PLLCFGR_DIVQ1EN |
        RCC_PLLCFGR_DIVP1EN |
        RCC_PLLCFGR_PLL2FRACEN;
    
    RCC->PLL1DIVR = (79UL << RCC_PLL1DIVR_R1_Pos) |
        (7UL << RCC_PLL1DIVR_Q1_Pos) |
        (1UL << RCC_PLL1DIVR_P1_Pos) |
        (383UL << RCC_PLL1DIVR_N1_Pos);
    RCC->PLL2DIVR = (7UL << RCC_PLL2DIVR_R2_Pos) |
        (1UL << RCC_PLL2DIVR_Q2_Pos) |
        (6UL << RCC_PLL2DIVR_P2_Pos) |
        (171UL << RCC_PLL2DIVR_N2_Pos);
    RCC->PLL3DIVR = (63UL << RCC_PLL3DIVR_R3_Pos) |
        (15UL << RCC_PLL3DIVR_Q3_Pos) |
        (6UL << RCC_PLL3DIVR_P3_Pos) |
        (383UL << RCC_PLL3DIVR_N3_Pos);
    
    RCC->CR |= (RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON);
    while(!(RCC->CR & RCC_CR_PLL1RDY));
    while(!(RCC->CR & RCC_CR_PLL2RDY));
    while(!(RCC->CR & RCC_CR_PLL3RDY));

    // Set up D2/D3 dividers to /2 (D1 divisors set up in clock_set_cpu())
    RCC->D2CFGR = (4UL << RCC_D2CFGR_D2PPRE2_Pos) |
        (4UL << RCC_D2CFGR_D2PPRE1_Pos);
    RCC->D3CFGR = (4UL << RCC_D3CFGR_D3PPRE_Pos);

    // Set cpus to run at slow speed once with plls as sysclk
    clock_set_cpu(clock_cpu_speed::cpu_48_48);

    // Set up peripherals to use the above mappings
    RCC->D1CCIPR = (2UL << RCC_D1CCIPR_CKPERSEL_Pos) |
        (0UL << RCC_D1CCIPR_SDMMCSEL_Pos) |
        (0UL << RCC_D1CCIPR_FMCSEL_Pos);
    RCC->D2CCIP1R = (2UL << RCC_D2CCIP1R_SPI45SEL_Pos) |
        (0UL << RCC_D2CCIP1R_SPI123SEL_Pos) |
        (1UL << RCC_D2CCIP1R_SAI1SEL_Pos);
    RCC->D2CCIP2R = (2UL << RCC_D2CCIP2R_LPTIM1SEL_Pos) |
        (2UL << RCC_D2CCIP2R_USBSEL_Pos) |
        (1UL << RCC_D2CCIP2R_RNGSEL_Pos);
    RCC->D3CCIPR = (1UL << RCC_D3CCIPR_I2C4SEL_Pos);
}

bool clock_set_cpu(clock_cpu_speed speed)
{
    // Set sysclk to use HSI16
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    while(RCC->CFGR & RCC_CFGR_SWS);

    // Set up flash latency (p.166) and new D1CPRE and HPRE
    RCC->AHB3ENR |= RCC_AHB3ENR_FLASHEN;
    (void)RCC->AHB3ENR;

    switch(speed)
    {
        case clock_cpu_speed::cpu_384_192:
            FLASH->ACR = (2UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (2UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (0UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (8UL << RCC_D1CFGR_HPRE_Pos);
            break;
            
        case clock_cpu_speed::cpu_192_192:
            FLASH->ACR = (2UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (2UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (8UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (0UL << RCC_D1CFGR_HPRE_Pos);
            break;

        case clock_cpu_speed::cpu_96_96:
            FLASH->ACR = (1UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (1UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (9UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (0UL << RCC_D1CFGR_HPRE_Pos);
            break;

        case clock_cpu_speed::cpu_48_48:
            FLASH->ACR = (0UL << FLASH_ACR_WRHIGHFREQ_Pos) |
                (0UL << FLASH_ACR_LATENCY_Pos);
            RCC->D1CFGR = (10UL << RCC_D1CFGR_D1CPRE_Pos) |
                (4UL << RCC_D1CFGR_D1PPRE_Pos) |
                (0UL << RCC_D1CFGR_HPRE_Pos);
            break;
    }

    // Set sysclk to use PLL
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) |
        (3UL << RCC_CFGR_SW_Pos);
    while((RCC->CFGR & RCC_CFGR_SW_Msk) != 3UL);

    return true;
}
