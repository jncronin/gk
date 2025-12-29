#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <cstdint>
#include <ctime>

using kernel_time = timespec;

static constexpr kernel_time kernel_time_invalid()
{
    return timespec { .tv_sec = 0, .tv_nsec = 0 };
}

static constexpr kernel_time kernel_time_from_ns(uint64_t ns)
{
    timespec ts { .tv_sec =  (time_t)(ns / 1000000000ULL), .tv_nsec = (long int)(ns % 1000000000ULL) };
    return ts;
}

static constexpr kernel_time kernel_time_from_us(uint64_t us)
{
    timespec ts { .tv_sec =  (time_t)(us / 1000000ULL), .tv_nsec = (long int)((us % 1000000ULL) * 1000ULL) };
    return ts;
}

static constexpr kernel_time kernel_time_from_ms(uint64_t ms)
{
    timespec ts { .tv_sec =  (time_t)(ms / 1000ULL), .tv_nsec = (long int)((ms % 1000ULL) * 1000000ULL) };
    return ts;
}

kernel_time kernel_time_from_timespec(const timespec *ts, int clock_id = CLOCK_MONOTONIC);

static constexpr uint64_t kernel_time_to_ns(const kernel_time &kt) { return kt.tv_nsec + kt.tv_sec * 1000000000ULL; }
static constexpr uint64_t kernel_time_to_us(const kernel_time &kt) { return kt.tv_nsec / 1000ULL + kt.tv_sec * 1000000ULL; }
static constexpr uint64_t kernel_time_to_ms(const kernel_time &kt) { return kt.tv_nsec / 1000000ULL + kt.tv_sec * 1000ULL; }
void kernel_time_to_timespec(const kernel_time &kt, timespec *ts, int clock_id = CLOCK_MONOTONIC);

static constexpr bool kernel_time_is_valid(const kernel_time &kt) { return kt.tv_nsec != 0 || kt.tv_sec != 0; }

constexpr inline timespec operator+=(timespec &a, const timespec &b)
{
    a.tv_nsec += b.tv_nsec;
    while(a.tv_nsec >= 1000000000)
    {
        a.tv_nsec -= 1000000000;
        a.tv_sec++;
    }
    a.tv_sec += b.tv_sec;
    return a;
}
constexpr inline timespec operator-=(timespec &a, const timespec &b)
{
    a.tv_nsec -= b.tv_nsec;
    while(a.tv_nsec < 0)
    {
        a.tv_nsec += 1000000000;
        a.tv_sec--;
    }
    a.tv_sec -= b.tv_sec;
    return a;
}
constexpr inline timespec operator+(const timespec &a, const timespec &b)
{
    auto ret = a;
    return ret += b;
}
constexpr inline timespec operator-(const timespec &a, const timespec &b)
{
    auto ret = a;
    return ret -= b;
}
constexpr inline bool operator>=(const timespec &a, const timespec &b)
{
    if(a.tv_sec < b.tv_sec)
        return false;
    if(a.tv_sec > b.tv_sec)
        return true;
    return a.tv_nsec >= b.tv_nsec;
}
constexpr inline bool operator>(const timespec &a, const timespec &b)
{
    if(a.tv_sec < b.tv_sec)
        return false;
    if(a.tv_sec > b.tv_sec)
        return true;
    return a.tv_nsec > b.tv_nsec;
}
constexpr inline bool operator<=(const timespec &a, const timespec &b)
{
    if(a.tv_sec < b.tv_sec)
        return true;
    if(a.tv_sec > b.tv_sec)
        return false;
    return a.tv_nsec <= b.tv_nsec;
}
constexpr inline bool operator<(const timespec &a, const timespec &b)
{
    if(a.tv_sec < b.tv_sec)
        return true;
    if(a.tv_sec > b.tv_sec)
        return false;
    return a.tv_nsec < b.tv_nsec;
}
constexpr inline bool operator==(const timespec &a, const timespec &b)
{
    if(a.tv_sec != b.tv_sec)
        return false;
    return a.tv_nsec == b.tv_nsec;
}
constexpr inline bool operator!=(const timespec &a, const timespec &b)
{
    if(a.tv_sec != b.tv_sec)
        return true;
    return a.tv_nsec != b.tv_nsec;
}

#endif
