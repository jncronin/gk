#include "gic.h"
#include <cstdint>
#include "logger.h"

void gic_irq_handler()
{
    auto iar = *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x20);
    __asm__ volatile("dmb ish\n" ::: "memory");

    auto irq_no = iar & 0x3ff;
    bool do_switch = false;

    switch(irq_no)
    {
        case 30:
            // TODO: can this be filtered off similar to svc #1??
            do_switch = true;
            __asm__ volatile("msr cntp_ctl_el0, %[mask_timer]\n" : : [mask_timer] "r" (0x3));

            break;

        default:
            klog("GIC: spurious interrupt: %d\n", irq_no);
            break;
    }

    // EOI
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x24) = iar;
    __asm__ volatile("dmb st\n" ::: "memory");

    if(do_switch)
    {
        __asm__ volatile("svc #1\n" ::: "memory");
    }
}
