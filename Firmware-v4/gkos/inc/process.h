#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>
#include "osspinlock.h"
#include "osfile.h"

class Thread;

class Process
{
    public:
        class open_files_t
        {
            public:
                Spinlock sl;

                std::vector<PFile> f{};

                int get_free_fildes(int start_fd = 0);
        };

        std::string name;
        std::vector<std::shared_ptr<Thread>> threads;
        Spinlock sl;

        open_files_t open_files{};

        std::string cwd = "";
};

#endif
