#ifndef GIC_IRQ_NOS_H
#define GIC_IRQ_NOS_H

/* Keep this file so it can be included from assembly - defines only */
#define GIC_BASE             0xfffffc004AC00000ULL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE+0x10000ULL)
#define GIC_INTERFACE_BASE   (GIC_BASE+0x20000ULL)

#define GIC_SGI_YIELD       0
#define GIC_SGI_IPI         1

#define GIC_PPI_NS_PHYS     30

#define GIC_IRQ_SPURIOUS    1023

#define GIC_TARGET_SELF             -1
#define GIC_TARGET_ALL              -2
#define GIC_TARGET_ALL_BUT_SELF     -3



#endif
