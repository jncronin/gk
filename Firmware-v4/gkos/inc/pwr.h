#ifndef PWR_H
#define PWR_H

#include <atomic>

void init_pwr();

using adouble = std::atomic<double>;

extern adouble vsys, isys, psys;

#endif
