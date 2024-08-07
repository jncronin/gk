#include "gkos.h"
#include "kernel_time.h"
#include "time.h"

kernel_time kernel_time::from_timespec(const timespec *ts, int clock_id)
{
    switch(clock_id)
    {
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC_RAW:
            return kernel_time::from_us(ts->tv_nsec / 1000ULL +
                ts->tv_sec * 1000000ULL);

        case CLOCK_REALTIME:
        {
            struct timespec tzero;
            clock_get_timebase(&tzero);

            auto ns_diff = ts->tv_nsec - tzero.tv_nsec;
            auto s_diff = ts->tv_sec - tzero.tv_sec;

            while(ns_diff < 0)
            {
                s_diff--;
                ns_diff += 1000000000;
            }
            return kernel_time::from_us(
                static_cast<uint64_t>(ns_diff / 1000ULL) +
                static_cast<uint64_t>(s_diff * 1000000ULL));
        }

        default:
            return kernel_time();
    }
}

kernel_time::kernel_time(uint64_t us)
{
    _us = us;
}

kernel_time kernel_time::from_ms(uint64_t ms)
{
    return kernel_time::from_us(ms * 1000ULL);
}

kernel_time kernel_time::from_ns(uint64_t ns)
{
    return kernel_time::from_us(ns / 1000ULL);
}

kernel_time kernel_time::from_us(uint64_t us)
{
    return kernel_time(us);
}

uint64_t kernel_time::to_ms() const
{
    return _us / 1000ULL;
}

uint64_t kernel_time::to_us() const
{
    return _us;
}

uint64_t kernel_time::to_ns() const
{
    return _us * 1000ULL;
}

bool kernel_time::is_valid() const
{
    return _us != 0ULL;
}

bool kernel_time::operator>=(const kernel_time &rhs)
{
    return _us >= rhs._us;
}

bool kernel_time::operator>(const kernel_time &rhs)
{
    return _us > rhs._us;
}

bool kernel_time::operator<=(const kernel_time &rhs)
{
    return _us <= rhs._us;
}

bool kernel_time::operator<(const kernel_time &rhs)
{
    return _us < rhs._us;
}

bool kernel_time::operator==(const kernel_time &rhs)
{
    return _us == rhs._us;
}

kernel_time &kernel_time::operator+=(const kernel_time &rhs)
{
    _us += rhs._us;
    return *this;
}

kernel_time &kernel_time::operator-=(const kernel_time &rhs)
{
    _us -= rhs._us;
    return *this;
}

kernel_time kernel_time::operator+(const kernel_time &rhs)
{
    return kernel_time(_us + rhs._us);
}

kernel_time kernel_time::operator-(const kernel_time &rhs)
{
    return kernel_time(_us - rhs._us);
}

void kernel_time::invalidate()
{
    _us = 0;
}
