#ifndef GIC_H
#define GIC_H

void gic_irq_handler();
void gic_enable_ap();
void gic_enable_sgi(unsigned int sgi_no);

#endif
