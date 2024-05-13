#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>
#include <region_allocator.h>
#include <unordered_set>
#include <unordered_map>

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
#else
        void lock();
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
        int lockcount = 0;

    public:
        Mutex(bool recursive = false);

        void lock();
        bool try_lock();
        bool unlock();
        bool try_delete();
};

class Condition
{
    protected:
        struct timeout { uint64_t tout; int *signalled; };
        std::unordered_map<Thread *, timeout> waiting_threads;
        Spinlock sl;

    public:
        void Wait(uint64_t tout = UINT64_MAX, int *signalled_ret = nullptr);
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
        uint32_t Wait(SignalOperation op = Noop, uint32_t val = 0, uint64_t tout = UINT64_MAX);
        uint32_t WaitOnce(SignalOperation op = Noop, uint32_t val = 0, uint64_t tout = UINT64_MAX);
        void Signal(SignalOperation op = Set, uint32_t val = 0x1);
        void Reset();
        SimpleSignal(uint32_t val = 0);

    protected:
        void do_op(SignalOperation op, uint32_t vop);
};

class BinarySemaphore
{
    protected:
        SimpleSignal ss;

    public:
        bool Wait(uint64_t tout = UINT64_MAX);
        bool WaitOnce(uint64_t tout = UINT64_MAX);
        void Signal();
};

class CountingSemaphore
{
    protected:
        SimpleSignal ss;

    public:
        bool Wait(uint64_t tout = UINT64_MAX);
        bool WaitOnce(uint64_t tout = UINT64_MAX);
        void Signal();
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
        ~CriticalGuard();
        CriticalGuard(const CriticalGuard&) = delete;

    protected:
        uint32_t cpsr;
        Spinlock &_s;
};

#endif
