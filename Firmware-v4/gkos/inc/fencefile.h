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
#include "gk_conf.h"

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
        int event_id = -1;

        void Signal();
        bool IsSignalled() { return s.Value() != 0; }

        bool Wait(kernel_time tout = kernel_time_invalid(),
            kernel_time busy_wait_time = kernel_time_from_us(GK_DMAFENCE_BUSYWAIT_US))
        {
#if GK_DMAFENCE_BUSYWAIT_US
            if(kernel_time_is_valid(busy_wait_time))
            {
                auto busy_wait_until = clock_cur() + busy_wait_time;
                if(kernel_time_is_valid(tout))
                {
                    busy_wait_until = std::min(busy_wait_until, tout);
                }

                while(clock_cur() < busy_wait_until)
                {
                    if(s.Value())
                    {
                        break;
                    }
                    __asm__ volatile("yield\n" ::: "memory");
                }
            }
#endif
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
