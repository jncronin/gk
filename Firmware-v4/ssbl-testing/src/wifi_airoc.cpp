#include <stm32mp2xx.h>
#include "pins.h"
#include "vmem.h"
#include "clocks.h"

#define GPIOC_VMEM (GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE)
#define RCC_VMEM (RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE)

static const constexpr pin WIFI_REG_ON { GPIOC_VMEM, 7 };

static void wifi_airoc_reset();

void init_wifi_airoc()
{
    RCC->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    __asm__ ("dsb sy\n" ::: "memory");

    WIFI_REG_ON.set_as_output();

    wifi_airoc_reset();
}

void wifi_airoc_reset()
{
    WIFI_REG_ON.clear();
    udelay(10000);
    WIFI_REG_ON.set();
    udelay(250000);
    
}