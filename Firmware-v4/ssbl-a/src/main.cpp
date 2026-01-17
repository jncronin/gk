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

    /* Only allow access to SRAMs for CA35.  We later (in gkos) give CM33 access to some */
    // Cannot access RISAB3/4 without the relevant SRAM being clocked
    RCC->SRAM1CFGR |= RCC_SRAM1CFGR_SRAM1EN;
    RCC->SRAM2CFGR |= RCC_SRAM2CFGR_SRAM2EN;
    __asm__ volatile("dsb sy\n" ::: "memory");
    
    for(auto risab : (RISAB_TypeDef *[]){ RISAB1, RISAB2, RISAB3, RISAB4, RISAB5, RISAB6 })
    {
        // Give privileged read/write access to CIDs 0,1
        for(unsigned int cid = 0; cid <= 6; cid++)
        {
            klog("risab @ %p, cid %u\n", risab, cid);
            auto enable_val = (cid <= 1) ? 0xffffffffU : 0U;
            risab->CID[cid].PRIVCFGR = enable_val;
            risab->CID[cid].RDCFGR = enable_val;
            risab->CID[cid].WRCFGR = enable_val;
        }

        for(unsigned int page = 0; page < 32; page++)
        {
            // Secure accesses only
            risab->PGSECCFGR[page] = 0xffU;
            // Privileged accesses only
            risab->PGPRIVCFGR[page] = 0xffU;
            // CID filtering
            risab->PGCIDCFGR[page] = RISAB_PGCIDCFGR_CFEN;
        }
    }

    // get some details from STPMIC25
    klog("SSBL: PMIC PRODUCT_ID: %08x, VERSION_SR: %08x\n",
        pmic_read_register(0), pmic_read_register(1));

    // start buck 7 if not already on
    pmic_vreg buck7 { pmic_vreg::Buck, 7, true, 3300, pmic_vreg::HP };
    pmic_set(buck7);
    
    pmic_dump_status();

    init_ddr();

    /* Give everything access to all of DDR, in privileged and unprivileged modes (still secure though) */
    RISAF4->REG[0].CFGR = 0;
    RISAF4->REG[0].STARTR = 0;
    RISAF4->REG[0].ENDR = (uint32_t)(ddr_get_size() - 1);
    RISAF4->REG[0].CIDCFGR = 0x00ff00ffU;
    RISAF4->REG[0].CFGR = 0x101;

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

    gbi.ddr_start = pmem_get_cur_brk();
    gbi.ddr_end = 0x80000000ULL + ddr_get_size();

    void (*sm_ep)(const gkos_boot_interface *, uint64_t) =
        (void (*)(const gkos_boot_interface *, uint64_t))el3_ept;

    // bring AP online
    AP_Target = (uintptr_t)ap_main;
    
    // disable irqs/fiqs, enable paging
    __asm__ volatile (
        "msr daifset, #0x3\n"

        "mrs x0, S3_1_C15_C2_1\n"       // CPUECTRL_EL1
        "orr x0, x0, #(0x1 << 6)\n"     // SMPEN
        "msr S3_1_C15_C2_1, x0\n"

        "bl invcache\n"

        "isb\n"
        "tlbi alle3\n"
        "dsb ish\n"
        "isb\n"
        
        "mrs x0, sctlr_el3\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el3, x0\n"
    : : : "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6");

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

        "bl invcache\n"

        "isb\n"
        "tlbi alle3\n"
        "dsb ish\n"
        "isb\n"
        
        "mrs x0, sctlr_el3\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el3, x0\n"
    : : : "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6");

    klog("AP: running sm entry point\n");
    auto sm_ap_ep = (void (*)(uint64_t))ap_entry;
    sm_ap_ep(gkos_ssbl_magic);
    while(true);
}
