#include <stm32mp2xx.h>
#include "logger.h"

void init_clocks()
{
    // Enable HSE
    RCC->OCENSETR = RCC_OCENSETR_HSEON;
    (void)RCC->OCENSETR;
    while((RCC->OCRDYR & RCC_OCRDYR_HSERDY) == 0);

    // Enable LSE and LSI through backup supply - do not wait for LSE ready here as may break
    // RM p.972
    PWR->BDCR1 |= PWR_BDCR1_DBD3P;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    __asm__ volatile("dsb sy\n" ::: "memory");
    while(RCC->BDCR & RCC_BDCR_LSERDY);
    RCC->BDCR &= ~RCC_BDCR_LSEBYP;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RCC->BDCR |= RCC_BDCR_LSEGFON;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RCC->BDCR |= RCC_BDCR_LSEON;
    RCC->BDCR |= RCC_BDCR_LSION;
    __asm__ volatile("dsb sy\n" ::: "memory");
    while(!(RCC->BDCR & RCC_BDCR_LSIRDY));
    __asm__ volatile("dsb sy\n" ::: "memory");
    PWR->BDCR1 &= ~PWR_BDCR1_DBD3P;

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

    /* We cannot alter PLL4 because it is used to clock the RCC registers.
        Add a PLL5 which is clocked off HSE40 at the same output (1200 MHz) */
    RCC->MUXSELCFGR = (RCC->MUXSELCFGR & ~RCC_MUXSELCFGR_MUXSEL1_Msk) |
        (1U << RCC_MUXSELCFGR_MUXSEL1_Pos);
    __asm__ volatile("dmb sy\n" ::: "memory");
    RCC->PLL5CFGR1 = RCC->PLL4CFGR1 & ~RCC_PLL5CFGR1_PLLEN;
    RCC->PLL5CFGR2 = (2UL << RCC_PLL5CFGR2_FREFDIV_Pos) |
        (120UL << RCC_PLL5CFGR2_FBDIV_Pos);
    RCC->PLL5CFGR3 = RCC->PLL4CFGR3;
    RCC->PLL5CFGR4 = RCC->PLL4CFGR4;
    RCC->PLL5CFGR5 = RCC->PLL4CFGR5;
    RCC->PLL5CFGR6 = RCC->PLL4CFGR6;
    RCC->PLL5CFGR7 = RCC->PLL4CFGR7;
    RCC->PLL5CFGR1 |= RCC_PLL5CFGR1_PLLEN;

    // Run Core + MCU off PLL5
    klog("FSBL: setting core clocks to PLL5\n");
    RCC->XBARxCFGR[0] = 0x41;
    klog("FSBL: done\n");

    
    // This all seems reasonable.  Clock OSPI1 off PLL4 / 12 -> 100 MHz
    RCC->FINDIVxCFGR[48] = 0x40U | 11U;
    while(RCC->FINDIVSR2 & (1U << (48 - 32)));              // wait for finish changing
    RCC->XBARxCFGR[48] = 0x40U;
    while(RCC->XBARxCFGR[48] & RCC_XBARxCFGR_XBARxSTS);     // wait for finish changing  
}
