#include <stm32mp2xx.h>
#include <cstring>

#include "pins.h"
#include "log.h"
#include <cstdio>
#include "i2c_poll.h"
#include "pmic.h"
#include "ddr.h"
#include "gic.h"
#include "vmem.h"
#include "elf.h"
#include "gkos_boot_interface.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

gkos_boot_interface gbi{};

// a stack to catch very early el1 exceptions
uint64_t el1_stack[128];

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
    printf("SSBL: from printf: %d\n", 1234);

    // get some details from STPMIC25
    klog("SSBL: PMIC PRODUCT_ID: %08x, VERSION_SR: %08x\n",
        pmic_read_register(0), pmic_read_register(1));

    init_ddr();
    init_vmem();
    epoint el1_ept = nullptr;
    if(elf_load((const void *)0x60060000, &el1_ept, 1) != 0)
    {
        klog("elf: el1 failed to load\n");
        while(true);
    }

    // TODO: load secure monitor into a paged EL3


    gbi.ddr_start = pmem_get_cur_brk();
    gbi.ddr_end = 0x80000000ULL + ddr_get_size();
    uint64_t gbi_vaddr = pmem_paddr_to_vaddr((uint64_t)&gbi, 1);
    
    uint64_t el1_estack_paddr = ((uint64_t)&el1_stack + sizeof(el1_stack)) & ~15ULL;
    el1_estack_paddr -= 16;
    *reinterpret_cast<uint64_t *>(el1_estack_paddr) = gbi_vaddr;
    *reinterpret_cast<uint64_t *>(el1_estack_paddr + 8) = gkos_magic;

    auto el1_estack_vaddr = pmem_paddr_to_vaddr(el1_estack_paddr, 1);


    // set-up for EL1 switch
    __asm__ volatile (
        "mov x0, xzr\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el1, x0\n"

        "adr x0, _vtors\n"      // TODO set own vtors for EL1
        "msr vbar_el1, x0\n"

        "mov x0, #(0x3 << 20)\n"
        "msr cpacr_el1, x0\n"   // disable trapping of neon/fpu instructions

        "msr sp_el1, %[el1_estack_vaddr]\n"

        "mrs x0, scr_el3\n"
        "bfc x0, #62, #1\n"     // clear NSE (part of secure bits)
        "bfc x0, #18, #1\n"     // clear EEL2 (no EL2)
        "orr x0, x0, #(1 << 10)\n" // set RW (use A64 for EL1)
        //"orr x0, x0, #1\n"      // set NS (combined with NSE - EL0/1 is non-secure)
        "bfc x0, #0, #1\n"      // only seems to work for EL1 secure for some reason
        "msr scr_el3, x0\n"

        // do we need to disable IRQs here to prevent spsr being overwritten?

        "mrs x0, spsr_el3\n"
        "bfc x0, #0, #4\n"      // clear M bits
        "orr x0, x0, #1\n"      // EL1 with sp_el1
        "orr x0, x0, #4\n"
        "msr spsr_el3, x0\n"

        "msr elr_el3, %[el1_ept]\n"     // load return address

        //"eret\n"
        : :
            [el1_estack_vaddr] "r" (el1_estack_vaddr),
            [el1_ept] "r" (el1_ept)
        : "memory", "x0"
    );

    // TODO: pass control to secure monitor (perform a jump within identity-mapped page)
    //  which will then eret to EL1

    __asm__ volatile("eret\n" ::: "memory");
    while(true);

    return 0;
}

/*
extern "C" void el1_start()
{
    klog("EL1 success\n");
    while(true);
}
*/

