#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <stm32h7xx.h>

inline __attribute__((always_inline)) static uint32_t DisableInterrupts()
{
    auto cpsr = __get_BASEPRI();
    __set_BASEPRI(0x10U);
    return cpsr;
}

inline __attribute__((always_inline)) static void RestoreInterrupts(uint32_t cpsr)
{
    __set_BASEPRI(cpsr);
}

/* Need to use HSEM here because STM32H7 does not implement bus locking on AXI */
template <typename T> static inline void cmpxchg(volatile T* ptr, T* oldval, T newval)
{
    while(HSEM->RLR[0] == 0);
    if(*ptr == *oldval)
    {
        *ptr = newval;
    }
    else
    {
        *oldval = *ptr;
    }
    HSEM->R[0] = 0;
}

template <typename T> static inline void set(volatile T* ptr, T newval)
{
    while(HSEM->RLR[0] == 0);
    *ptr = newval;
    HSEM->R[0] = 0;
}

#endif
