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

class kernel_time
{
    private:
        uint64_t _us;

    public:
        kernel_time from_ns(uint64_t ns);
        kernel_time from_us(uint64_t us);
        kernel_time from_ms(uint64_t ms);
        kernel_time from_timespec(const timespec *ts);

        uint64_t to_ns();
        uint64_t to_us();
        uint64_t to_ms();
        void to_timespec(timespec *ts);
};

bool clock_set_cpu(clock_cpu_speed speed);

void delay_ms(uint64_t nms);
uint64_t clock_cur_ms();
uint64_t clock_cur_us();

void clock_get_timebase(struct timespec *tp);
void clock_set_timebase(const struct timespec *tp);
void clock_get_now(struct timespec *tp);
uint64_t clock_timespec_to_ms(const struct timespec &tp);


#endif
