#ifndef CLOCKS_H
#define CLOCKS_H

#include <cstdint>
#include <ctime>

void init_clocks();

enum clock_cpu_speed
{
    cpu_48_48,
    cpu_96_96,
    cpu_192_192,
    cpu_384_192
};

bool clock_set_cpu(clock_cpu_speed speed);

void delay_ms(uint64_t nms);
uint64_t clock_cur_ms();

void clock_get_timebase(struct timespec *tp);
void clock_set_timebase(const struct timespec *tp);
void clock_get_now(struct timespec *tp);
uint64_t clock_timespec_to_ms(const struct timespec &tp);


#endif
