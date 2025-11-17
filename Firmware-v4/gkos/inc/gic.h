#ifndef GIC_H
#define GIC_H

void gic_irq_handler();

#define GIC_BASE             0xfffffc004AC00000ULL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000ULL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000ULL)

#define GIC_SGI_YIELD       0
#define GIC_SGI_IPI         1

#define GIC_TARGET_SELF             -1
#define GIC_TARGET_ALL              -2
#define GIC_TARGET_ALL_BUT_SELF     -3

int gic_set_target(unsigned int irq_no, int core_id = GIC_TARGET_SELF);
int gic_set_enable(unsigned int irq_no);
int gic_send_sgi(unsigned int sgi_no, int core_id = GIC_TARGET_SELF);

#endif
