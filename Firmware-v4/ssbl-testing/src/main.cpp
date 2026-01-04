#include <stm32mp2xx.h>
#include <cstring>

#include "pins.h"
#include "logger.h"
#include <cstdio>
#include "i2c_poll.h"
#include "pmic.h"
#include "gic.h"
#include "vmem.h"
#include "elf.h"
#include "gkos_boot_interface.h"
#include "elf_get_symbol.h"
#include "clocks.h"
#include "i2c.h"
#include "screen.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

gkos_boot_interface gbi{};

// a stack to catch very early el1 exceptions
uint64_t el1_stack[128];

// data for AP
uintptr_t ap_entry = 0;
extern uintptr_t AP_Target;

void init_clocks();
void ap_main();

int main(uint32_t bootrom_val)
{
    // Set up clocks for CPU1
    klog("SSBL: start\n");
    init_gic();
    init_clocks();
    init_vmem();

    // enable EL3 MMU
    __asm__ volatile (
        "msr daifset, #0x3\n"

        "mrs x0, S3_1_C15_C2_1\n"       // CPUECTRL_EL1
        "orr x0, x0, #(0x1 << 6)\n"     // SMPEN
        "msr S3_1_C15_C2_1, x0\n"

        "dsb sy\n"

        "mrs x0, sctlr_el3\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el3, x0\n"

        "msr daifclr, #0x3\n"
    : : : "memory", "x0");

    
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

    /* Set up RISAB6 to deny access to CM33 for the first 96 kiB of VDERAM,
        enable access to the last 32 kiB
        For CM33, the last page will be read-only (contains _cur_s updated by CA35)
    */
    for(auto i = 0U; i < 7; i++)
    {
        RISAB6->CID[i].PRIVCFGR = 0;

        if(i == 2)
        {
            RISAB6->CID[i].RDCFGR = 0xff000000U;
            RISAB6->CID[i].WRCFGR = 0x7f000000U;
        }
        else
        {
            RISAB6->CID[i].RDCFGR = 0xffffffffU;
            RISAB6->CID[i].WRCFGR = 0xffffffffU;
        }
    }
    for(auto i = 0U; i < 32; i++)
    {
        RISAB6->PGCIDCFGR[i] = 0x1; // enable filtering for all pages
    }
        

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

    /* Start CPU2
        - comment out for now as current cm33 firmware overwrites the _cur_s etc data
            at the end of VDERAM
    */
    //RCC->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;

    // get some details from STPMIC25
    klog("SSBL: PMIC PRODUCT_ID: %08x, VERSION_SR: %08x\n",
        pmic_read_register(0), pmic_read_register(1));

    // start buck 7 if not already on
    pmic_vreg buck7 { pmic_vreg::Buck, 7, true, 3300, pmic_vreg::HP };
    pmic_set(buck7);

    // start vddaudio for testing
    pmic_vreg ldo6 { pmic_vreg::LDO, 6, true, 3300, pmic_vreg::Normal };
    pmic_set(ldo6);
    
    pmic_dump_status();

    void init_lsm();
    init_lsm();

    init_i2c();

    while(true)
    {
        void lsm_poll();
        void pwr_poll();

        lsm_poll();
        pwr_poll();

        udelay(40000);
    }

    return 0;
}
