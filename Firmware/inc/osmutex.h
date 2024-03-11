#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>
#include <region_allocator.h>

#define DEBUG_SPINLOCK      1


class Thread;
class Spinlock
{
    protected:
        volatile uint32_t _lock_val = 0;
#if DEBUG_SPINLOCK
        volatile Thread *locked_by = nullptr;
        int locking_core = 0;
#endif

    public:
        void lock();
        void unlock();
        Spinlock();
};

class Mutex
{
    protected:
        Thread *owner = nullptr;
        Spinlock sl;
        SRAM4Vector<Thread *> waiting_threads;

    public:
        void lock();
        bool try_lock();
        bool unlock();
};

class Condition
{
    protected:
        SRAM4Vector<Thread *> waiting_threads;
        Spinlock sl;

    public:
        void Wait();
        void Signal();
};

class SimpleSignal
{
    protected:
        Thread *waiting_thread = nullptr;
        uint32_t signal_value = 0;
        Spinlock sl;

    public:
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
