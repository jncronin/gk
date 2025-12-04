#include "gic.h"
#include <cstdint>
#include "logger.h"
#include "gk_conf.h"
#include "scheduler.h"

consteval uint32_t valid_cores()
{
    uint32_t v = 0;
    for(uint32_t i = 0U; i < GK_NUM_CORES; i++)
    {
        v |= 1UL << i;
    }
    return v;
}

void SDMMC1_IRQHandler();
void USB3DR_IRQHandler();

void gic_irq_handler(uint32_t iar)
{
    auto irq_no = iar & 0x3ff;
    bool do_switch = false;

    switch(irq_no)
    {
        case GIC_SGI_YIELD:
        case 30:
            klog("GIC: unfiltered task switch interrupt detected\n");
            break;

        case 155:
            SDMMC1_IRQHandler();
            break;

        case 259:
        case 260:
            USB3DR_IRQHandler();
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

int gic_send_sgi(unsigned int sgi_no, int core_id)
{
    if(sgi_no >= 8)
        return -1;
    
    unsigned int filter = 0;
    unsigned int core_list = 0;
    if(core_id == GIC_TARGET_SELF)
    {
        filter = 2U;
    }
    else if(core_id == GIC_TARGET_ALL_BUT_SELF)
    {
        filter = 1U;
    }
    else if(core_id == GIC_TARGET_ALL)
    {
        filter = 0U;
        core_list = valid_cores();
    }
    else if(core_id < GK_NUM_CORES)
    {
        filter = 0U;
        core_list = 1U << core_id;
    }
    else
    {
        // invalid core_id
        return -1;
    }

    *(volatile uint32_t *)(GIC_DISTRIBUTOR_BASE + 0xf00) = sgi_no |
        (1U << 15) |
        (core_list << 16) |
        (filter << 24);

    return 0;
}

static void gic_set_bit(unsigned int irq_n, uintptr_t base);
static void gic_set_8bit(unsigned int irq_n, unsigned int val, uintptr_t base);
static void gic_set_2bit(unsigned int irq_n, unsigned int val, uintptr_t base);
static void gic_set_priority(unsigned int irq_n, unsigned int priority);
static void gic_set_cfg(unsigned int irq_n, unsigned int cfg);

int gic_set_enable(unsigned int irq_n)
{
    gic_set_bit(irq_n, GIC_DISTRIBUTOR_BASE + 0x100);
    return 0;
}

int gic_clear_enable(unsigned int irq_n)
{
    gic_set_bit(irq_n, GIC_DISTRIBUTOR_BASE + 0x180);
    return 0;
}

int gic_set_target(unsigned int irq_n, int cpu_id)
{
    unsigned int ucpu_id = (cpu_id == GIC_TARGET_SELF) ? (1U << GetCoreID()) : (unsigned int)cpu_id;

    gic_set_8bit(irq_n, ucpu_id & 0xfU, GIC_DISTRIBUTOR_BASE + 0x800);
    return 0;
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

