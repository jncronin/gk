#include "thread.h"

std::shared_ptr<Thread> Thread::Create(const std::string &name,
            threadstart_t func,
            void *p,
            bool is_priv, int priority,
            std::shared_ptr<Process> owning_process)
{
    auto t = std::make_shared<Thread>();

    t->name = name;
    t->p = owning_process;



    return t;
}
