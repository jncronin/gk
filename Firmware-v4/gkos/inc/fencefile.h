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
class UserFenceManager;

class dma_fence
{
    private:
        SimpleSignal s;

    public:
        uint32_t id = 0;
        bool has_id = false;
        std::weak_ptr<UserFenceManager> ufm;

        void Signal();
        bool IsSignalled() { return s.Value() != 0; }
        bool Wait(kernel_time tout = kernel_time_invalid())
        {
            return s.Wait(SimpleSignal::SignalOperation::Noop, 0, tout);
        }
};

class FenceFile : public File
{
    public:
        FenceFile();

        /* The fence is shared between userspace and kernel - this is the userspace reference */
        std::shared_ptr<dma_fence> fence;
};

#endif
