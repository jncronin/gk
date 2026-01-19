#ifndef DEBOUNCE_H
#define DEBOUNCH_H

#include <cstdint>
#include <climits>
#include "pins.h"

enum pin_state
{
    Bouncing = 0,
    StableLow = 1,
    StableHigh = 2,
    NewStableState = 4,
    LongPress = 8,
    Repeat = 16
};

static inline pin_state operator|(pin_state a, pin_state b)
{
    return static_cast<pin_state>(static_cast<int>(a) |
        static_cast<int>(b));
}

static inline pin_state& operator|=(pin_state &a, pin_state b)
{
    return a = a | b;
}

template <typename pin_t,
    int tick_period_ms = 5,
    int high_time_ms = 20,
    int low_time_ms = high_time_ms,
    int long_press_ms = 1000,
    int repeat_ms = 500,
    typename val_T = unsigned int> class Debounce
{
    protected:
        const pin_t &_p;
        const val_T v;

        static const int high_ticks = high_time_ms / tick_period_ms;
        static const int low_ticks = low_time_ms / tick_period_ms;
        static const int long_press_ticks = long_press_ms / tick_period_ms;
        static const int repeat_ticks = repeat_ms / tick_period_ms;

        static_assert(high_ticks > 0,
            "high_time_ms must be greater than tick_period_ms");
        static_assert(high_ticks < 32,
            "high_time_ms must be less than tick_period_ms * 32");
        static_assert(low_ticks > 0,
            "low_time_ms must be greater than tick_period_ms");
        static_assert(low_ticks < 32,
            "low_time_ms must be less than tick_period_ms * 32");
        
        const uint32_t high_mask = ~(0xffffffffUL << high_ticks);
        const uint32_t low_mask = ~(0xffffffffUL << low_ticks);

        uint32_t pin_samples = 0xaaaaaaaaUL;    // bouncing 101010...
        pin_state cur_stable_state = pin_state::Bouncing;

        int press_ticks = 0;

    public:
        Debounce(const pin_t &p, val_T val = val_T()) : _p(p), v(val) {}

        pin_state tick()
        {
            pin_samples = (pin_samples << 1) |
                (_p.value() ? 1 : 0);
            
            pin_state new_state = pin_state::Bouncing;
            if((pin_samples & high_mask) == high_mask)
                new_state = pin_state::StableHigh;
            else if((pin_samples & low_mask) == 0)
                new_state = pin_state::StableLow;
            
            if((new_state != pin_state::Bouncing) &&
                (new_state != cur_stable_state))
            {
                // new edge
                cur_stable_state = new_state;
                press_ticks = 0;
                return new_state | pin_state::NewStableState;
            }
            else
            {
                // unchanged

                // can we detect a long press?
                if(new_state != pin_state::Bouncing)
                {
                    if(press_ticks < INT_MAX)
                    {
                        // do not allow rollover
                        press_ticks++;
                    }

                    if(press_ticks == long_press_ticks)
                    {
                        // equal, but not greater than - send only once
                        new_state |= pin_state::LongPress;
                    }

                    if((press_ticks % repeat_ticks) == 0)
                    {
                        new_state |= pin_state::Repeat;
                    }
                }
                return new_state;
            }
        }

        const pin &get_pin() const
        {
            return _p;
        }

        val_T get_val() const
        {
            return v;
        }
};

#endif
