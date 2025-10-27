#include <stm32mp2xx.h>

#include "pins.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

int main()
{
    EV_BLUE.set_as_output();

    while(1)
    {
        EV_BLUE.set();
        for(int i = 0; i < 2500000; i++);
        EV_BLUE.clear();
        for(int i = 0; i < 2500000; i++);
    }

    return 0;
}
