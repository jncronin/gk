#ifndef CLOCKS_H
#define CLOCKS_H

#include <ctime>

#include "gkos_boot_interface.h"
#include "cpuclock.h"

void init_clocks(const gkos_boot_interface *gbi);

timespec clock_cur();
uint64_t clock_cur_ns();
uint64_t clock_cur_us();
uint64_t clock_cur_ms();
void udelay(unsigned int);

void clock_get_timebase(struct timespec *tp);
void clock_get_realtime(struct timespec *tp);
int clock_get_timespec_from_rtc(timespec *ts);
void clock_set_timebase(const struct timespec *tp);
int clock_set_rtc_from_timespec(const timespec *ts);

unsigned int clock_set_cpu_and_vddcpu(unsigned int freq);

constexpr inline timespec operator+=(timespec &a, const timespec &b)
{
    a.tv_nsec += b.tv_nsec;
    if(a.tv_nsec >= 1000000000)
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
    if(a.tv_nsec < 0)
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
