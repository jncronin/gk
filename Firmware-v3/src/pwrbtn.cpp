#include <stm32h7rsxx.h>
#include "pins.h"
#include "gk_conf.h"

static const constexpr pin MCU_PWR_EN { GPIOC, 13 };

extern "C" INTFLASH_FUNCTION int pwrbtn_setvregen(int val)
{
    if(val)
    {
        MCU_PWR_EN.set();
    }
    else
    {
        MCU_PWR_EN.clear();
    }
    MCU_PWR_EN.set_as_output();
    return 0;
}
