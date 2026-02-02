#include "lockfreebuffer.h"
#include <algorithm>
#include <cstring>

// see https://doc.dpdk.org/guides/prog_guide/ring_lib.html

#define MASK (bufsize - 1)

void LockFreeBuffer::init(uint8_t *b, size_t blen)
{
    buf = b;
    bufsize = blen;
}

LockFreeBuffer::LockFreeBuffer(uint8_t *b, size_t blen)
{
    init(b, blen);
}

ssize_t LockFreeBuffer::write(const void *d, size_t len)
{
    auto l_prod_head = prod_head.load();

    while(true)
    {
        auto l_cons_tail = cons_tail.load();

        auto sz_avail = (l_prod_head == l_cons_tail) ? bufsize : 
            ((l_cons_tail - l_prod_head) & MASK);

        auto act_len = std::min(len, sz_avail);
        if(act_len == 0)
        {
            return -1;
        }

        auto l_prod_next = (l_prod_head + act_len) & MASK;

        // try and update prod_head
        auto prod_head_updated = prod_head.compare_exchange_weak(l_prod_head, l_prod_next,
            std::memory_order_release, std::memory_order_relaxed);

        if(!prod_head_updated)
            continue;   // retry
        
        // write the data to l_prod_head
        write_int((const uint8_t *)d, act_len, l_prod_head);

        // keep spinning until we update prod_tail
        while(!prod_tail.compare_exchange_weak(l_prod_head, l_prod_next,
            std::memory_order_release, std::memory_order_relaxed))
        {
            __asm__ volatile("wfe \n" ::: "memory");
        }
        __asm__ volatile("sev \n" ::: "memory");

        return (ssize_t)act_len;
    }
}

ssize_t LockFreeBuffer::read(void *d, size_t len)
{
    auto l_cons_head = cons_head.load();

    while(true)
    {
        auto l_prod_tail = prod_tail.load();

        auto sz_avail = (l_prod_tail - l_cons_head) & MASK;

        auto act_len = std::min(len, sz_avail);

        if(act_len == 0)
        {
            return -1;
        }

        auto l_cons_next = (l_cons_head + act_len) & MASK;

        // try and update cons_head
        auto cons_head_updated = cons_head.compare_exchange_weak(l_cons_head, l_cons_next,
            std::memory_order_release, std::memory_order_relaxed);

        if(!cons_head_updated)
            continue;   // retry

        // read data
        read_int((uint8_t *)d, act_len, l_cons_head);

        // keep spinning until we update cons_tail
        while(!cons_tail.compare_exchange_weak(l_cons_head, l_cons_next,
            std::memory_order_release, std::memory_order_relaxed))
        {
            __asm__ volatile("wfe \n" ::: "memory");
        }
        __asm__ volatile("sev \n" ::: "memory");

        return (ssize_t)act_len;
    }
}

void LockFreeBuffer::write_int(const uint8_t *d, size_t len, size_t wptr)
{
    auto first_part_len = bufsize - wptr;
    auto act_first_part = std::min(first_part_len, len);
    memcpy(&buf[wptr], d, act_first_part);
    
    len -= act_first_part;
    if(len)
        memcpy(&buf[0], &d[act_first_part], len);
}

void LockFreeBuffer::read_int(uint8_t *d, size_t len, size_t rptr)
{
    auto first_part_len = bufsize - rptr;
    auto act_first_part = std::min(first_part_len, len);
    memcpy(d, &buf[rptr], act_first_part);

    len -= act_first_part;
    if(len)
        memcpy(&d[act_first_part], &buf[0], len);
}
