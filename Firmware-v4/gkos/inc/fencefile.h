#ifndef FENCEFILE_H
#define FENCEFILE_H

#include <unistd.h>
//#include <ext4.h>
#include <string>
#include <memory>
//#include "ff.h"
#include "_sys_dirent.h"
#include <vector>
#include "osmutex.h"
#include "linux_types.h"
#include "osfile.h"

int fence_open(PFile *f, std::shared_ptr<dma_fence> fence = nullptr);

struct dma_fence
{
    BinarySemaphore s;
};

class FenceFile : public File
{
    public:
        /* The fence is shared between userspace and kernel - this is the userspace reference */
        std::shared_ptr<dma_fence> fence;
};

#endif
