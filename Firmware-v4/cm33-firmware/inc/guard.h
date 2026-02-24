#ifndef GUARD_H
#define GUARD_H

#include <cstdint>
#include <stm32mp2xx.h>

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

class CriticalGuard
{
    public:
        CriticalGuard();
        ~CriticalGuard();
        CriticalGuard(const CriticalGuard&) = delete;

    protected:
        uint32_t cpsr;
};

using UninterruptibleGuard = CriticalGuard;

#endif
