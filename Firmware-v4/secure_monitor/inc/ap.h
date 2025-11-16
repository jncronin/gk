#ifndef AP_H
#define AP_H

static const constexpr unsigned int ncores = 2;

#include <cstdint>

struct AP_Data
{
    volatile bool ready;
    void (* volatile epoint)(void *, void *);
    volatile void *p0;
    volatile void *p1;
    volatile uintptr_t el1_stack;
    volatile uintptr_t ttbr1;
    volatile uintptr_t vbar;
};

extern AP_Data aps[ncores];

#endif
