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
#include "osevent.h"
#include "_gk_proccreate.h"
#include "gk_conf.h"

class Process
{
    public:
        Spinlock sl;
        
        SRAM4String name;
        SRAM4Vector<Thread *> threads;

        MemRegion heap;
        MemRegion code_data;

        uint32_t brk = 0;

        int rc;
        bool for_deletion = false;

        File *open_files[GK_MAX_OPEN_FILES];

        CPUAffinity default_affinity;

        /* pthread TLS data */
        pthread_key_t next_key = 0;
        std::map<pthread_key_t, void (*)(void *)> tls_data;

        /* mmap regions */
        struct mmap_region { MemRegion mr; int fd; int is_read; int is_write; int is_exec; };
        std::map<uint32_t, mmap_region> mmap_regions;
        std::map<uint32_t, mmap_region>::iterator get_mmap_region(uint32_t addr, uint32_t len);

        /* display modes */
        uint16_t screen_w = 640;
        uint16_t screen_h = 480;
        uint8_t screen_pf = 0;

        /* current working directory */
        std::string cwd = "/";

        /* parameters passed to program */
        MemRegion mr_params;
        int argc;
        char **argv;

        /* Events */
        FixedQueue<Event, GK_NUM_EVENTS_PER_PROCESS> events;
};

extern Process *focus_process;
extern Process kernel_proc;

#endif
