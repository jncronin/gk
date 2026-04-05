#ifndef WAITWOUND_H
#define WAITWOUND_H

#include <cstdint>
#include <kernel_time.h>
#include <ostypes.h>
#include <vector>
#include <osmutex.h>

class WaitWoundContext
{
    protected:
        ticket_t ticket;
        std::vector<id_t> locked_mutexes;
        bool done_acquire = false;

    public:
        WaitWoundContext();
        bool lock(Mutex &m);
        bool lock_slow(Mutex &m);
        void acquire_done();    // purely a debugging tool - set done_acquire then prevent further lock() calls
};

#endif
