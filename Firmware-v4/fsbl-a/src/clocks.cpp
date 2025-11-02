#include <stm32mp2xx.h>

void init_clocks()
{
    // Enable HSE
    RCC->OCENSETR = RCC_OCENSETR_HSEON;
    (void)RCC->OCENSETR;
    while((RCC->OCRDYR & RCC_OCRDYR_HSERDY) == 0);

    // OSPI on XBAR 48 (see RM 21.4.9)
    // 0x44200000 (RCC) + 0x1018 + 4 * x

    // Initial XBAR setting is 0x40 (PLL4) for 0 (MCU) and 1 (SDMMC1) and 0x45 (HSI) for the rest
    // Predividers are all 0 (div1)
    // Final dividers are 0x43 (enabled, div4) for 0 (MCU) and 1 (SDMMC1) and 0x40 (enabled, div1) for the rest

    // PLL muxes (MUXSEL @ 0x44201000) = 0 => all 8 PLLs from HSI64
    /* PLL4 is set up by BOOTROM as follows:
        CFGR1 = 0x11000101      // enabled, running, SSC reset
        CFGR2 = 0x004b0002      // input divide 2, VCO mult 75
        CFRG3 = 0x04000000      // SSC disabled, no fractional bits
        CFGR4 = 0x00000200      // postdiv1, postdiv2 active
        CFGR5 = 0               // SSC config
        CFGR6 = 2               // postdiv1 = /2
        CFGR7 = 1               // postdiv2 = /1

        to give an output of 1200 MHz
    */
    
    // This all seems reasonable.  Clock OSPI1 off PLL4 / 12 -> 100 MHz
    RCC->FINDIVxCFGR[48] = 0x40U | 12U;
    while(RCC->FINDIVSR2 & (1U << (48 - 32)));              // wait for finish changing
    RCC->XBARxCFGR[48] = 0x40U;
    while(RCC->XBARxCFGR[48] & RCC_XBARxCFGR_XBARxSTS);     // wait for finish changing  
}
