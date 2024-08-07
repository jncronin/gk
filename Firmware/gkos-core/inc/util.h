#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <stm32h7xx.h>

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

static inline unsigned int ptr_to_hsem_id(volatile void* ptr)
{
    /* split hsem accesses so we can use more than one
        need to profile to determine the best algorithm here but at a guess this
        seems okay */
    unsigned int ret = (unsigned int)(uintptr_t)ptr;
    return (ret >> 2) & 0x1fU;
}

/* Need to use HSEM here because STM32H7 does not implement bus locking on AXI */
template <typename T> static inline void cmpxchg(volatile T* ptr, T* oldval, T newval)
{
    while(HSEM->RLR[ptr_to_hsem_id(ptr)] == 0);
    if(*ptr == *oldval)
    {
        *ptr = newval;
    }
    else
    {
        *oldval = *ptr;
    }
    HSEM->R[ptr_to_hsem_id(ptr)] = 0;
}

template <typename T> static inline void set(volatile T* ptr, T newval)
{
    while(HSEM->RLR[ptr_to_hsem_id(ptr)] == 0);
    *ptr = newval;
    HSEM->R[ptr_to_hsem_id(ptr)] = 0;
}

#endif
