#ifndef GIC_H
#define GIC_H

#include <cstdint>
#include "gic_irq_nos.h"

extern "C" void gic_irq_handler(uint32_t aiar);

int gic_set_target(unsigned int irq_no, int core_id = GIC_TARGET_SELF);
int gic_set_enable(unsigned int irq_no);
int gic_send_sgi(unsigned int sgi_no, int core_id = GIC_TARGET_SELF);

#endif
