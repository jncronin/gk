#ifndef QUEUE_H
#define QUEUE_H

#include "region_allocator.h"
#include "osmutex.h"
#include "thread.h"
#include <queue>

template <typename T> using SRAM4Queue = std::queue<T, std::deque<T, SRAM4RegionAllocator<T>>>;

template <typename T, int nitems> class FixedQueue
{
    protected:
        T _b[nitems];
        Spinlock sl;
        int _wptr = 0;
        int _rptr = 0;
        SRAM4Vector<Thread *> waiting_threads;

        constexpr inline int ptr_plus_one(int p)
        {
            p++;
            if(p >= nitems)
                p = 0;
            return p;
        }

        inline void signal_waiting()
        {
            auto t = GetCurrentThreadForCore();
            bool hpt = false;
            for(auto bt : waiting_threads)
            {
                bt->is_blocking = false;
                if(bt->base_priority > t->base_priority)
                    hpt = true;
            }
            waiting_threads.clear();
            if(hpt)
            {
                SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
            }
        }

    public:
        inline bool empty()
        {
            return _wptr == _rptr;
        }

        inline bool full()
        {
            return ptr_plus_one(_wptr) == _rptr;
        }

        bool Push(const T& v)
        {
            CriticalGuard cg(sl);
            if(full())
                return false;
            _b[_wptr] = v;
            _wptr = ptr_plus_one(_wptr);
            signal_waiting();
            return true;
        }

        bool Peek(T *v)
        {
            if(!v)
                return false;

            CriticalGuard cg(sl);
            if(empty())
                return false;
            return _b[_rptr];
        }

        bool Pop(T *v)
        {
            if(!v)
                return false;
            
            while(true)
            {
                CriticalGuard cg(sl);
                if(empty())
                {
                    auto t = GetCurrentThreadForCore();
                    waiting_threads.push_back(t);
                    t->is_blocking = true;
                    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
                }
                else
                {
                    *v = _b[_rptr];
                    _rptr = ptr_plus_one(_rptr);
                    return true;
                }
            }
        }
};

#endif
