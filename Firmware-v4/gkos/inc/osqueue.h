#ifndef QUEUE_H
#define QUEUE_H

#include "scheduler.h"
#include "osmutex.h"
#include "thread.h"
#include "ipi.h"
#include <queue>
#include <cstring>
#include <clocks.h>
#include "weak_ptr_unordered_set.h"

#include "gk_conf.h"

class BaseQueue
{
    public:
        BaseQueue(void *buf, int _nitems, size_t item_size) : _b(buf), nitems(_nitems), sz(item_size) {}

    protected:
        void *_b;
        int nitems;
        size_t sz;

        int _wptr = 0;
        int _rptr = 0;
        WeakPtrUnorderedSet<Thread> waiting_threads;
        Spinlock sl;

        constexpr inline int ptr_plus_one(int p)
        {
            p++;
            if(p >= nitems)
                p = 0;
            return p;
        }

        inline void signal_waiting()
        {
            for(auto bt : waiting_threads)
            {
                auto btp = bt.lock();
                if(btp)
                {
                    CriticalGuard cg(btp->sl_blocking);
                    btp->set_is_blocking(false);
                    btp->block_until = kernel_time_invalid();
                    btp->blocking_on_prim = nullptr;
                    btp->blocking_on_thread.reset();
                    signal_thread_woken(btp);
                }
            }
            waiting_threads.clear();
        }

        bool _Push(const void *v)
        {
            if(full())
            {
#if DEBUG_FULLQUEUE
                CriticalGuard cg2(s_rtt);
                SEGGER_RTT_printf(0, "queue: write fail\n");
#endif
                return false;
            }

            memcpy(&(reinterpret_cast<char *>(_b)[_wptr * sz]), v, sz);
            //_b[_wptr] = v;
            _wptr = ptr_plus_one(_wptr);
            signal_waiting();
            return true;
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
            return _Push(v);
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

        bool Pop(void *v, kernel_time timeout = kernel_time())
        {
            if(!v)
                return false;

            if(kernel_time_is_valid(timeout))
                timeout += clock_cur();
            
            while(true)
            {
                {
                    CriticalGuard cg(sl);
                    if(empty())
                    {
                        {
                        UninterruptibleGuard ug;
                            auto t = GetCurrentPThreadForCore();
                            waiting_threads.insert(t);
                            t->set_is_blocking(true);
                            t->blocking_on_prim = this;
                            if(kernel_time_is_valid(timeout))
                                t->block_until = timeout;
                        }
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
                if(kernel_time_is_valid(timeout) && clock_cur() >= timeout)
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

        size_t Push(const T* v, size_t n)
        {
            CriticalGuard cg;
            for(unsigned int i = 0; i < n; i++)
            {
                if(!_Push(&v[i]))
                    return i;
            }
            return n;
        }

    protected:
        T buf[_nitems];
};

class Queue : public BaseQueue
{
    public:
        Queue(int _nitems, size_t item_size) : BaseQueue(nullptr, _nitems, item_size)
        {
            _b = new char[nitems * item_size];
        }

        ~Queue()
        {
            if(_b)
                delete[] reinterpret_cast<char *>(_b);
        }
};

#endif
