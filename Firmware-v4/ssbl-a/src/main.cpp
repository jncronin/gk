#include <stm32mp2xx.h>
#include <cstring>

#include "pins.h"
#include "log.h"
#include <cstdio>
#include "i2c_poll.h"
#include "pmic.h"
#include "ddr.h"
#include "gic.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

void init_clocks();

int main(uint32_t bootrom_val)
{
    // Set up clocks for CPU1
    klog("SSBL: start\n");
    init_gic();
    init_clocks();
    
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
        
        We set up region 0 to be the last 16 kiB of flash, and send this to CID 2 (CPU2)

        The rest should stay under the default i.e. boot CPU only
    */
    RISAF2->REG[0].CFGR = 0;
    RISAF2->REG[0].STARTR = 0x3fc000;
    RISAF2->REG[0].ENDR = 0x3fffff;
    RISAF2->REG[0].CIDCFGR = 7U;    // TRACE/CPU0/CPU1
    RISAF2->REG[0].CFGR = 0xf0101;  // all privilege, secure, enable

    // Start up the CM33 code running from QSPI @ 0x603fc000
    // Boot in secure mode
    RCC->SYSCPU1CFGR |= RCC_SYSCPU1CFGR_SYSCPU1EN;
    (void)RCC->SYSCPU1CFGR;
    CA35SYSCFG->M33_TZEN_CR |= CA35SYSCFG_M33_TZEN_CR_CFG_SECEXT;
    CA35SYSCFG->M33_INITSVTOR_CR = 0x603fc000;
    RCC->CPUBOOTCR &= ~RCC_CPUBOOTCR_BOOT_CPU2;
    (void)RCC->CPUBOOTCR;
    RCC->C2RSTCSETR = RCC_C2RSTCSETR_C2RST;
    while(RCC->C2RSTCSETR & RCC_C2RSTCSETR_C2RST);

    /* Start CPU2 */
    RCC->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;

    klog("SSBL: CPU2 started\n");
    printf("SSBL: from printf: %d\n", 1234);

    // get some details from STPMIC25
    klog("SSBL: PMIC PRODUCT_ID: %08x, VERSION_SR: %08x\n",
        pmic_read_register(0), pmic_read_register(1));

    init_ddr();

    // try a switch to el1
    __asm__ volatile (
        "msr sctlr_el1, xzr\n"  // TODO set I, C, M bits to enable vmem directly

        "adr x0, _vtors\n"      // TODO set own vtors for EL1
        "msr vbar_el1, x0\n"

        "mov x0, #(0x3 << 20)\n"
        "msr cpacr_el1, x0\n"   // disable trapping of neon/fpu instructions

        "ldr x0, =el1_stack\n"
        "add x0, x0, #128*8\n"
        "bfc x0, #0, #4\n"      // align stack
        "msr sp_el1, x0\n"

        "mrs x0, scr_el3\n"
        "bfc x0, #62, #1\n"     // clear NSE (part of secure bits)
        "bfc x0, #18, #1\n"     // clear EEL2 (no EL2)
        "orr x0, x0, #(1 << 10)\n" // set RW (use A64 for EL1)
        //"orr x0, x0, #1\n"      // set NS (combined with NSE - EL0/1 is non-secure)
        "bfc x0, #0, #1\n"      // only seems to work for EL1 secure for some reason
        "msr scr_el3, x0\n"

        "mrs x0, spsr_el3\n"
        "bfc x0, #0, #4\n"      // clear M bits
        "orr x0, x0, #1\n" // EL1 with sp_el1
        "orr x0, x0, #4\n"
        "msr spsr_el3, x0\n"

        "adr x0, el1_start\n"
        "msr elr_el3, x0\n"     // load return address

        "eret\n"
        ::: "memory"
    );

    while(true);

    return 0;
}

uint64_t el1_stack[128];

extern "C" void el1_start()
{
    klog("EL1 success\n");
    while(true);
}


