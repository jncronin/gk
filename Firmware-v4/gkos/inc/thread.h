#ifndef THREAD_H
#define THREAD_H

#include <memory>

class Process;

class Thread
{
    public:
        std::shared_ptr<Process> p;
        std::string name;

        typedef void *(*threadstart_t)(void *p);
        static std::shared_ptr<Thread> Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process);
};



#endif
