#ifndef OSSPINLOCK_H
#define OSSPINLOCK_H

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <tuple>
#include <array>
#include "logger.h"

inline __attribute__((always_inline)) static uint64_t DisableInterrupts()
{
    uint64_t cpsr;
    __asm__ volatile(
        "mrs %[cpsr], daif\n"
        "msr daifset, #0b0010\n"
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

template <class T> class CriticalGuard
{
    private:
        T* sl;
        T* slb;
        uint64_t cpsr;

        bool is_locked;
        bool has_disabled_ints;

    public:
        CriticalGuard(T &_sl1, T &_sl2) : sl(&_sl1), slb(&_sl2), is_locked(false), has_disabled_ints(false)
        {
            relock();
        }

        CriticalGuard(T &_sl1) : sl(&_sl1), slb(nullptr), is_locked(false), has_disabled_ints(false)
        {
            relock();
        }

        void relock()
        {
            if(is_locked)
            {
                klog("CriticalGuard: nested lock() - potential bug\n");
            }

            while(true)
            {
                disable_interrupts();
                if(sl->try_lock())
                {
                    if(!slb || slb->try_lock())
                    {
                        is_locked = true;
                        return;
                    }
                    else
                    {
                        sl->unlock();
                    }
                }
                restore_interrupts();
            }
        }

        void unlock(bool lock_test = true)
        {
            // typically we guard some sort of data access - ensure this is complete prior to
            //  re-enabling the lock
            __asm__ volatile("dmb ish\n" ::: "memory");
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
            sl->unlock();
            if(slb)
                slb->unlock();
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
};

template <class T> class Guard
{
    private:
        T* sl;
        uint64_t cpsr;

        bool is_locked;

    public:
        Guard(T &_sl) : sl(&_sl), is_locked(false)
        {
            relock();
        }

        void relock()
        {
            if(is_locked)
            {
                klog("CriticalGuard: nested lock() - potential bug\n");
            }

            while(true)
            {
                if(sl->try_lock())
                {
                    is_locked = true;
                    return;
                }
            }
        }

        void unlock(bool lock_test = true)
        {
            if(!is_locked)
            {
                if(lock_test)
                {
                    klog("CriticalGuard: mismatched lock/unlock - potential bug\n");
                }
                return;
            }
            sl->unlock();
            is_locked = false;
        }

        Guard() = delete;
        Guard(const Guard &) = delete;

        ~Guard()
        {
            unlock(false);
        }
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
