#ifndef LOCKFREEBUFFER_H
#define LOCKFREEBUFFER_H

#include <cstdint>
#include <cstddef>
#include <atomic>

class LockFreeBuffer
{
    protected:
        uint8_t *buf;
        size_t bufsize;

        std::atomic<size_t> cons_head, cons_tail, prod_head, prod_tail;

        // write with wraparound
        void write_int(const uint8_t *d, size_t len, size_t wptr);

        // read with wraparound
        void read_int(uint8_t *d, size_t len, size_t rptr);

    public:
        ssize_t write(const void *d, size_t len);
        ssize_t read(void *d, size_t len);
        void init(uint8_t *b, size_t blen);

        LockFreeBuffer(uint8_t *b, size_t blen);
};

#endif
