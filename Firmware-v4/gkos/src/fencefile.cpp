#include "fencefile.h"
#include "process.h"
#include "etnaviv/src/user_fences.h"

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

FenceFile::FenceFile()
{
    type = FileType::FT_Fence;
}

int fence_open(PFile *f, std::shared_ptr<dma_fence> fence)
{
    auto ff = std::make_shared<FenceFile>();
    if(fence)
        ff->fence = fence;
    *f = std::move(ff);
    return 0;
}

void dma_fence::Signal()
{
    if(has_id)
    {
        auto fm = ufm.lock();
        if(fm)
            fm->Deregister(id);
    }

    s.Signal();
}
