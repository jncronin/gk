#ifndef WAITWOUND_H
#define WAITWOUND_H

#include <cstdint>
#include <kernel_time.h>
#include <ostypes.h>
#include <vector>
#include <list>
#include <memory>
#include <osmutex.h>

class WaitWoundContext
{
    protected:
        ticket_t ticket;
        std::list<std::shared_ptr<Mutex>> locked_mutexes;
        bool done_acquire = false;
        Spinlock sl;

    public:
        WaitWoundContext();
        int lock(std::shared_ptr<Mutex> &m);
        int lock_slow(std::shared_ptr<Mutex> &m);
        void acquire_done();    // purely a debugging tool - set done_acquire then prevent further lock() calls
        ~WaitWoundContext();
};

#endif
