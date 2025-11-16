#include "smc.h"
#include "ap.h"
#include "logger.h"

#define GIC_BASE             0x4AC00000UL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000UL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000UL)

void smc_handler(SMC_Call smc_id, exception_regs *regs)
{
    switch(smc_id)
    {
        case SMC_Call::StartupAP:
            if(regs->x0 < ncores)
            {
                auto coreid = (unsigned int)regs->x0;
                aps[coreid].epoint = (void (*)(void *, void *))regs->x1;
                aps[coreid].p0 = (volatile void *)regs->x2;
                aps[coreid].p1 = (volatile void *)regs->x3;
                aps[coreid].el1_stack = (uintptr_t)regs->x4;
                aps[coreid].ttbr1 = (uintptr_t)regs->x5;
                aps[coreid].vbar = (uintptr_t)regs->x6;
                aps[coreid].ready = true;

                // ping core
                *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0xf00) = (0x8ULL) |
                    (1ULL << (16 + coreid));

                klog("SM: start ap %u\n", coreid);
            }
            break;
    }
}
