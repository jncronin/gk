#include <stm32mp2xx.h>

#include "pins.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

void init_clocks();

int main(uint32_t bootrom_val)
{
    // Set up clocks so that we can get a nice fast clock for QSPI
    //init_clocks();
    
    EV_BLUE.set_as_output();

    // say hi
    for(int n = 0; n < 10; n++)
    {
        EV_BLUE.set();
        for(int i = 0; i < 2500000; i++);
        EV_BLUE.clear();
        for(int i = 0; i < 2500000; i++);
    }

    /* Give CM33 access to RISAF2 (OSPI) (already has secure access to the other RAMs except BKPSRAM)
        RISAF2->HWCFGR = 0x100c0505
        
        We set up region 0 to be the last 2 MiB of flash, and send this to CID 2 (CPU2)

        The rest should stay under the default i.e. boot CPU only
    */
    RISAF2->REG[0].CFGR = 0;
    RISAF2->REG[0].STARTR = 0x200000;
    RISAF2->REG[0].ENDR = 0x3fffff;
    RISAF2->REG[0].CIDCFGR = 7U;    // TRACE/CPU0/CPU1
    RISAF2->REG[0].CFGR = 0xf0101;  // all privilege, secure, enable

    // Start up the CM33 code running from QSPI @ 0x60200000
    // Boot in secure mode
    RCC->SYSCPU1CFGR |= RCC_SYSCPU1CFGR_SYSCPU1EN;
    (void)RCC->SYSCPU1CFGR;
    CA35SYSCFG->M33_TZEN_CR |= CA35SYSCFG_M33_TZEN_CR_CFG_SECEXT;
    CA35SYSCFG->M33_INITSVTOR_CR = 0x60200000;
    RCC->CPUBOOTCR &= ~RCC_CPUBOOTCR_BOOT_CPU2;
    (void)RCC->CPUBOOTCR;
    RCC->C2RSTCSETR = RCC_C2RSTCSETR_C2RST;
    while(RCC->C2RSTCSETR & RCC_C2RSTCSETR_C2RST);
    RCC->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;

    while(true);

    return 0;
}
