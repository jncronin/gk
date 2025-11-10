#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <cstdint>

inline __attribute__((always_inline)) static uint64_t DisableInterrupts()
{
    uint64_t cpsr;
    __asm__ volatile(
        "mrs %[cpsr], daif\n"
        "msr daifclr, #0b0010\n"
        : [cpsr] "=r" (cpsr) : : "memory");
    return cpsr;
}

inline __attribute__((always_inline)) static void RestoreInterrupts(uint64_t cpsr)
{
    __asm__ volatile(
        "msr daif, %[cpsr]\n"
        : : [cpsr] "r" (cpsr) : "memory");
}

class Spinlock
{
    protected:
        volatile uint32_t _lock_val = 0;

    public:
        void lock();
        bool try_lock();
        void unlock();
};

static_assert(sizeof(Spinlock) == 4);

#endif
