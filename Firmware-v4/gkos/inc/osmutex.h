#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <cstdint>
#include "osspinlock.h"
#include "kernel_time.h"
#include <unordered_map>
#include <unordered_set>
#include "threadproclist.h"

class Thread;
using WPThread = std::weak_ptr<Thread>;

class Mutex
{
    protected:
        WPThread owner{};
        std::unordered_set<id_t> waiting_threads;
        bool is_recursive = false;
        bool echeck = false;
        int lockcount = 0;
        Spinlock sl;

    public:
        id_t id;
        Mutex(bool recursive = false, bool error_check = false);
        void lock(bool allow_deadlk = false);
        bool try_lock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool unlock(int *reason = nullptr, bool force = false);
        bool try_delete(int *reason = nullptr);
};

class RwLock
{
    protected:
        WPThread wrowner{};
        std::unordered_set<id_t> rdowners;
        std::unordered_set<id_t> waiting_threads;
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
        std::unordered_set<id_t> waiting_threads;
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
        Spinlock sl;

    public:
        id_t id;
        void Wait(kernel_time tout = kernel_time(), int *signalled_ret = nullptr);
        void Signal(bool all = true);
        ~Condition();
};

class SimpleSignal
{
    protected:
        uint32_t signal_value = 0;
        Spinlock sl;

    public:
        WPThread waiting_thread{};
        enum SignalOperation { Noop, Set, Or, And, Xor, Add, Sub };
        uint32_t Wait(SignalOperation op = Noop, uint32_t val = 0, kernel_time tout = kernel_time());
        uint32_t WaitOnce(SignalOperation op = Noop, uint32_t val = 0, kernel_time tout = kernel_time());
        uint32_t Value();
        void Signal(SignalOperation op = Set, uint32_t val = 0x1);
        SimpleSignal(uint32_t val = 0);

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


#endif
