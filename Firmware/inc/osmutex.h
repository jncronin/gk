#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>
#include <region_allocator.h>

class Thread;
class Spinlock
{
    protected:
        volatile uint32_t _lock_val = 0;

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
        uint32_t Wait();
        uint32_t WaitOnce();
        void Signal(uint32_t val = 0x1);
        void Reset();
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
