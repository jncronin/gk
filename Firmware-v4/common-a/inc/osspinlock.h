#ifndef OSSPINLOCK_H
#define OSSPINLOCK_H

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <tuple>
#include <array>
#include "logger.h"

static inline unsigned int get_core_id()
{
    uint64_t mpidr_el1;
    __asm__ volatile("mrs %[mpidr_el1], mpidr_el1\n" : [mpidr_el1] "=r" (mpidr_el1));
    return (unsigned int)(mpidr_el1 & 0xffU);
}

typedef uint64_t interrupt_state_t;

inline __attribute__((always_inline)) static interrupt_state_t DisableInterrupts()
{
    uint64_t cpsr;
    __asm__ volatile(
        "mrs %[cpsr], daif\n"
        "msr daifset, #0b0010\n"
        : [cpsr] "=r" (cpsr) : : "memory");
    return cpsr;
}

inline __attribute__((always_inline)) static void RestoreInterrupts(interrupt_state_t cpsr)
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
        bool lock();
        bool try_lock();
        void unlock(bool unlock = true);
};

static_assert(sizeof(Spinlock) == 4);

template <class T>
concept Spinlock_c = requires(T a)
{
    { a.lock() } -> std::same_as<bool>;
    { a.try_lock() } -> std::same_as<bool>;
    { a.unlock() } -> std::same_as<void>;
};

template<class... Ts>
concept all_same = 
        sizeof...(Ts) < 2 ||
        std::conjunction_v<
            std::is_same<std::tuple_element_t<0, std::tuple<Ts...>>, Ts>...
        >;

template <Spinlock_c... Spinlock_t> requires all_same<Spinlock_t...>
class CriticalGuard
{
    private:
        using sl_t = std::tuple_element_t<0, std::tuple<Spinlock_t...>>;
        std::array<bool, sizeof...(Spinlock_t)> locked = {};

        // Variable backoff logic
        const unsigned int base_window = 16;
        const unsigned int max_window = 1024;
        const unsigned int core_step = 8;
        unsigned int prng_state, current_window = base_window;

    public:
        CriticalGuard(Spinlock_t &... args) : sl(args...), is_locked(false), has_disabled_ints(false), _try_only(false)
        {
            prng_state = (unsigned int)(uintptr_t)this;
            relock();
        }

        CriticalGuard(bool try_once, Spinlock_t &... args) : sl(args...), is_locked(false), has_disabled_ints(false), _try_only(try_once)
        {
            prng_state = (unsigned int)(uintptr_t)this;
            relock();
        }

        void relock()
        {
            if(is_locked)
            {
                klog("CriticalGuard: nested lock() - potential bug\n");
            }
            __asm__ volatile("sevl\n" ::: "memory");
            while(true)
            {
                __asm__ volatile("wfe\n" ::: "memory");
                size_t i = 0U;
                disable_interrupts();
                std::fill(locked.begin(), locked.end(), false);
                if(std::apply([&](Spinlock_t &... apply_args) { return (... && (locked[i++] = apply_args.try_lock(), locked[i - 1])); }, sl))
                {
                    is_locked = true;
                    return;
                }

                unsigned int core_id = get_core_id();

                i = 0U;
                std::apply([&](Spinlock_t &... apply_args) { (... , apply_args.unlock(locked[i++])); }, sl);
                restore_interrupts();

                if(_try_only)
                {
                    // only try once - code can see if we're locked by using IsLocked()
                    return;
                }

                // Variable backoff logic

                // XORshift prng
                prng_state ^= prng_state << 13;
                prng_state ^= prng_state >> 7;
                prng_state ^= prng_state << 17;

                auto delay = prng_state % current_window + core_id * core_step;

                for(auto j = 0u; j < delay; j++)
                    __asm__ volatile("yield\n" ::: "memory");
            }
        }

        void unlockone(size_t id)
        {
            std::array<bool, sizeof...(Spinlock_t)> to_unlock{};
            to_unlock[id] = locked[id];

            size_t i = 0U;
            std::apply([&](Spinlock_t &... apply_args) { (... , apply_args.unlock(to_unlock[i++])); }, sl);
            locked[id] = false;
        }

        void unlock(bool lock_test = true)
        {
            if(!is_locked)
            {
                if(lock_test)
                {
                    klog("CriticalGuard: mismatched lock/unlock - potential bug\n");
                }
                if(has_disabled_ints)
                {
                    restore_interrupts();
                }
                return;
            }

            size_t i = 0U;
            std::apply([&](Spinlock_t &... apply_args) { (... , apply_args.unlock(locked[i++])); }, sl);
            is_locked = false;
            restore_interrupts();
        }

        void disable_interrupts()
        {
            if(has_disabled_ints)
            {
                klog("CriticalGuard: nested interrupt disable - potential bug\n");
            }
            cpsr = DisableInterrupts();
            has_disabled_ints = true;
        }

        void restore_interrupts()
        {
            if(!has_disabled_ints)
            {
                klog("CriticalGuard: mismatched restore/disable interrupts\n");
            }
            has_disabled_ints = false;
            RestoreInterrupts(cpsr);
        }

        CriticalGuard() = delete;
        CriticalGuard(const CriticalGuard &) = delete;

        ~CriticalGuard()
        {
            unlock(false);
        }

        bool IsLocked() const { return is_locked; }

    private:
        std::tuple<Spinlock_t &...> sl;
        uint64_t cpsr;
        bool is_locked;
        bool has_disabled_ints;
        bool _try_only;
};

template <Spinlock_c... Spinlock_t> requires all_same<Spinlock_t...>
class Guard
{
    private:
        using sl_t = std::tuple_element_t<0, std::tuple<Spinlock_t...>>;

    public:
        Guard(Spinlock_t &... args) : sl(args...)
        {
            constexpr std::size_t size = sizeof...(args);
            while(true)
            {
                std::array<bool, size> locked = {};
                size_t i = 0U;
                if(std::apply([&](Spinlock_t &... apply_args) { return (... && (locked[i++] = apply_args.try_lock(), locked[i - 1])); }, sl))
                    return;

                i = 0U;
                std::apply([&](Spinlock_t &... apply_args) { (... , apply_args.unlock(locked[i++])); }, sl);
            }
        }

        Guard() = delete;
        Guard(const Guard &) = delete;

        ~Guard()
        {
            std::apply([](Spinlock_t &... apply_args) { (... , apply_args.unlock()); }, sl);
        }

    private:
        std::tuple<Spinlock_t &...> sl;
};

class UninterruptibleGuard
{
    public:
        inline UninterruptibleGuard() { cpsr = DisableInterrupts(); }
        inline ~UninterruptibleGuard() { RestoreInterrupts(cpsr); }

    private:
        uint64_t cpsr;
};

#endif
