#ifndef CLOCKS_H
#define CLOCKS_H

#include <cstdint>
#include <ctime>
#include <time.h>
#include <string>

extern "C" void init_clocks();

class kernel_time
{
    private:
        uint64_t _us;

    public:
        static constexpr kernel_time from_ns(uint64_t ns)
        {
            return kernel_time::from_us(ns / 1000ULL);
        }

        static constexpr kernel_time from_us(uint64_t us)
        {
            return kernel_time(us);
        }

        static constexpr kernel_time from_ms(uint64_t ms)
        {
            return kernel_time::from_us(ms * 1000ULL);
        }

        static kernel_time from_timespec(const timespec *ts, int clock_id = CLOCK_MONOTONIC);

        uint64_t to_ns() const;
        uint64_t to_us() const;
        uint64_t to_ms() const;
        void to_timespec(timespec *ts, int clock_id = CLOCK_MONOTONIC) const;

        constexpr kernel_time(uint64_t us = 0ULL) : _us(us) {}

        bool is_valid() const;
        void invalidate();

        kernel_time operator+(const kernel_time &rhs);
        kernel_time operator-(const kernel_time &rhs);
        kernel_time &operator+=(const kernel_time &rhs);
        kernel_time &operator-=(const kernel_time &rhs);
        bool operator==(const kernel_time &rhs) const;
        bool operator<(const kernel_time &rhs) const;
        bool operator<=(const kernel_time &rhs) const;
        bool operator>(const kernel_time &rhs) const;
        bool operator>=(const kernel_time &rhs) const;
};

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
int clock_set_tzone(const std::string &tzstr);

timespec operator+(const timespec &a, const timespec &b);
timespec operator-(const timespec &a, const timespec &b);

#endif
