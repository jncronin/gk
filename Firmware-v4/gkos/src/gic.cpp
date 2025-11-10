#include "gic.h"
#include <cstdint>
#include "logger.h"

void gic_irq_handler()
{
    auto iar = *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x20);
    __asm__ volatile("dmb ish\n" ::: "memory");

    auto irq_no = iar & 0x3ff;

    switch(irq_no)
    {
        case 30:
            // set another second in the future
            __asm__ volatile("msr cntp_tval_el0, %[delay]\n" : : [delay] "r" (64000000) : "memory");
            klog("gkos: systick\n");

            break;

        default:
            klog("GIC: spurious interrupt: %d\n", irq_no);
            break;
    }

    // EOI
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x24) = iar;
    __asm__ volatile("dmb st\n" ::: "memory");
}
