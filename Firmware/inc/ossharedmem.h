#ifndef OSSHAREDMEM_H
#define OSSHAREDMEM_H

#include <cstdlib>
#include "thread.h"

/* Use this class around any reads/writes to memory that may be immediately accessed by another
    cpu (e.g. client/server message passing between threads running on both cpus)
   It ensures cache coherency between the two cpus
   
   Not required for memory accesses in SRAM4 space which is by default non-cached */
class SharedMemoryGuard
{
    public:
        SharedMemoryGuard(const void *start, size_t len, bool will_read, bool will_write);
        SharedMemoryGuard() = delete;
        SharedMemoryGuard(SharedMemoryGuard &&) = delete;
        SharedMemoryGuard(const SharedMemoryGuard &) = delete;
        ~SharedMemoryGuard();

    protected:
        int old_core_pin, coreid;
        Thread *t;
        uint32_t start;
        uint32_t len;
        bool is_read, is_write;
};


#endif
