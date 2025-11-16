#include "gic.h"
#include "sm_clocks.h"
#include "logger.h"
#include <cstdint>

#define GIC_BASE             0x4AC00000UL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000UL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000UL)

void gic_irq_handler()
{
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

void gic_enable_ap()
{
    // GIC registers to do with PPIs and SGIs are banked, therefore ensure timers go to EL1
    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x080) = 0xffff00ffUL;

    /* Now set up the current core to handle interrupts, route group 0 to FIQ */
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0) = 0x3 | (1UL << 3);       // CTLR, FIQEN
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x4) = 0x80;    // priority mask - half-way value

    /* Within the core, enable interrupts.
        Route FIQ to EL3, keep IRQ at own level
        - Use SCR_EL3 for this
    */
    uint64_t scr_el3;
    __asm__ volatile("mrs %[scr_el3], scr_el3\n" : [scr_el3] "=r" (scr_el3));
    // Route FIQs to EL3, leave IRQs for EL0/1.  Route SErrors to EL3
    scr_el3 |= (0x1ULL << 2) | (0x1ULL << 3);
    scr_el3 &= ~(0x1ULL << 1);
    __asm__ volatile("msr scr_el3, %[scr_el3]\n" :: [scr_el3] "r" (scr_el3));   

    // Enable DAIF (debug, SError, IRQ, FIQ)
    __asm__ volatile("msr DAIFClr, #0xf\n" ::: "memory");
}

void gic_enable_sgi(unsigned int sgi_no)
{
    if(sgi_no >= 32)
        return;
    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x100) = 1UL << sgi_no;
}
