#include <stm32h7rsxx.h>
#include "pins.h"
#include "btnled.h"
#include "gk_conf.h"

INTFLASH_RDATA static const constexpr pin MCU_PWR_EN { GPIOC, 13 };

extern "C" INTFLASH_FUNCTION int pwrbtn_setvregen(int val)
{
    MCU_PWR_EN.set_as_output();
    if(val)
    {
        MCU_PWR_EN.set();
    }
    else
    {
        MCU_PWR_EN.clear();
    }

    btnled_setcolor_init(0xff0000);

    return 0;
}
