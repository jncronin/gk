#include "clocks.h"
#include "time.h"
#include "kernel_time.h"

kernel_time kernel_time_from_timespec(const timespec *ts, int clock_id)
{
    switch(clock_id)
    {
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC_RAW:
            return *ts;

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

            timespec treal;
            treal.tv_sec = s_diff;
            treal.tv_nsec = ns_diff;
            return treal;
        }

        default:
            return kernel_time_invalid();
    }
}
