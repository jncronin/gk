#ifndef PROCESS_H
#define PROCESS_H

class Thread;

#include "memblk.h"
#include "region_allocator.h"
#include <string>
#include <map>
#include "osmutex.h"
#include "osqueue.h"
#include "osfile.h"
#include "_gk_event.h"
#include "_gk_proccreate.h"
#include "gk_conf.h"
#include <sys/types.h>
#include <unordered_set>

class GKOS_FUNC(Process)
{
    public:
        GKOS_FUNC(Process)();
        ~GKOS_FUNC(Process)();

        GKOS_FUNC(Spinlock) sl;
        
        std::string name;
        std::string window_title;
        std::vector<GKOS_FUNC(Thread) *> threads;

        MemRegion heap;
        MemRegion code_data;
        bool heap_is_exec = false;
        bool is_priv = true;
        uint32_t thread_finalizer = 0;

        uint32_t brk = 0;

        int rc;
        bool for_deletion = false;

        GKOS_FUNC(File) *open_files[GK_MAX_OPEN_FILES];

        CPUAffinity default_affinity;

        /* pthread TLS data */
        pthread_key_t next_key = 0;
        std::map<pthread_key_t, void (*)(void *)> tls_data;

        /* mmap regions */
        struct mmap_region { MemRegion mr; int fd; int is_read; int is_write; int is_exec; bool is_sync; };
        std::map<uint32_t, mmap_region> mmap_regions;
        std::map<uint32_t, mmap_region>::iterator get_mmap_region(uint32_t addr, uint32_t len);

        /* current working directory */
        std::string cwd = "/";

        /* parameters passed to program */
        int argc;
        char **argv;

        /* TLS segment, if any */
        bool has_tls = false;
        size_t tls_base = 0;
        size_t tls_filsz = 0;
        size_t tls_memsz = 0;

        /* PID support */
        pid_t pid;
        pid_t ppid;     // parent pid
        std::unordered_set<pid_t> child_processes;

        /* primitives owned by this process */
        std::unordered_set<GKOS_FUNC(Mutex) *> owned_mutexes;
        std::unordered_set<GKOS_FUNC(Condition) *> owned_conditions;
        std::unordered_set<GKOS_FUNC(RwLock) *> owned_rwlocks;
        std::unordered_set<GKOS_FUNC(UserspaceSemaphore) *> owned_semaphores;

        /* noncore data for process */
        void *noncore_data;
};

extern GKOS_FUNC(Process) kernel_proc;

/* support for atomically providing a new pid and associating with a process */
class GKOS_FUNC(ProcessList)
{
    public:
        pid_t RegisterProcess(GKOS_FUNC(Process) *p);
        void DeleteProcess(pid_t pid, int retval);
        int GetReturnValue(pid_t pid, int *retval, bool wait = false);
        GKOS_FUNC(Process) *GetProcess(pid_t pid);

    private:
        struct pval
        {
            GKOS_FUNC(Process) *p;
            bool is_alive;
            int retval;
            std::unordered_set<Thread *> waiting_threads;
        };

        std::vector<pval> pvals;

        GKOS_FUNC(Spinlock) sl;
};

extern GKOS_FUNC(ProcessList) proc_list;

#endif
