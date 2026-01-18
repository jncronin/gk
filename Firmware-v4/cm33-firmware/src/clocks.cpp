#include "clocks.h"
#include <cstdint>
#include <stm32mp2xx.h>

volatile uint32_t cur_tick = 0;

void clock_tick()
{
    // tick at 200 Hz frequency (5 ms), cnt at 5 MHz (200 ns) frequency
    cur_tick = cur_tick + 1;
}

timespec clock_cur()
{
    while(true)
    {
        uint32_t _tick_a = cur_tick;
        uint32_t _cur_cnt = TIM6->CNT;
        uint32_t _tick_b = cur_tick;

        if(_tick_a == _tick_b)
        {
            timespec ret;
            ret.tv_nsec = ((long)(_tick_a % 200)) * 5000000L;
            ret.tv_nsec += ((long)_cur_cnt) * 200L;
            ret.tv_sec = (time_t)(_tick_a / 200);
            return ret;
        }
    }
}

uint64_t clock_cur_ns()
{
    auto ts = clock_cur();
    return ts.tv_nsec + (uint64_t)ts.tv_sec * 1000000000;
}

uint64_t clock_cur_us()
{
    return clock_cur_ns() / 1000ULL;
}

uint64_t clock_cur_ms()
{
    return clock_cur_ns() / 1000000ULL;
}

void udelay(unsigned int d)
{
    auto until = clock_cur_us() + (uint64_t)d;
    while(clock_cur_us() < until);
}
