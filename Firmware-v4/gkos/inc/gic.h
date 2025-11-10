#ifndef GIC_H
#define GIC_H

void gic_irq_handler();

#define GIC_BASE             0xfffffc004AC00000ULL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000ULL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000ULL)


#endif
