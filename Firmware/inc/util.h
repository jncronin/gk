#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <stm32h7rsxx.h>

inline __attribute__((always_inline)) static uint32_t DisableInterrupts()
{
#if GK_USE_IRQ_PRIORITIES
    auto cpsr = __get_BASEPRI();
    __set_BASEPRI(0x10U);
#else
    auto cpsr = __get_PRIMASK();
    __disable_irq();
#endif
    return cpsr;
}

inline __attribute__((always_inline)) static void RestoreInterrupts(uint32_t cpsr)
{
#if GK_USE_IRQ_PRIORITIES
    __set_BASEPRI(cpsr);
#else
    __set_PRIMASK(cpsr);
#endif
}

template <typename T> static inline void cmpxchg(volatile T* ptr, T* oldval, T newval)
{
    if(*ptr == *oldval)
    {
        *ptr = newval;
    }
    else
    {
        *oldval = *ptr;
    }
}

template <typename T> static inline void set(volatile T* ptr, T newval)
{
    *ptr = newval;
}

#endif
