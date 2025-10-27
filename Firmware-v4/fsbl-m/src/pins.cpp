#include <stm32mp2xx.h>

#include "pins.h"

#define RCC_ENABLE(x) if(gpio == x) { RCC->x ## CFGR |= 2; (void)RCC->x ## CFGR; }

void pin_set(const struct pin &p, int mode, int ospeed, int af, int otype, int pup)
{
    GPIO_TypeDef *gpio = p.gpio;
    
    RCC_ENABLE(GPIOA);
    RCC_ENABLE(GPIOB);
    RCC_ENABLE(GPIOC);
    RCC_ENABLE(GPIOD);
    RCC_ENABLE(GPIOE);
    RCC_ENABLE(GPIOF);
    RCC_ENABLE(GPIOG);
    RCC_ENABLE(GPIOH);
    RCC_ENABLE(GPIOI);
    RCC_ENABLE(GPIOJ);
    RCC_ENABLE(GPIOK);
    RCC_ENABLE(GPIOZ);

    int pin = p.pin;

    gpio->MODER &= ~(3UL << (pin * 2));
    gpio->MODER |= (mode << (pin * 2));

    /* limit GPIOC13/14/15 speed */
    if(gpio == GPIOC && ((pin == 13) || (pin == 14) || (pin == 15)))
        ospeed = 0;

    gpio->OSPEEDR &= ~(3UL << (pin * 2));
    gpio->OSPEEDR |= (ospeed << (pin * 2));
    if(pin < 8)
    {
        gpio->AFR[0] &= ~(15UL << (pin * 4));
        gpio->AFR[0] |= (af << (pin * 4));
    }
    else
    {
        gpio->AFR[1] &= ~(15UL << ((pin - 8) * 4));
        gpio->AFR[1] |= (af << ((pin - 8) * 4));
    }
    gpio->OTYPER &= ~(1UL << pin);
    gpio->OTYPER |= (otype << pin);

    gpio->PUPDR &= ~(3UL << (pin * 2));
    gpio->PUPDR |= (pup << (pin * 2));
}

void pin::set() const
{
    gpio->BSRR = 1UL << pin;
}

void pin::clear() const
{
    gpio->BSRR = 1UL << (pin + 16);
}

bool pin::value() const
{
    return (gpio->IDR & (1UL << pin)) != 0;
}

void pin::set_as_af(otype _otype, pup _pup, int af_override) const
{
    pin_set(*this, 2, 3, af_override == -1 ? af : af_override,
        (int)_otype, (int)_pup);
}

void pin::set_as_output(otype _otype, pup _pup) const
{
    pin_set(*this, 1, 3, 0, (int)_otype, (int)_pup);
}

void pin::set_as_input(pup _pup) const
{
    pin_set(*this, 0, 3, 0, 0, (int)_pup);
}
