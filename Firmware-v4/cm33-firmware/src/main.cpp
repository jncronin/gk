#include <stm32mp2xx.h>
#include "pins.h"

static const constexpr pin EV_BLUE      { GPIOJ, 7 };
static const constexpr pin EV_RED       { GPIOH, 4 };
static const constexpr pin EV_GREEN     { GPIOD, 8 };
static const constexpr pin EV_ORANGE    { GPIOJ, 6 };

int main()
{
    EV_GREEN.set_as_output();

    // say hi
    for(int n = 0; n < 10; n++)
    {
        EV_GREEN.set();
        for(int i = 0; i < 2500000; i++);
        EV_GREEN.clear();
        for(int i = 0; i < 2500000; i++);
    }

    while(true)
    {
        __SEV();
    }
}
