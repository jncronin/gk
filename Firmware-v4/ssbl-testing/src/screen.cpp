#include "screen.h"
#include "pins.h"
#include "vmem.h"
#include "i2c.h"

#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
static const constexpr pin PWM_BACKLIGHT { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE), 4 };
static const constexpr pin LS_OE_N { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE), 0 };
static const constexpr pin CTP_WAKE { (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOA_BASE), 2 };

void init_screen()
{
    RCC_VMEM->GPIOICFGR |= RCC_GPIOICFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    RCC_VMEM->GPIOGCFGR |= RCC_GPIOGCFGR_GPIOxEN;
    RCC_VMEM->GPIOACFGR |= RCC_GPIOACFGR_GPIOxEN;
    RCC_VMEM->GPIOBCFGR |= RCC_GPIOBCFGR_GPIOxEN;
    RCC_VMEM->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    RCC_VMEM->LTDCCFGR |= RCC_LTDCCFGR_LTDCEN;
    __asm__ volatile("dsb sy\n");

    // turn on backlight
    PWM_BACKLIGHT.set_as_output();
    PWM_BACKLIGHT.set();

    // enable level shifter
    LS_OE_N.set_as_output();
    LS_OE_N.clear();

    // enable CTP
    CTP_WAKE.set_as_output();
    CTP_WAKE.set();
}

void screen_poll()
{
    static bool backlight = false;

    if(backlight)
    {
        PWM_BACKLIGHT.clear();
        backlight = false;
    }
    else
    {
        PWM_BACKLIGHT.set();
        backlight = true;
    }

    auto &i2c4 = i2c(4);
    // check ctp responds
    uint8_t reg0;
    i2c4.RegisterRead(0x40, (uint8_t)0, &reg0, 1);
    klog("ctp: reg0: %x\n", reg0);

}
