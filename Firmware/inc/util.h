#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <stm32h7xx.h>

inline __attribute__((always_inline)) static uint32_t DisableInterrupts()
{
    auto cpsr = __get_PRIMASK();
    __disable_irq();
    return cpsr;
}

inline __attribute__((always_inline)) static void RestoreInterrupts(uint32_t cpsr)
{
    __set_PRIMASK(cpsr);
}


#endif
