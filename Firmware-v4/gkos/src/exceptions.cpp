#include <cstdio>
#include <cstdint>
#include "logger.h"
#include "gic.h"
#include "vblock.h"
#include "vmem.h"

struct exception_regs
{
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18;
    uint64_t fp, lr, res0, saved_spsr_el1, saved_elr_el1;
    uint64_t fpu_regs[16];
};

static uint64_t TranslationFault_Handler(bool user, bool write, uint64_t address, uint64_t el);

extern "C" uint64_t Exception_Handler(uint64_t esr, uint64_t far,
    uint64_t etype, exception_regs *regs, uint64_t lr)
{
    if(etype == 0x281)
    {
        gic_irq_handler();
        return 0;
    }
    else if(etype == 0x201 || etype == 0x401 || etype == 0x601)
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

    while(true);
}
