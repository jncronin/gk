#ifndef PWR_H
#define PWR_H

extern "C" int pwr_disable_regulators();
int pwr_set_vos_high();
double pwr_get_vdd();
void *pwr_monitor_thread(void *p);

#endif
