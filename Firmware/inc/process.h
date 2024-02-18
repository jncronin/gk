#ifndef PROCESS_H
#define PROCESS_H

class Thread;

#include "memblk.h"
#include "region_allocator.h"
#include <string>
#include "osmutex.h"
#include "osfile.h"

#define GK_MAX_OPEN_FILES      16

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
};


#endif
