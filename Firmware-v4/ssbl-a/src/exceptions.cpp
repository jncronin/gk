#include <cstdio>
#include <cstdint>
#include "gic.h"

struct exception_regs
{
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18;
    uint64_t fp, lr;
};

extern "C" uint64_t Exception_Handler(uint64_t esr, uint64_t far,
    uint64_t etype, exception_regs *regs, uint64_t lr)
{
    printf("EXCEPTION: type: %08lx, esr: %08lx, far: %08lx, lr: %08lx\n",
        etype, esr, far, lr);

    if(etype == 0x280)
    {
        // handle interrupt
        gic_irq_handler();
        return 0;
    }

    while(true);

    // we can change the address to return to by returning anything other than 0 here
    return 0;
}
