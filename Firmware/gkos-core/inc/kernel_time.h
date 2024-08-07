#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <stdint.h>
#include <time.h>

class GKOS_FUNC(kernel_time)
{
    private:
        uint64_t _us;

    public:
        static GKOS_FUNC(kernel_time) from_ns(uint64_t ns);
        static GKOS_FUNC(kernel_time) from_us(uint64_t us);
        static GKOS_FUNC(kernel_time) from_ms(uint64_t ms);
        static GKOS_FUNC(kernel_time) from_timespec(const timespec *ts, int clock_id = CLOCK_MONOTONIC);

        uint64_t to_ns() const;
        uint64_t to_us() const;
        uint64_t to_ms() const;
        void to_timespec(timespec *ts, int clock_id = CLOCK_MONOTONIC) const;

        GKOS_FUNC(kernel_time)(uint64_t us = 0ULL);

        bool is_valid() const;
        void invalidate();

        GKOS_FUNC(kernel_time) operator+(const GKOS_FUNC(kernel_time) &rhs);
        GKOS_FUNC(kernel_time) operator-(const GKOS_FUNC(kernel_time) &rhs);
        GKOS_FUNC(kernel_time) &operator+=(const GKOS_FUNC(kernel_time) &rhs);
        GKOS_FUNC(kernel_time) &operator-=(const GKOS_FUNC(kernel_time) &rhs);
        bool operator==(const GKOS_FUNC(kernel_time) &rhs);
        bool operator<(const GKOS_FUNC(kernel_time) &rhs);
        bool operator<=(const GKOS_FUNC(kernel_time) &rhs);
        bool operator>(const GKOS_FUNC(kernel_time) &rhs);
        bool operator>=(const GKOS_FUNC(kernel_time) &rhs);
};

uint64_t clock_cur_ms();
uint64_t clock_cur_us();
GKOS_FUNC(kernel_time) clock_cur();

void clock_get_timebase(struct timespec *tp);
void clock_set_timebase(const struct timespec *tp);
void clock_get_now(struct timespec *tp);
uint64_t clock_timespec_to_ms(const struct timespec &tp);

int clock_set_rtc_from_timespec(const struct timespec *tp);
int clock_get_timespec_from_rtc(struct timespec *tp);



#endif
