#include <stm32mp2xx.h>
#include <cstring>

#include "pins.h"
#include "logger.h"
#include <cstdio>
#include "i2c_poll.h"
#include "pmic.h"
#include "ddr.h"
#include "gic.h"
#include "vmem.h"
#include "elf.h"
#include "gkos_boot_interface.h"
#include "elf_get_symbol.h"

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

    /* Start CPU2 */
    RCC->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;

    klog("SSBL: CPU2 started\n");

    // get some details from STPMIC25
    klog("SSBL: PMIC PRODUCT_ID: %08x, VERSION_SR: %08x\n",
        pmic_read_register(0), pmic_read_register(1));

    init_ddr();
    init_vmem();

    // load secure monitor into a paged EL3
    epoint el3_ept = nullptr;
    if(elf_load((const void *)0x60300000, &el3_ept, 1) != 0)
    {
        klog("elf: secure monitor failed to load\n");
        while(true);
    }

    // do we have an AP entry point?
    ap_entry = elf_get_symbol((const void *)0x60300000, "_ap_kstart");
    if(ap_entry)
    {
        klog("elf: AP entry point found at %llx\n", ap_entry);
    }

    // bring AP online
    AP_Target = (uintptr_t)ap_main;

    gbi.ddr_start = pmem_get_cur_brk();
    gbi.ddr_end = 0x80000000ULL + ddr_get_size();

    void (*sm_ep)(const gkos_boot_interface *, uint64_t) =
        (void (*)(const gkos_boot_interface *, uint64_t))el3_ept;
    
    // disable irqs/fiqs, enable paging
    __asm__ volatile (
        "msr daifset, #0x3\n"

        "mrs x0, S3_1_C15_C2_1\n"       // CPUECTRL_EL1
        "orr x0, x0, #(0x1 << 6)\n"     // SMPEN
        "msr S3_1_C15_C2_1, x0\n"
        
        "mrs x0, sctlr_el3\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el3, x0\n"
    : : : "memory", "x0");

    sm_ep(&gbi, gkos_ssbl_magic);
    while(true);

    return 0;
}

void ap_main()
{
    if(!ap_entry)
    {
        klog("AP: no sm entry point found, halting AP\n");
        while(true)
        {
            __asm__ volatile("wfi \n" ::: "memory");
        }
    }

    void init_vmem_ap();
    init_vmem_ap();

    // disable irqs/fiqs, enable paging
    __asm__ volatile (
        "msr daifset, #0x3\n"

        "mrs x0, S3_1_C15_C2_1\n"       // CPUECTRL_EL1
        "orr x0, x0, #(0x1 << 6)\n"     // SMPEN
        "msr S3_1_C15_C2_1, x0\n"
        
        "mrs x0, sctlr_el3\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el3, x0\n"
    : : : "memory", "x0");

    klog("AP: running sm entry point\n");
    auto sm_ap_ep = (void (*)(uint64_t))ap_entry;
    sm_ap_ep(gkos_ssbl_magic);
    while(true);
}
