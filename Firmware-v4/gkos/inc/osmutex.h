#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <cstdint>
#include "osspinlock.h"
#include "kernel_time.h"
#include <unordered_map>
#include <unordered_set>
#include "threadproclist.h"
#include <memory>
#include <vector>
#include <limits>

class Thread;
using PThread = std::shared_ptr<Thread>;

class Mutex
{
    protected:
        id_t owner = 0;
        std::unordered_set<id_t> waiting_threads = std::unordered_set<id_t>(8);
        bool is_recursive = false;
        bool echeck = false;
        int lockcount = 0;

    public:
        Spinlock sl;
        id_t id;
        Mutex(bool recursive = false, bool error_check = false);
        void lock(bool allow_deadlk = false);
        bool try_lock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        void _lock(bool allow_deadlk = false);
        bool _try_lock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool unlock(int *reason = nullptr, bool force = false);
        std::pair<bool, std::vector<PThread>> _unlock(int *reason = nullptr, bool force = false);
        bool unlock(bool do_unlock);
        bool try_delete(int *reason = nullptr);
};

class RwLock
{
    protected:
        id_t wrowner = 0;
        std::unordered_set<id_t> rdowners = std::unordered_set<id_t>(8);
        std::unordered_set<id_t> waiting_threads = std::unordered_set<id_t>(8);
        Spinlock sl;

    public:
        id_t id;
        bool try_wrlock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool try_rdlock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool try_delete(int *reason = nullptr);
        bool unlock(int *reason = nullptr);
};

class UserspaceSemaphore
{
    protected:
        unsigned int val;
        std::unordered_set<id_t> waiting_threads = std::unordered_set<id_t>(8);
        Spinlock sl;

    public:
        id_t id;
        bool try_wait(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        void post(int n=1, bool add=true);
        bool try_delete(int *reason = nullptr);
        unsigned int get_value();

        UserspaceSemaphore(unsigned int val = 0);
};

class Condition
{
    protected:
        struct timeout { kernel_time tout; int *signalled; };
        std::unordered_map<id_t, timeout> waiting_threads;

    public:
        Spinlock sl;
        id_t id;
        void Wait(kernel_time tout = kernel_time(), int *signalled_ret = nullptr);
        void _Wait(kernel_time tout = kernel_time(), int *signalled_ret = nullptr);
        void Signal(bool all = true);
        ~Condition();
};

class SimpleSignal
{
    protected:
        uint32_t signal_value = 0;
        uint32_t max_value = std::numeric_limits<decltype(max_value)>::max();
        Spinlock sl;

    public:
        id_t waiting_thread = 0;
        enum SignalOperation { Noop, Set, Or, And, Xor, Add, Sub, AddIfLessThanMax };
        uint32_t Wait(SignalOperation op = Noop, uint32_t val = 0, kernel_time tout = kernel_time());
        uint32_t WaitOnce(SignalOperation op = Noop, uint32_t val = 0, kernel_time tout = kernel_time());
        uint32_t Value();
        void Signal(SignalOperation op = Set, uint32_t val = 0x1);
        SimpleSignal(uint32_t val = 0, uint32_t max_val = std::numeric_limits<decltype(max_value)>::max());

    protected:
        void do_op(SignalOperation op, uint32_t vop);
};

class BinarySemaphore
{
    protected:
        SimpleSignal ss;

    public:
        bool Wait(kernel_time tout = kernel_time());
        bool WaitOnce(kernel_time tout = kernel_time());
        void Signal();
        void Clear();
        bool Value();
};

class CountingSemaphore
{
    protected:
        SimpleSignal ss;

    public:
        bool Wait(kernel_time tout = kernel_time());
        bool WaitOnce(kernel_time tout = kernel_time());
        void Signal();
        unsigned int Value();
        CountingSemaphore(unsigned int value = 0) : ss(value) {};
};

template <typename... Mutex_t> requires all_same<Mutex_t...>
class MutexGuard
{
    private:
        using sl_t = std::tuple_element_t<0, std::tuple<Mutex_t...>>;

    public:
        MutexGuard(Mutex_t &... args) : sl(args...), is_locked(false)
        {
            relock();
        }

        void relock()
        {
            if(is_locked)
            {
                klog("MutexGuard: nested lock() - potential bug\n");
            }
            constexpr std::size_t size = sizeof...(Mutex_t);
            while(true)
            {
                std::array<bool, size> locked = {};
                size_t i = 0U;
                if(std::apply([&](Mutex_t &... apply_args) { return (... && (locked[i++] = apply_args.try_lock(), locked[i - 1])); }, sl))
                {
                    is_locked = true;
                    return;
                }

                i = 0U;
                std::apply([&](Mutex_t &... apply_args) { (... , apply_args.unlock(locked[i++])); }, sl);
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
            std::apply([](Mutex_t &... apply_args) { (... , apply_args.unlock()); }, sl);
            is_locked = false;
        }

        MutexGuard() = delete;
        MutexGuard(const MutexGuard &) = delete;

        ~MutexGuard()
        {
            unlock(false);
        }

    private:
        std::tuple<Mutex_t &...> sl;
        uint64_t cpsr;
        bool is_locked;
};

class Barrier
{
    protected:
        unsigned int nrequired;
        unsigned int ncur = 0;
        std::vector<id_t> waiting_threads;

    public:
        Spinlock sl{};
        id_t id;

        int Wait();

        Barrier(unsigned int _nrequired);
        ~Barrier();
};

#endif
