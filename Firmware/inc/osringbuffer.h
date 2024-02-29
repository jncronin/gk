#ifndef OSRINGBUFFER_H
#define OSRINGBUFFER_H

#include <stddef.h>
#include <cstring>

size_t memcpy_split_src(void *dst, const void *src, size_t n, size_t src_ptr, size_t split_ptr);
size_t memcpy_split_dest(void *dst, const void *src, size_t n, size_t dest_ptr, size_t split_ptr);

template<typename T, unsigned int nitems> class RingBuffer
{
    protected:
        size_t _wptr = 0;
        size_t _rptr = 0;
        T b[nitems];
        constexpr inline unsigned int ptr_plus_one(unsigned int p)
        {
            p++;
            if(p >= nitems)
                p = 0;
            return p;
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

        int Write(const T* d, int n = 1)
        {
            int nwritten = 0;
            for(int i = 0; i < n; i++, nwritten++)
            {
                if(full())
                {
                    return nwritten;
                }
                b[_wptr] = d[i];
                _wptr = ptr_plus_one(_wptr);
            }
            return nwritten;
        }
        int Write(const T &d)
        {
            return Write(&d);
        }

        int Read(T* d, int n = 1)
        {
            int nread = 0;
            for(int i = 0; i < n; i++, nread++)
            {
                if(empty())
                {
                    return nread;
                }
                d[i] = b[_rptr];
                _rptr = ptr_plus_one(_rptr);
            }
            return nread;
        }
};

#endif
