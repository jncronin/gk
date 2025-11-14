#ifndef OSSPINLOCK_H
#define OSSPINLOCK_H

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <tuple>

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
        bool lock();
        bool try_lock();
        void unlock();
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

    public:
        CriticalGuard(Spinlock_t &... args) : sl(args...)
        {
            while(true)
            {
                cpsr = DisableInterrupts();
                if(std::apply([](Spinlock_t &... apply_args) { return (... && apply_args.try_lock()); }, sl))
                    return;

                std::apply([](Spinlock_t &... apply_args) { (... , apply_args.unlock()); }, sl);
                RestoreInterrupts(cpsr);
            }
        }

        CriticalGuard() = delete;
        CriticalGuard(const CriticalGuard &) = delete;

        ~CriticalGuard()
        {
            std::apply([](Spinlock_t &... apply_args) { (... , apply_args.unlock()); }, sl);
            RestoreInterrupts(cpsr);
        }

    private:
        std::tuple<Spinlock_t &...> sl;
        uint64_t cpsr;
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
