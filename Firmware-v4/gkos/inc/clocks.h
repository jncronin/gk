#ifndef CLOCKS_H
#define CLOCKS_H

#include <ctime>

#include "gkos_boot_interface.h"

void init_clocks(const gkos_boot_interface *gbi);

timespec clock_cur();
uint64_t clock_cur_ns();
uint64_t clock_cur_us();
uint64_t clock_cur_ms();
void udelay(unsigned int);


#endif
