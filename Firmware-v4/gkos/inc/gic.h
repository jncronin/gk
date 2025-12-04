#ifndef GIC_H
#define GIC_H

#include <cstdint>
#include "gic_irq_nos.h"
#include "gk_conf.h"

extern "C" void gic_irq_handler(uint32_t aiar);

int gic_set_target(unsigned int irq_no, int core_id = GIC_TARGET_SELF);
int gic_set_enable(unsigned int irq_no);
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
