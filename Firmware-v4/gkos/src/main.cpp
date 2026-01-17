#include "logger.h"
#include "clocks.h"
#include "pmem.h"
#include "gic.h"
#include "vblock.h"
#include "gkos_boot_interface.h"
#include "process.h"
#include "thread.h"
#include "kheap.h"
#include "scheduler.h"
#include "smc_interface.h"
#include "vmem.h"
#include "gk_conf.h"
#include "sd.h"
#include "ext4_thread.h"
#include "threadproclist.h"
#include "screen.h"
#include "process_interface.h"
#include "sound.h"
#include "bootinfo.h"
#include "i2c.h"
#include "pwr.h"
#include "ctp.h"
#include "memchk.h"
#include <memory>

// test threads
std::shared_ptr<Process> p_kernel;
std::shared_ptr<Thread> t_a, t_b;

[[maybe_unused]] static void *task_a(void *);
[[maybe_unused]] static void *task_b(void *);

static void start_ap(unsigned int ap_no);

unsigned int reboot_flags = 0;

void *init_thread(void *);

// saved copy of the boot interface
constinit gkos_boot_interface gbi;

extern "C" int mp_kpremain(const gkos_boot_interface *_gbi, uint64_t magic)
{
    // These need to happen before __libc_init_array, which may call malloc
    gbi = *_gbi;

    init_clocks(_gbi);
    
    klog("gkos: startup\n");

    uint64_t magic_str[2] = { magic, 0 };

    klog("gkos: magic: %s, ddr: %llx - %llx\n", (const char *)(&magic_str[0]), _gbi->ddr_start, _gbi->ddr_end);
    
    init_pmem(_gbi->ddr_start, _gbi->ddr_end);

    // Initialize a upper half block manager in the space between the mapped physical memory and the kernel
    init_vblock();
    init_kheap();
    return 0;
}

extern "C" int mp_kmain(const gkos_boot_interface *_gbi, uint64_t magic)
{
    switch(gbi.btype)
    {
        case gkos_boot_interface::board_type::GKV4:
            klog("gkos: booting on GKV4 board\n");
            break;
        case gkos_boot_interface::board_type::EV1:
            klog("gkos: booting on EV1 board\n");
            break;
    }

    init_memchk();

    // allocate some space to test page faults
    auto pf_test = vblock_alloc(VBLOCK_64k, false, true, false);
    *(uint64_t *)pf_test.base = 0xdeadbeef;

    // GIC - enable IRQ 30 (physical timer) + IPIs
    gic_set_enable(30);
    gic_set_enable(GIC_SGI_YIELD);
    gic_set_enable(GIC_SGI_IPI);
    
    // time 1s using generic timer
    //__asm__ volatile("msr cntp_tval_el0, %[delay]\n" : : [delay] "r" (64000000) : "memory");

    uint64_t last_ms = clock_cur_ms();

    // Set up the kernel process
    p_kernel = Process::Create("kernel", true);
    {
        CriticalGuard cg(p_kernel->open_files.sl);
        auto fd_stdin = p_kernel->open_files.get_fixed_fildes(STDIN_FILENO);
        p_kernel->open_files.f[fd_stdin] = std::make_shared<UARTFile>(true, false);

        auto fd_stdout = p_kernel->open_files.get_fixed_fildes(STDOUT_FILENO);
        p_kernel->open_files.f[fd_stdout] = std::make_shared<UARTFile>(false, true);

        auto fd_stderr = p_kernel->open_files.get_fixed_fildes(STDERR_FILENO);
        p_kernel->open_files.f[fd_stderr] = std::make_shared<UARTFile>(false, true);
    }
    {
        CriticalGuard cg(p_kernel->env.sl);
        p_kernel->env.envs.push_back("NAME=gk");
        p_kernel->env.envs.push_back("HOME=/home/user");
        p_kernel->env.envs.push_back("USER=user");
        p_kernel->env.envs.push_back("NUMBER_OF_PROCESSORS=" + std::to_string(sched.ncores));
    }

    // Create some threads

    //Schedule(Thread::Create("testa", task_a, nullptr, true, 1, p_kernel));
    //Schedule(Thread::Create("testb", task_b, nullptr, true, 1, p_kernel));

    init_i2c();
    init_sd();
    init_ext4();
    init_screen();
    init_sound();
    init_pwr();
    init_ctp();

    init_process_interface();

    Schedule(Thread::Create("init", init_thread, nullptr, true, GK_PRIORITY_NORMAL, p_kernel));

    start_ap(1);

    sched.StartForCurrentCore();

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
        uint64_t cntp_ctl_el0, cntpct_el0, cntp_cval_el0;
        __asm__ volatile(
            "mrs %[cntp_ctl_el0], cntp_ctl_el0\n"
            "mrs %[cntpct_el0], cntpct_el0\n"
            "mrs %[cntp_cval_el0], cntp_cval_el0\n" :
            [cntp_ctl_el0] "=r" (cntp_ctl_el0),
            [cntpct_el0] "=r" (cntpct_el0),
            [cntp_cval_el0] "=r" (cntp_cval_el0));
        klog("A (%u), ctl: %llx, pct: %llu, cval: %llu\n",
            GetCoreID(),
            cntp_ctl_el0, cntpct_el0, cntp_cval_el0);
    }
}

void *task_b(void *)
{
    while(true)
    {
        uint64_t cntp_ctl_el0, cntpct_el0, cntp_cval_el0;
        __asm__ volatile(
            "mrs %[cntp_ctl_el0], cntp_ctl_el0\n"
            "mrs %[cntpct_el0], cntpct_el0\n"
            "mrs %[cntp_cval_el0], cntp_cval_el0\n" :
            [cntp_ctl_el0] "=r" (cntp_ctl_el0),
            [cntpct_el0] "=r" (cntpct_el0),
            [cntp_cval_el0] "=r" (cntp_cval_el0));
        klog("B (%u), ctl: %llx, pct: %llu, cval: %llu\n",
            GetCoreID(),
            cntp_ctl_el0, cntpct_el0, cntp_cval_el0);
    }
}

static void ap_epoint()
{
    
    // GIC - enable IRQ 30 (physical timer) + IPIs
    gic_set_enable(30);
    gic_set_enable(GIC_SGI_YIELD);
    gic_set_enable(GIC_SGI_IPI);

    sched.StartForCurrentCore();
    while(true);
}

void start_ap(unsigned int ap_no)
{
    if(ap_no >= GK_NUM_CORES)
    {
        klog("kernel: invalid ap core id %u\n", ap_no);
        return;
    }

    // create a stack
    auto vm_ap_stack = vblock_alloc(VBLOCK_64k, false, true, false);
    if(!vm_ap_stack.valid)
    {
        klog("kernel: failed to allocate ap stack\n");
        return;
    }
    if(vmem_map(vm_ap_stack, InvalidPMemBlock()) != 0)
    {
        klog("kernel: failed to map ap stack\n");
        return;
    }

    __asm__ volatile(
        "mov x0, %[ap_no]\n"
        "mov x1, %[epoint]\n"
        "mov x2, xzr\n"
        "mov x3, xzr\n"
        "mov x4, %[ap_stack]\n"
        "mrs x5, ttbr1_el1\n"
        "mrs x6, vbar_el1\n"
        "smc %[svc_no]\n" ::
        [svc_no] "i" (SMC_Call::StartupAP),
        [ap_no] "r" (ap_no),
        [epoint] "r" (ap_epoint),
        [ap_stack] "r" (vm_ap_stack.data_end())
        :
        "x0", "x1", "x2", "x3", "x4", "x5"
    );
}
