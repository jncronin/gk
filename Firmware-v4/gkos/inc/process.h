#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>
#include "osspinlock.h"
#include "osfile.h"
#include "vblock.h"
#include "ostypes.h"
#include "osmutex.h"
#include <unordered_set>
#include <map>
#include "sync_primitive_locks.h"
#include "gk_conf.h"
#include "_gk_event.h"
#include "osqueue.h"
#include "proc_vmem.h"

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
                void release_all();
        };

        class userspace_mem_t
        {
            public:
                Mutex m = Mutex(true);
                uintptr_t ttbr0;
                MapVBlockAllocator vblocks;
        };

        class environ_t
        {
            public:
                Spinlock sl;
                std::vector<std::string> envs;
                std::vector<std::string> args;

                /* Current directory */
                std::string cwd = "";
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

                bool new_clut = false;
                std::vector<uint32_t> clut;
        };

        struct audio_conf_t
        {
            unsigned int nbuffers = 0;
            unsigned int buf_size_bytes;
            unsigned int buf_ndtr;
            unsigned int wr_ptr;
            unsigned int rd_ptr;
            unsigned int rd_ready_ptr;
            uintptr_t silence_paddr;
            Condition waiting_threads;
            bool enabled = false;
            unsigned int freq, nbits, nchan;
            unsigned int audio_max_buffer_size = 0;

            VMemBlock mr_sound = InvalidVMemBlock();
            PMemBlock p_sound = InvalidPMemBlock();
            Spinlock sl_sound;
        };

        std::string name;
        std::vector<id_t> threads;
        id_t id, ppid;
        Spinlock sl;

        bool is_privileged = true;
        std::unique_ptr<userspace_mem_t> user_mem;

        open_files_t open_files{};
        owned_pages_t owned_pages{};
        environ_t env{};
        heap_t heap{};
        screen_t screen{};
        audio_conf_t audio{};

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

        /* Events */
        FixedQueue<Event, GK_NUM_EVENTS_PER_PROCESS> events;
        uint32_t mouse_buttons = 0;
        uint64_t gamepad_buttons = 0;
        prockeymap_t keymap;
        int HandleInputEvent(uint32_t cmd);

        /* create a process */
        static PProcess Create(const std::string &name, bool is_privileged = false,
            PProcess parent = nullptr);

        /* Kill a process */
        static void Kill(id_t pid, int retval = 0);

        /* Threads waiting on us to end */
        std::unordered_set<id_t> waiting_threads;

        Process() = default;
        ~Process();
};

extern PProcess p_kernel;

PProcess GetFocusProcess();
id_t GetFocusPid();
int SetFocusProcess(PProcess p);

#endif
