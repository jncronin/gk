#include <cstdint>
#include "gkos_boot_interface.h"
#include "logger.h"
#include "osspinlock.h"
#include "vmem.h"
#include "gkos_vmem.h"
#include "vblock.h"
#include "sm_clocks.h"
#include "clocks.h"
#include "elf.h"
#include "gic.h"
#include "ap.h"

#define STACK_SIZE  65536

uint64_t mp_stack_vaddr;
uint64_t ap_stack_vaddr;

uint64_t ddr_start;
uint64_t ddr_end;

uint64_t vaddr_ptr;

gkos_boot_interface gbi_for_el1;

AP_Data aps[ncores] = { 0 };

static inline uint64_t align(uint64_t v)
{
    return (v + 65535ULL) & ~65535ULL;
}

extern "C" uint64_t mp_preinit(const gkos_boot_interface *gbi)
{
    /* This is called prior to __libc_init_array so cannot rely on global objects being inited */

    // Get end of virtual space
    extern int _ebss;
    vaddr_ptr = (uint64_t)&_ebss;
    vaddr_ptr = align(vaddr_ptr);

    // Get stack space for mp and ap with guard pages before, between, after
    auto stack_alloc = vmem_alloc(3 * 65536 + STACK_SIZE * 2);
    mp_stack_vaddr = stack_alloc + 65536;
    ap_stack_vaddr = stack_alloc + 65536 * 2 + STACK_SIZE;

    // pointers for the pmem alloc
    ddr_start = gbi->ddr_start;
    ddr_end = gbi->ddr_end;

    pmem_map_region(mp_stack_vaddr, STACK_SIZE, true, true);
    pmem_map_region(ap_stack_vaddr, STACK_SIZE, true, true);

    return mp_stack_vaddr + STACK_SIZE;
}

extern "C" uint64_t ap_preinit()
{
    return ap_stack_vaddr + STACK_SIZE;
}

extern "C" void mp_kmain(const gkos_boot_interface *gbi, uint64_t magic)
{
    klog("SM: startup\n");

    clock_takeover();

    // init el1 vmem
    init_vmem(1);
    epoint ept_el1;
    if(elf_load((const void *)0x60060000, &ept_el1, 1) != 0)
    {
        klog("SM: elf_load failed\n");
        while(true);
    }

    // make a stack for el1 and map into our namespace
    auto el1_stack_vaddr = vmem_alloc(65536);
    auto el1_stack_paddr = pmem_vaddr_to_paddr(el1_stack_vaddr, true, true, 3);
    auto el1_stack_vaddr_el1 = el1_stack_paddr + UH_START;

    // populate data for el1 (keep some space for us)
    gbi_for_el1.ddr_start = ddr_start + 8 * STACK_SIZE;
    gbi_for_el1.ddr_end = ddr_end;
    ddr_end = gbi_for_el1.ddr_start;    // adjust our allocator
    extern uint64_t clock_block_paddr;
    gbi_for_el1.cur_s = (volatile uint64_t *)(clock_block_paddr);
    gbi_for_el1.tim_ns_precision = (volatile uint64_t *)(clock_block_paddr + 8ULL);

    auto gbi_vaddr_el1 = pmem_vaddr_to_paddr((uint64_t)&gbi_for_el1, false, false, 3) + UH_START;

    // put the above arguments on the el1 stack
    uint64_t el1_estack_ptr = ((uint64_t)el1_stack_vaddr + 65536);
    el1_estack_ptr -= 16;
    *reinterpret_cast<volatile uint64_t *>(el1_estack_ptr) = gbi_vaddr_el1;
    *reinterpret_cast<volatile uint64_t *>(el1_estack_ptr + 8) = gkos_sm_magic;

    // Move APs to waiting state
    extern uint64_t AP_Start;
    AP_Start = 1;

    // eret to el1
    __asm__ volatile (
        "mrs x0, S3_1_C15_C2_1\n"       // CPUECTRL_EL1
        "orr x0, x0, #(0x1 << 6)\n"     // SMPEN
        "msr S3_1_C15_C2_1, x0\n"

        "mov x0, xzr\n"
        "orr x0, x0, #(0x1 << 0)\n"     // M
        "orr x0, x0, #(0x1 << 2)\n"     // C
        "orr x0, x0, #(0x1 << 12)\n"    // I
        "msr sctlr_el1, x0\n"

        "mov x0, #(0x3 << 20)\n"
        "msr cpacr_el1, x0\n"   // disable trapping of neon/fpu instructions

        "msr sp_el1, %[el1_estack_vaddr_el1]\n"

        "mrs x0, scr_el3\n"
        "bfc x0, #62, #1\n"     // clear NSE (part of secure bits)
        "bfc x0, #18, #1\n"     // clear EEL2 (no EL2)
        "orr x0, x0, #(1 << 10)\n" // set RW (use A64 for EL1)
        //"orr x0, x0, #1\n"      // set NS (combined with NSE - EL0/1 is non-secure)
        "bfc x0, #0, #1\n"      // only seems to work for EL1 secure for some reason
        "msr scr_el3, x0\n"

        // disable interrupts for setting up spsr and elr, then re-enable them during the eret
        "msr daifset, #0xf\n"

        "mrs x0, spsr_el3\n"
        "bfc x0, #0, #4\n"      // clear M bits
        "orr x0, x0, #1\n"      // EL1 with sp_el1
        "orr x0, x0, #4\n"
        "bfc x0, #6, #3\n"      // clear AIF bits (re-enable interrupts on eret)
        "msr spsr_el3, x0\n"

        "msr elr_el3, %[el1_ept]\n"     // load return address

        "eret\n"
        : :
            [el1_estack_vaddr_el1] "r" (el1_stack_vaddr_el1 + 65536 - 16),
            [el1_ept] "r" (ept_el1)
        : "memory", "x0"
    );
}

extern "C" void ap_kmain(uint64_t magic)
{
    klog("SM: AP startup\n");

    uint64_t coreid;
    __asm__ volatile("mrs %[coreid], mpidr_el1\n" : [coreid] "=r" (coreid));
    coreid &= 0xff;

    // have GIC send us PPI interrupts in the secure range
    gic_enable_ap();
    gic_enable_sgi(8);

    __asm__ volatile("msr daifclr, #0xf\n" ::: "memory");

    while(true)
    {
        __asm__ volatile("wfi \n");
        if(aps[coreid].ready)
        {
            uint64_t tcr_el1 = 0;
                tcr_el1 |= (64ULL - 42ULL) << 16;       // 42 bit upper half paging
                tcr_el1 |= (0x1ULL << 24) | (0x1ULL << 26);      // cached accesses to page tables
                tcr_el1 |= (0x3ULL << 28);                      // shareable page tables
                tcr_el1 |= 3ULL << 30;                  // 64 kiB granule
                tcr_el1 |= 2ULL << 32;                  // intermediate physical address 40 bits

            __asm__ volatile(
                "mrs x0, S3_1_C15_C2_1\n"       // CPUECTRL_EL1
                "orr x0, x0, #(0x1 << 6)\n"     // SMPEN
                "msr S3_1_C15_C2_1, x0\n"

                "mov x0, xzr\n"
                "orr x0, x0, #(0x1 << 0)\n"     // M
                "orr x0, x0, #(0x1 << 2)\n"     // C
                "orr x0, x0, #(0x1 << 12)\n"    // I
                "msr sctlr_el1, x0\n"

                "msr mair_el1, %[mair]\n"
                "msr hcr_el2, xzr \n"
                "msr tcr_el1, %[tcr_el1]\n"
                "msr ttbr1_el1, %[ttbr1_el1]\n"

                "mov x0, #(0x3 << 20)\n"
                "msr cpacr_el1, x0\n"   // disable trapping of neon/fpu instructions

                "msr sp_el1, %[el1_estack_vaddr_el1]\n"

                "mrs x0, scr_el3\n"
                "bfc x0, #62, #1\n"     // clear NSE (part of secure bits)
                "bfc x0, #18, #1\n"     // clear EEL2 (no EL2)
                "orr x0, x0, #(1 << 10)\n" // set RW (use A64 for EL1)
                //"orr x0, x0, #1\n"      // set NS (combined with NSE - EL0/1 is non-secure)
                "bfc x0, #0, #1\n"      // only seems to work for EL1 secure for some reason
                "msr scr_el3, x0\n"

                // disable interrupts for setting up spsr and elr, then re-enable them during the eret
                "msr daifset, #0xf\n"

                "mrs x0, spsr_el3\n"
                "bfc x0, #0, #4\n"      // clear M bits
                "orr x0, x0, #1\n"      // EL1 with sp_el1
                "orr x0, x0, #4\n"
                "bfc x0, #6, #3\n"      // clear AIF bits (re-enable interrupts on eret)
                "msr spsr_el3, x0\n"

                "msr elr_el3, %[el1_ept]\n"     // load return address
                "mov x0, %[p0]\n"
                "mov x1, %[p1]\n"

                "msr vbar_el1, %[vbar_el1]\n"   // vtors

                "eret\n"
                : :
                    [el1_estack_vaddr_el1] "r" (aps[coreid].el1_stack),
                    [el1_ept] "r" (aps[coreid].epoint),
                    [p0] "r" (aps[coreid].p0),
                    [p1] "r" (aps[coreid].p1),
                    [ttbr1_el1] "r" (aps[coreid].ttbr1),
                    [mair] "r" (mair),
                    [tcr_el1] "r" (tcr_el1),
                    [vbar_el1] "r" (aps[coreid].vbar)
                : "memory", "x0"
            );
        }
    }
}
