#ifndef QUEUE_H
#define QUEUE_H

#include "region_allocator.h"
#include "scheduler.h"
#include "osmutex.h"
#include "thread.h"
#include <queue>
#include <cstring>
#include <clocks.h>
#include <unordered_set>

template <typename T> using SRAM4Queue = std::queue<T, std::deque<T, SRAM4RegionAllocator<T>>>;

class BaseQueue
{
    public:
        BaseQueue(void *buf, int _nitems, size_t item_size) : _b(buf), nitems(_nitems), sz(item_size) {}

    protected:
        void *_b;
        int nitems;
        size_t sz;

        Spinlock sl;
        int _wptr = 0;
        int _rptr = 0;
        std::unordered_set<Thread *> waiting_threads;

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
                bt->block_until = 0;
                if(bt->base_priority > t->base_priority)
                    hpt = true;
            }
            waiting_threads.clear();
            if(hpt)
            {
                Yield();
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

        bool Push(const void *v)
        {
            CriticalGuard cg(sl);
            if(full())
                return false;

            memcpy(&(reinterpret_cast<char *>(_b)[_wptr * sz]), v, sz);
            //_b[_wptr] = v;
            _wptr = ptr_plus_one(_wptr);
            signal_waiting();
            return true;
        }

        bool Peek(void *v)
        {
            if(!v)
                return false;

            CriticalGuard cg(sl);
            if(empty())
                return false;

            memcpy(v, &(reinterpret_cast<char *>(_b)[_rptr * sz]), sz);
            return true;
        }

        bool TryPop(void *v)
        {
            CriticalGuard cg(sl);
            if(empty())
            {
                return false;
            }
            else
            {
                memcpy(v, &(reinterpret_cast<char *>(_b)[_rptr * sz]), sz);
                _rptr = ptr_plus_one(_rptr);
                return true;
            }
        }

        bool Pop(void *v, uint64_t timeout = 0)
        {
            if(!v)
                return false;

            if(timeout)
                timeout += clock_cur_ms();
            
            while(true)
            {
                {
                    CriticalGuard cg(sl);
                    if(empty())
                    {
                        auto t = GetCurrentThreadForCore();
                        waiting_threads.insert(t);
                        t->is_blocking = true;
                        if(timeout)
                            t->block_until = timeout;
                        Yield();
                    }
                    else
                    {
                        memcpy(v, &(reinterpret_cast<char *>(_b)[_rptr * sz]), sz);
                        _rptr = ptr_plus_one(_rptr);
                        return true;
                    }
                }
                __DMB();
                if(timeout && clock_cur_ms() >= timeout)
                {
                    return false;
                }
            }
        }

};

template <typename T, int _nitems> class FixedQueue : public BaseQueue
{
    public:
        FixedQueue() : BaseQueue(buf, _nitems, sizeof(T)) {}

        bool Push(const T& v)
        {
            return BaseQueue::Push(&v);
        }

    protected:
        T buf[_nitems];
};

class Queue : public BaseQueue
{
    public:
        Queue(int _nitems, size_t item_size) : BaseQueue(nullptr, _nitems, item_size)
        {
            _b = malloc_region(_nitems * item_size, REG_ID_SRAM4);
        }

        ~Queue()
        {
            if(_b)
                free_region(_b, REG_ID_SRAM4);
        }
};

#endif
