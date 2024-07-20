#ifndef CLOCKS_H
#define CLOCKS_H

#include <cstdint>
#include <ctime>
#include <time.h>

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
        static kernel_time from_ns(uint64_t ns);
        static kernel_time from_us(uint64_t us);
        static kernel_time from_ms(uint64_t ms);
        static kernel_time from_timespec(const timespec *ts, int clock_id = CLOCK_MONOTONIC);

        uint64_t to_ns() const;
        uint64_t to_us() const;
        uint64_t to_ms() const;
        void to_timespec(timespec *ts, int clock_id = CLOCK_MONOTONIC) const;

        kernel_time(uint64_t us = 0ULL);

        bool is_valid() const;
        void invalidate();

        kernel_time operator+(const kernel_time &rhs);
        kernel_time operator-(const kernel_time &rhs);
        kernel_time &operator+=(const kernel_time &rhs);
        kernel_time &operator-=(const kernel_time &rhs);
        bool operator==(const kernel_time &rhs);
        bool operator<(const kernel_time &rhs);
        bool operator<=(const kernel_time &rhs);
        bool operator>(const kernel_time &rhs);
        bool operator>=(const kernel_time &rhs);
};

bool clock_set_cpu(clock_cpu_speed speed);

void delay_ms(uint64_t nms);
uint64_t clock_cur_ms();
uint64_t clock_cur_us();
kernel_time clock_cur();

void clock_get_timebase(struct timespec *tp);
void clock_set_timebase(const struct timespec *tp);
void clock_get_now(struct timespec *tp);
uint64_t clock_timespec_to_ms(const struct timespec &tp);

int clock_set_rtc_from_timespec(const struct timespec *tp);
int clock_get_timespec_from_rtc(struct timespec *tp);

#endif
