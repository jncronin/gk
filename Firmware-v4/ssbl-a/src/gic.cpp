/* Some GIC notes
    
    3 types of GIC input:
        Software generated interrupts (SGI) - think IPI here.  Target 1/many cores specified
            at firing time.  IRQ numbers 0-15.
        Private peripheral interrputs (PPI) - generated within a core and targeted at the 
            same core.  Think e.g. LAPIC interrupt.  IRQ numbers 16-31.
        Shared peripheral interrupts (SPI).  "standard" peripheral interrupts.  Target a
            specific core specified in GIC setup.  IRQ numbers 32-416 (max in STM32MP2).
        
    2 groups:
        Group 0 = FIQ/IRQ (determined by GICC_CTRL on an individual core)
             - need to read more but essentially they have (some) banked registers
            so can execute without register push/pop.  Could be useful for timer interrupt.
        Group 1 = IRQ - standard IRQs.

        Groups are determined by the GICD_IGROUPx registers.
    */

#include <cstdint>
#include <cstdio>
#include "clocks.h"
#include "logger.h"

/*
    There is also a MSI interface - GIC2VM, which on STM32MP2 allows an extra 32 software
        triggered interrputs (but they go through the SPI mechanism).
    GIC2VM @ 0x48090000
*/

#define GIC_BASE             0x4AC00000UL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000UL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000UL)

static void gic_set_bit(unsigned int irq_n, uintptr_t base);
static void gic_set_8bit(unsigned int irq_n, unsigned int val, uintptr_t base);
static void gic_set_2bit(unsigned int irq_n, unsigned int val, uintptr_t base);
static void gic_set_enable(unsigned int irq_n);
static void gic_clear_enable(unsigned int irq_n);
static void gic_set_target(unsigned int irq_n, unsigned int cpuid);
static void gic_set_priority(unsigned int irq_n, unsigned int priority);
static void gic_set_cfg(unsigned int irq_n, unsigned int cfg);

extern "C" uint64_t Read_SCR_EL3();
extern "C" void Write_SCR_EL3(uint64_t);

void init_gic()
{
    /* Make all interrupts group 1 (non-secure) with the exception of:
        138 (TIM3)
    */
    for(auto i = 0U; i < 13; i++)
    {
        *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x080 + 0x4 * i) = 0xffffffff;
    }
    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x080 + 0x4 * (138 / 32)) =
        *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0x080 + 0x4 * (138 / 32)) &
        ~(1UL << (138 % 32));
    
    /* Set up the distributor to pass IRQs to the cores */
    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0) = 0x3;     // CTLR

    /* Now set up the current core to handle interrupts, route group 0 to FIQ */
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0) = 0x3 | (1UL << 3);       // CTLR, FIQEN
    *(volatile uint32_t *)(GIC_INTERFACE_BASE + 0x4) = 0x80;    // priority mask - half-way value

    /* Within the core, enable interrupts.
        Route FIQ to EL3, keep IRQ at own level
        - Use SCR_EL3 for this
    */
    auto scr_el3 = Read_SCR_EL3();
    // Route FIQs to EL3, leave IRQs for EL0/1.  Route SErrors to EL3
    scr_el3 |= (0x1ULL << 2) | (0x1ULL << 3);
    scr_el3 &= ~(0x1ULL << 1);
    Write_SCR_EL3(scr_el3);
    

    // Enable DAIF (debug, SError, IRQ, FIQ)
    __asm__ volatile("msr DAIFClr, #0xf\n" ::: "memory");

    /* Enable TIM3 interrupt (138) - see RM 31.2 */
    gic_set_target(138, 0x1);
    gic_set_enable(138);
}

void gic_fiq_handler()
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

void gic_set_enable(unsigned int irq_n)
{
    gic_set_bit(irq_n, GIC_DISTRIBUTOR_BASE + 0x100);
}

[[maybe_unused]] void gic_clear_enable(unsigned int irq_n)
{
    gic_set_bit(irq_n, GIC_DISTRIBUTOR_BASE + 0x180);
}

[[maybe_unused]] void gic_set_target(unsigned int irq_n, unsigned int cpu_id)
{
    gic_set_8bit(irq_n, cpu_id & 0xf, GIC_DISTRIBUTOR_BASE + 0x800);
}

[[maybe_unused]] void gic_set_priority(unsigned int irq_n, unsigned int priority)
{
    gic_set_8bit(irq_n, priority << 3, GIC_DISTRIBUTOR_BASE + 0x400);
}

[[maybe_unused]] void gic_set_cfg(unsigned int irq_n, unsigned int cfg)
{
    gic_set_2bit(irq_n, cfg & 0x3U, GIC_DISTRIBUTOR_BASE + 0xc00);
}

[[maybe_unused]] void gic_set_bit(unsigned int irq_n, uintptr_t base)
{
    auto dword = irq_n / 32;
    auto bit = irq_n % 32;
    *(volatile uint32_t *)(base + dword * 4) = 1UL << bit;
}

void gic_set_8bit(unsigned int irq_n, unsigned int val, uintptr_t base)
{
    auto dword = irq_n / 4;
    auto bit = (irq_n % 4) * 8;
    auto mask = 0xffU << bit;
    val <<= bit;

    *(volatile uint32_t *)(base + dword * 4) =
        (*(volatile uint32_t *)(base + dword * 4) & ~mask) | val;
}

void gic_set_2bit(unsigned int irq_n, unsigned int val, uintptr_t base)
{
    auto dword = irq_n / 16;
    auto bit = (irq_n % 16) * 2;
    auto mask = 0x3U << bit;
    val <<= bit;

    *(volatile uint32_t *)(base + dword * 4) =
        (*(volatile uint32_t *)(base + dword * 4) & ~mask) | val;
}

