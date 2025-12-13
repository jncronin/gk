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
#include <map>
#include "sync_primitive_locks.h"
#include "gk_conf.h"
#include "_gk_event.h"
#include "osqueue.h"

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
                std::unordered_set<uint32_t> p{};

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
                std::vector<std::string> args;
        };

        class pthread_tls_t
        {
            public:
                Spinlock sl;
                pthread_key_t next_key = 0;
                std::map<pthread_key_t, void (*)(void *)> tls_data;
        };

        class heap_t
        {
            public:
                Spinlock sl;
                // allow for lazy-initialized heap because process could conceivably mmap everything
                VMemBlock vb_heap = InvalidVMemBlock();
                uintptr_t brk = 0;
        };

        class screen_t
        {
            public:
                Spinlock sl;
                uint16_t screen_w = GK_SCREEN_WIDTH;
                uint16_t screen_h = GK_SCREEN_HEIGHT;
                uint8_t screen_pf = 0;
                unsigned int screen_refresh = 60;

                unsigned int screen_layer = 0;
        };

        std::string name;
        std::vector<std::shared_ptr<Thread>> threads;
        id_t id, ppid;
        Spinlock sl;

        bool is_privileged = true;
        std::unique_ptr<userspace_mem_t> user_mem;

        open_files_t open_files{};
        owned_pages_t owned_pages{};
        environ_t env{};
        heap_t heap{};
        screen_t screen{};

        /* Return value */
        int rc = 0;

        /* Owned userspace sync primitives */
        owned_sync_list<Mutex> owned_mutexes = owned_sync_list(MutexList);
        owned_sync_list<Condition> owned_conditions = owned_sync_list(ConditionList);
        owned_sync_list<RwLock> owned_rwlocks = owned_sync_list(RwLockList);
        owned_sync_list<UserspaceSemaphore> owned_semaphores = owned_sync_list(UserspaceSemaphoreList);

        /* pthread TLS data */
        pthread_tls_t pthread_tls{};

        /* elf TLS template */
        VMemBlock vb_tls = InvalidVMemBlock();
        size_t vb_tls_data_size;        // actual size of TLS data to copy

        /* Current directory */
        std::string cwd = "";

        /* Events */
        FixedQueue<Event, GK_NUM_EVENTS_PER_PROCESS> events;

        /* create a process */
        static PProcess Create(const std::string &name, bool is_privileged = false,
            PProcess parent = nullptr);

        /* Kill this process */
        void Kill();

        Process() = default;
};

extern PProcess p_kernel;

#endif
