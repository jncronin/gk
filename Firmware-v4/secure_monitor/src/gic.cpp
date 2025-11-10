#include "gic.h"
#include "sm_clocks.h"
#include "logger.h"
#include <cstdint>

#define GIC_BASE             0x4AC00000UL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000UL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000UL)

void gic_irq_handler()
{
    // TODO: switch to using group1 at some point
    auto iar = *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0xc);
    __asm__ volatile("dmb ish\n" ::: "memory");

    auto irq_no = iar & 0x3ff;

    switch(irq_no)
    {
        case 138:
            clock_irq_handler();
            break;

        default:
            klog("GIC: spurious interrupt: %d\n", irq_no);
            break;
    }

    // EOI
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x10) = iar;
    __asm__ volatile("dmb st\n" ::: "memory");
}
