#include "cpu.h"
#include "gkos_vmem.h"
#include "gk_conf.h"

void cpu_start_local_timer()
{
    __asm__ volatile(
        "ldr x0, =0x303\n"          // prevent el0 from using timer
        "msr cntkctl_el1, x0\n"
        "mov x0, #0x3\n"            // enable, mask interrupts (unmasked at first task switch)
        "msr cntp_ctl_el0, x0\n"
        : : : "memory", "x0"
    );
}

void cpu_setup_vmem()
{
    uint64_t tcr_el1 = 0;
    tcr_el1 |= (64ULL - 42ULL) << 16;       // 42 bit upper half paging
    tcr_el1 |= (0x1ULL << 24) | (0x1ULL << 26);      // cached accesses to page tables
    tcr_el1 |= (0x3ULL << 28);                      // shareable page tables
    tcr_el1 |= 3ULL << 30;                  // 64 kiB granule
    tcr_el1 |= 2ULL << 32;                  // intermediate physical address 40 bits

    tcr_el1 |= (64ULL - 42ULL) << 0;        // 42 bit lower half paging
    tcr_el1 |= (1ULL << 7);                 // disable lower half paging for now (re-enabled in switcher)
    tcr_el1 |= (0x1ULL << 8) | (0x1ULL << 10);      // cached access to page tables
    tcr_el1 |= (0x3ULL << 12);              // shareable page tables
    tcr_el1 |= (0x1ULL << 14);              // 64 kiB granule

    tcr_el1 |= (1ULL << 36);                // ASID is 16 bytes

    __asm__ volatile(
        "msr mair_el1, %[mair]\n"
        "msr tcr_el1, %[tcr_el1]\n"
    : :
        [mair] "r" (mair),
        [tcr_el1] "r" (tcr_el1)
    : "memory");
}

void cpu_setup_userspace_permissions()
{
    uint64_t sctlr_el1;
    __asm__ volatile("mrs %[sctlr_el1], sctlr_el1\n" :
        [sctlr_el1] "=r" (sctlr_el1) :: "memory");

    uint64_t userspace_cache_bits = (1UL << 14) | (1UL << 15) | (1UL << 26); // allow access to ctr_el0 and dc __x instructions

#if GK_ALLOW_USERSPACE_CACHE_MAINTENANCE
    sctlr_el1 |= userspace_cache_bits;
#else
    sctlr_el1 &= ~userspace_cache_bits;
#endif

    __asm__ volatile("msr sctlr_el1, %[sctlr_el1]\n" ::
        [sctlr_el1] "r" (sctlr_el1) : "memory");
}
