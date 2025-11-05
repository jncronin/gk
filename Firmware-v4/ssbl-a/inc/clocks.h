#ifndef CLOCKS_H
#define CLOCKS_H

#include <time.h>

void init_clocks();
void clock_set_cpu(unsigned int freq);
void clock_irq_handler();
timespec clock_cur();
uint64_t clock_cur_ns();
uint64_t clock_cur_us();
uint64_t clock_cur_ms();

#endif
