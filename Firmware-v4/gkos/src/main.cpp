#include "logger.h"
#include "clocks.h"
#include "pmem.h"
#include "gic.h"
#include "vblock.h"
#include "gkos_boot_interface.h"
#include "process.h"
#include "thread.h"
#include "kheap.h"
#include <memory>

// test threads
std::shared_ptr<Process> p_kernel;
std::shared_ptr<Thread> t_a, t_b;

static void *task_a(void *);
static void *task_b(void *);

extern "C" int mp_kpremain(const gkos_boot_interface *gbi, uint64_t magic)
{
    // These need to happen before __libc_init_array, which may call malloc

    init_clocks(gbi);
    
    klog("gkos: startup\n");

    uint64_t magic_str[2] = { magic, 0 };

    klog("gkos: magic: %s, ddr: %llx - %llx\n", (const char *)(&magic_str[0]), gbi->ddr_start, gbi->ddr_end);
    
    init_pmem(gbi->ddr_start, gbi->ddr_end);

    // Initialize a upper half block manager in the space between the mapped physical memory and the kernel
    init_vblock();
    init_kheap();
    return 0;
}

extern "C" int mp_kmain(const gkos_boot_interface *gbi, uint64_t magic)
{
    // allocate some space to test page faults
    auto pf_test = vblock_alloc(VBLOCK_64k, VBLOCK_TAG_WRITE);
    *(uint64_t *)pf_test.base = 0xdeadbeef;

    // GIC - route irq 30 to us
    const auto irq_n = 30U;
    const auto target_word = irq_n / 4U;
    const auto target_bit = (irq_n % 4U) * 8U;
    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x800 + 0x4 * target_word) =
        (*(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x800 + 0x4 * target_word) & ~(0xffUL << target_bit)) |
        (0x1UL << target_bit);
    const auto enable_word = irq_n / 32U;
    const auto enable_bit = irq_n % 32U;
    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x100 + 0x4 * enable_word) = 1UL << enable_bit;
    
     // enable irqs
    __asm__ volatile("msr daifclr, #0xf\n");

    // test systick timer - use generic timer el1 physical
    __asm__ volatile(
        "ldr x0, =0x303\n"          // prevent el0 from using timer
        "msr cntkctl_el1, x0\n"
        "mov x0, #0x1\n"            // enable, unmask interrupts
        "msr cntp_ctl_el0, x0\n"
        : : : "memory", "x0"
    );

    // time 1s using generic timer
    __asm__ volatile("msr cntp_tval_el0, %[delay]\n" : : [delay] "r" (64000000) : "memory");

    uint64_t last_ms = clock_cur_ms();


    // Create some threads
    p_kernel = std::make_shared<Process>();
    p_kernel->name = "kernel";

    t_a = Thread::Create("testa", task_a, nullptr, true, 1, p_kernel);
    t_b = Thread::Create("testb", task_b, nullptr, true, 1, p_kernel);

    while(true)
    {
        //udelay(50000);

        //__asm__ volatile("wfi\n" ::: "memory");

        //uint64_t ptimer;
        //__asm__ volatile("mrs %[ptimer], cntpct_el0\n" : [ptimer] "=r" (ptimer) : : "memory");
        //klog("gkos: tick %llu\n", ptimer);

        if(clock_cur_ms() > (last_ms + 1500))
        {
            klog("gkos: tick\n");
            last_ms = clock_cur_ms();
        }
    }

    return 0;
}

void *task_a(void *)
{
    while(true)
    {
        klog("A\n");
    }
}

void *task_b(void *)
{
    while(true)
    {
        klog("B\n");
    }
}
