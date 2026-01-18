#ifndef PINS_H
#define PINS_H

#include <stm32mp2xx.h>

struct pin
{
    GPIO_TypeDef *gpio;
    int pin;
    int af;

    enum otype { PushPull = 0, OpenDrain = 1 };
    enum pup { None = 0, PullUp = 1, PullDown = 2 };

    void set() const;
    void clear() const;
    bool value() const;
    void set_as_af(otype otype = PushPull, pup pup = None, int af = -1) const;
    void set_as_output(otype otype = PushPull, pup pup = None) const;
    void set_as_input(pup pup = None) const;
    void set_as_analog(pup pup = None) const;
};

void pin_set(const struct pin &p, int mode = 1, int ospeed = 3, int af = 0, int otype = 0, int pup = 0);

#endif
