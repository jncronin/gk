#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>

class Thread;

class Process
{
    public:
        std::string name;
        std::vector<std::shared_ptr<Thread>> threads;
};

#endif
