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

class unknown_queue_type { };

template <typename T> class BaseQueue
{
    public:
        BaseQueue(T *buf, int _nitems, size_t item_size) : _b(buf), nitems(_nitems), sz(item_size) {}

    protected:
        T *_b;
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

        bool _Push(const T &v)
        {
            if(full())
            {
#if DEBUG_FULLQUEUE
                CriticalGuard cg2(s_rtt);
                SEGGER_RTT_printf(0, "queue: write fail\n");
#endif
                return false;
            }


            if constexpr(std::is_same_v<T, unknown_queue_type>)
            {
                auto b = (char *)_b;
                memcpy(&b[_wptr * sz], (const void *)&v, sz);
            }
            else
            {
                _b[_wptr] = v;
            }

            //_b[_wptr] = v;
            _wptr = ptr_plus_one(_wptr);
            signal_waiting();
            return true;
        }

        bool _Push(T &&v)
        {
            static_assert(std::is_same_v<T, unknown_queue_type> == false);

            if(full())
            {
#if DEBUG_FULLQUEUE
                CriticalGuard cg2(s_rtt);
                SEGGER_RTT_printf(0, "queue: write fail\n");
#endif
                return false;
            }

            _b[_wptr] = std::move(v);

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
            static_assert(std::is_same_v<T, unknown_queue_type>);
            CriticalGuard cg(sl);
            return _Push((const T &)v);
        }

        bool Push(const T &v)
        {
            CriticalGuard cg(sl);
            return _Push(v);
        }

        bool Push(T &&v)
        {
            CriticalGuard cg(sl);
            return _Push(std::move(v));
        }

        bool Peek(T *v)
        {
            if(!v)
                return false;

            CriticalGuard cg(sl);
            if(empty())
                return false;

            *v = _b[_rptr];
            return true;
        }

        bool TryPop(T *v)
        {
            if(!v)
                return false;
            
            CriticalGuard cg(sl);
            if(empty())
            {
                return false;
            }
            else
            {
                if constexpr(std::is_same_v<T, unknown_queue_type>)
                {
                    auto b = (char *)_b;
                    memcpy(v, &b[_rptr * sz], sz);
                }
                else
                {
                    *v = _b[_rptr];
                }
                _rptr = ptr_plus_one(_rptr);
                return true;
            }
        }

        bool Pop(void *v, kernel_time timeout = kernel_time())
        {
            static_assert(std::is_same_v<T, unknown_queue_type>);
            return Pop((T *)v, timeout);
        }

        bool Pop(T *v, kernel_time timeout = kernel_time())
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
                        if constexpr(std::is_same_v<T, unknown_queue_type>)
                        {
                            auto b = (char *)_b;
                            memcpy((void *)v, &b[_rptr * sz], sz);
                        }
                        else
                        {
                            *v = _b[_rptr];
                        }
                        _rptr = ptr_plus_one(_rptr);
                        return true;
                    }
                }
                __asm__ volatile("dmb sy\n" ::: "memory");
                if(kernel_time_is_valid(timeout) && clock_cur() >= timeout)
                {
                    return false;
                }
            }
        }

};

template <typename T, int _nitems> class FixedQueue : public BaseQueue<T>
{
    public:
        FixedQueue() : BaseQueue<T>(buf, _nitems, sizeof(T)) {}

        using BaseQueue<T>::Push;

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

template <typename T> class Queue : public BaseQueue<T>
{
    public:
        Queue(int _nitems, size_t item_size = sizeof(T)) : BaseQueue<T>(nullptr, _nitems, item_size)
        {
            this->_b = (T*)new char[_nitems * item_size];
        }

        ~Queue()
        {
            if(this->_b)
                delete[] reinterpret_cast<char *>(this->_b);
        }
};

using CStyleQueue = Queue<unknown_queue_type>;

#endif
