#include <cstdio>
#include <cstdint>
#include "logger.h"
#include "gic.h"
#include "vblock.h"
#include "vmem.h"
#include "thread.h"
#include "process.h"
#include "syscalls_int.h"

struct exception_regs
{
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, res0;
    uint64_t saved_spsr_el1, saved_elr_el1;
    uint64_t res1, res2;            // ensure q0 32 byte alignment ?needed
    uint64_t fpu_regs[16];
    uint64_t fp, lr;
};

static uint64_t TranslationFault_Handler(bool user, bool write, uint64_t address, uint64_t el);

extern "C" uint64_t Exception_Handler(uint64_t esr, uint64_t far,
    uint64_t etype, exception_regs *regs, uint64_t lr)
{
    if(etype == 0x401 && (esr == 0x46000000 || esr == 0x56000000))
    {
        SyscallHandler((syscall_no)regs->x0, (void *)regs->x1, (void *)regs->x2, (void *)regs->x3);
        return 0;
    }

    if(etype == 0x201 || etype == 0x401 || etype == 0x601)
    {
        // exception
        auto ec = (esr >> 26) & 0x3fULL;
        auto iss = esr & 0x1ffffffULL;
        if(ec == 0b100100 || ec == 0b100101)
        {
            // data abort

            auto dfsc = iss & 0x3fULL;

            if(dfsc >= 4 && dfsc <= 7)
            {
                // page fault

                bool user = (etype > 0x201) || (ec == 0b100100);
                bool write = (iss & (1ULL << 6)) != 0;

                klog("EXCEPTION: type: %llx, esr: %llx, far: %llx, lr: %llx, sp: %llx, nested elr: %llx\n",
                    etype, esr, far, lr, (uint64_t)regs, regs->saved_elr_el1);

                return TranslationFault_Handler(user, write, far, lr);
            }
        }
    }

    klog("EXCEPTION: type: %llx, esr: %llx, far: %llx, lr: %llx, sp: %llx, nested elr: %llx\n",
        etype, esr, far, lr, (uint64_t)regs, regs->saved_elr_el1);

    uint64_t ttbr0, ttbr1, tcr;
    __asm__ volatile("mrs %[ttbr0], ttbr0_el1\n"
        "mrs %[ttbr1], ttbr1_el1\n"
        "mrs %[tcr], tcr_el1\n"
    :
        [ttbr0] "=r" (ttbr0),
        [ttbr1] "=r" (ttbr1),
        [tcr] "=r" (tcr)
    : : "memory");
    klog("EXCEPTION: ttbr0: %llx, ttbr1: %llx, tcr: %llx\n", ttbr0, ttbr1, tcr);

    while(true);

    // we can change the address to return to by returning anything other than 0 here
    return 0;
}

static uint64_t UserThreadFault()
{
    klog("User thread fault\n");
    while(true);
}

static uint64_t SupervisorThreadFault()
{
    klog("Supervisor thread fault\n");
    while(true);
}

uint64_t TranslationFault_Handler(bool user, bool write, uint64_t far, uint64_t lr)
{
    klog("TranslationFault %s %s @ %llx from %llx\n", user ? "USER" : "SUPERVISOR",
        write ? "WRITE" : "READ", far, lr);

    if(far >= 0x8000000000000000ULL)
    {
        if(user)
        {
            // user access to upper half
            return UserThreadFault();
        }

        // Check vblock for access
        auto be = vblock_valid(far);
        if(!be.valid)
        {
            return SupervisorThreadFault();
        }
        if(far < be.data_start() || far >= be.data_end())
        {
            klog("pf: guard page hit\n");
            return SupervisorThreadFault();
        }

        klog("pf: lazy map %llx\n", far);

        if(vmem_map(far & ~(VBLOCK_64k), 0, be.user, be.write, be.exec))
        {
            return SupervisorThreadFault();
        }
        return 0;
    }
    else
    {
        // check vblock for access
        auto umem = GetCurrentThreadForCore()->p->user_mem.get();
        if(umem == nullptr)
        {
            klog("pf: lower half without a valid lower half vblock\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        VMemBlock be = InvalidVMemBlock();
        {
            CriticalGuard cg(umem->sl);
            be = vblock_valid(far, umem->blocks);
        }

        if(!be.valid)
        {
            klog("pf: user space vblock not found\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        if(far < be.data_start() || far >= be.data_end())
        {
            klog("pf: guard page hit\n");
            return user ? UserThreadFault() : SupervisorThreadFault();
        }

        klog("pf: lazy map %llx\n", far);

        {
            CriticalGuard cg(umem->sl);

            if(vmem_map(far & ~(VBLOCK_64k), 0, be.user, be.write, be.exec, umem->ttbr0))
            {
                return user ? UserThreadFault() : SupervisorThreadFault();
            }
        }

        klog("pf: done\n");
        return 0;
    }

    while(true);
}
