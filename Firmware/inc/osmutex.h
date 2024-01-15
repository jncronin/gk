#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>

class Spinlock
{
    protected:
        uint32_t _lock_val = 0;

    public:
        void lock();
        void unlock();
};

class Mutex
{
    public:
        void lock();
        void unlock();
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
