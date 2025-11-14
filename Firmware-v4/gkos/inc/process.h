#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>
#include "osspinlock.h"

class Thread;

class Process
{
    public:
        std::string name;
        std::vector<std::shared_ptr<Thread>> threads;
        Spinlock sl;
};

#endif
