#ifndef SMC_H
#define SMC_H

#include "smc_interface.h"
#include <cstdint>

struct exception_regs
{
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18;
    uint64_t fp, lr, res0, saved_spsr_el1, saved_elr_el1;
};

void smc_handler(SMC_Call smc_id, exception_regs *regs);

#endif
