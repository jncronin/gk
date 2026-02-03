#ifndef GIC_H
#define GIC_H

#include <cstdint>
#include "gic_irq_nos.h"
#include "gk_conf.h"

struct exception_regs
{
    uint64_t fp, lr;
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, res0;
    uint64_t saved_spsr_el1, saved_elr_el1;
    uint64_t fpu_regs[64];
};

extern "C" void gic_irq_handler(uint32_t aiar, exception_regs *regs, uint64_t elr_el1);

typedef void (*gic_irqhandler_t)(exception_regs *regs, uint64_t elr_el1);

int gic_set_target(unsigned int irq_no, int core_id = GIC_TARGET_SELF);
int gic_set_enable(unsigned int irq_no);
int gic_set_handler(unsigned int irq_no, gic_irqhandler_t handler);
int gic_clear_enable(unsigned int irq_no);
int gic_send_sgi(unsigned int sgi_no, int core_id = GIC_TARGET_SELF);

static constexpr unsigned int gic_enabled_cores()
{
    unsigned int coreids = 0;
    unsigned int n_cores = GK_NUM_CORES;
    for(auto i = 0U; i < n_cores; i++)
        coreids |= 1U << i;
    return coreids;
}
static constexpr auto _gic_enabled_cores = gic_enabled_cores();
#define GIC_ENABLED_CORES           _gic_enabled_cores

#endif
