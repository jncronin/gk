#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>
#include <region_allocator.h>
#include <unordered_set>
#include <unordered_map>
#include "clocks.h"

#define DEBUG_SPINLOCK      1


class Thread;
class Spinlock
{
    protected:
        volatile uint32_t _lock_val = 0;
#if DEBUG_SPINLOCK
        volatile Thread *locked_by = nullptr;
        volatile int locking_core = 0;
        volatile uint32_t locking_pc;
#endif

    public:
#if DEBUG_SPINLOCK
        void lock(uint32_t _locking_pc);
        bool try_lock(uint32_t _locking_pc);
#else
        void lock();
        bool try_lock();
#endif
        void unlock();
        Spinlock();
};

class Mutex
{
    protected:
        Thread *owner = nullptr;
        Spinlock sl;
        std::unordered_set<Thread *> waiting_threads;
        bool is_recursive = false;
        bool echeck = false;
        int lockcount = 0;

    public:
        Mutex(bool recursive = false, bool error_check = false);

        void lock();
        bool try_lock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool unlock(int *reason = nullptr);
        bool try_delete(int *reason = nullptr);
};

class RwLock
{
    protected:
        Thread *wrowner = nullptr;
        std::unordered_set<Thread *> rdowners;
        std::unordered_set<Thread *> waiting_threads;
        Spinlock sl;

    public:
        bool try_wrlock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool try_rdlock(int *reason = nullptr, bool block = true, kernel_time tout = kernel_time());
        bool try_delete(int *reason = nullptr);
        bool unlock(int *reason = nullptr);
};

class UserspaceSemaphore
{
    protected:
        Spinlock sl;
        unsigned int val;
        std::unordered_set<Thread *> waiting_threads;

    public:
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
        std::unordered_map<Thread *, timeout> waiting_threads;
        Spinlock sl;

    public:
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
        Thread *waiting_thread = nullptr;
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

class UninterruptibleGuard
{
    public:
        UninterruptibleGuard();
        ~UninterruptibleGuard();
        UninterruptibleGuard(const UninterruptibleGuard&) = delete;

    protected:
        uint32_t cpsr;
};

class CriticalGuard
{
    public:
        CriticalGuard(Spinlock &s);
        CriticalGuard(Spinlock &s1, Spinlock &s2);
        CriticalGuard(Spinlock &s1, Spinlock &s2, Spinlock &s3);
        ~CriticalGuard();
        CriticalGuard(const CriticalGuard&) = delete;

    protected:
        uint32_t cpsr;
        Spinlock *_s1, *_s2, *_s3;
};

#endif
