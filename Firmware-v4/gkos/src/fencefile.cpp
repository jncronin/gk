#include "fencefile.h"
#include "process.h"

std::shared_ptr<dma_fence> sync_file_get_fence(int fd)
{
    auto p = GetCurrentProcessForCore();
    if(!p)
        return nullptr;
    
    CriticalGuard cg(p->open_files.sl);
    if(fd < 0 || (size_t)fd >= p->open_files.f.size())
        return nullptr;
    auto ptr = p->open_files.f[fd].get();

    if(!ptr)
        return nullptr;

    if(ptr->GetType() != FileType::FT_Fence)
    {
        klog("sync_file_get_fence: fd %d is not a fence\n");
        return nullptr;
    }

    auto fptr = reinterpret_cast<FenceFile *>(ptr);
    return fptr->fence;
}
