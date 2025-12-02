#include <cstdio>
#include <cstdint>
#include "gic.h"
#include "logger.h"
#include "smc.h"

extern "C" uint64_t Exception_Handler(uint64_t esr, uint64_t far,
    uint64_t etype, exception_regs *regs, uint64_t lr)
{
    if(etype == 0x300 || etype == 0x500 || etype == 0x700)
    {
        // handle interrupt
        gic_irq_handler();
        return 0;
    }

    klog("SM EXCEPTION: type: %08lx, esr: %08lx, far: %llx, lr: %llx\n",
        etype, esr, far, lr);


    if(etype == 0x380)
    {
        // for some reason an SError is fired upon entry to EL1.  Ignore the first few.
        static int n_serror = 0;
        if(n_serror++ < 5)
            return 0;
    }

    if(etype == 0x400)
    {
        // exception from lower level
        auto ec = (esr >> 26) & 0x3fULL;
        klog("SM SYNC EXCEPTION from lower level: ec: %llx\n", ec);
        if(ec == 0x17)
        {
            // SMC call
            smc_handler((SMC_Call)(esr & 0xffffL), regs);
            return 0;
        }
    }

    while(true);

    return 0;
}
