#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>
#include "osspinlock.h"
#include "osfile.h"
#include "vblock.h"
#include "ostypes.h"
#include <unordered_set>

class Thread;
class Process;

using PProcess = std::shared_ptr<Process>;
using WPProcess = std::weak_ptr<Process>;

class Process
{
    public:
        class open_files_t
        {
            public:
                Spinlock sl;

                std::vector<PFile> f{};

                int get_free_fildes(int start_fd = 0);
                int get_fixed_fildes(int fd);
        };

        class owned_pages_t
        {
            public:
                Spinlock sl;
                std::unordered_set<uintptr_t> p{};

                void add(const PMemBlock &b);
        };

        class userspace_mem_t
        {
            public:
                Spinlock sl;
                uintptr_t ttbr0;
                VBlock blocks;
        };

        class environ_t
        {
            public:
                Spinlock sl;
                std::vector<std::string> envs;
        };

        std::string name;
        std::vector<std::shared_ptr<Thread>> threads;
        Spinlock sl;

        bool is_privileged = true;
        std::unique_ptr<userspace_mem_t> user_mem;

        open_files_t open_files{};
        owned_pages_t owned_pages{};
        environ_t env{};

        std::string cwd = "";
        Process(const std::string &name, bool is_privileged = false,
            PProcess parent = nullptr);
};

extern PProcess p_kernel;

#endif
