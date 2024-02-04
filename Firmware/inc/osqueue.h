#ifndef QUEUE_H
#define QUEUE_H

#include "region_allocator.h"
#include "osmutex.h"
#include <queue>

template <typename T> using SRAM4Queue = std::queue<T, std::deque<T, SRAM4RegionAllocator<T>>>;

template <typename T, int nitems> class FixedQueue
{
    protected:
        T _b[nitems];
        Spinlock sl;
        int _n;
        Condition waiting;

    public:
        bool Push(const T& v)
        {
            CriticalGuard cg(sl);
            if(_n >= nitems)
                return false;
            _b[_n++] = v;
            waiting.Signal();
            return true;
        }

        bool Peek(T *v)
        {
            CriticalGuard cg(sl);
            if(_n)
            {
                *v = _b[_n - 1];
                return true;
            }
            return false;
        }

        bool Pop(T *v)
        {
            {
                CriticalGuard cg(sl);
                if(_n)
                {
                    *v = _b[--_n];
                    return true;
                }

                waiting.Wait();
            }
            while(!_n) {}

            {
                CriticalGuard cg(sl);
                if(_n)
                {
                    *v = _b[--_n];
                    return true;
                }
            }
            return false;
        }
};

#endif
