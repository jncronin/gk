#ifndef DDR_H
#define DDR_H

void init_ddr();
void ddr_set_mt(uint32_t mt_s); // Set speed in MT/s (i.e. 2x DDRCLK speed for DDR)

#endif
