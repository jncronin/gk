#include "linux_types.h"
#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"

/* Write access needs all fences to signal (all reads and the exclusive write one)
    Read access only needs excludive reads to signal */
bool dma_resv::IsSignalled(bool for_write)
{
    for(auto &f : write_fences)
    {
        if(!f->IsSignalled())
            return false;
    }

    if(!for_write)
        return true;

    for(auto &f : read_fences)
    {
        if(!f->IsSignalled())
            return false;
    }

    return true;
}

bool dma_resv::Wait(bool for_write, kernel_time tout)
{
    for(auto &f : write_fences)
    {
        if(!f->Wait(tout))
            return false;
    }

    if(!for_write)
        return true;
    
    for(auto &f : read_fences)
    {
        if(!f->Wait(tout))
            return false;
    }

    return true;
}
