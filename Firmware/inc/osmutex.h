#ifndef OSMUTEX_H
#define OSMUTEX_H

#include <memory>
#include <util.h>

class Mutex
{
    public:
        void lock();
        void unlock();
};

#endif
