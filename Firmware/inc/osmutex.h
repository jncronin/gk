#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>
#include <region_allocator.h>

class Thread;
class Spinlock
{
    protected:
        uint32_t _lock_val = 0;

    public:
        void lock();
        void unlock();
        Spinlock();
};

class Mutex
{
    protected:
        Thread *owner;

    public:
        void lock();
        void unlock();
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
