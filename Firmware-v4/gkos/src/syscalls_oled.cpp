#include "process.h"
#include "syscalls_int.h"
#include "screen.h"
#include "_gk_memaddrs.h"

bool is_parent_of(id_t child, id_t parent);

int syscall_setoled(int pid, int scr_id,
    int fd, unsigned int w, unsigned int h,
    unsigned int pf, unsigned int stride, int *_errno)
{
    PProcess p;
    Process *curp = GetCurrentProcessForCore();

    /* First get the process - we allow this syscall to refer to child
        processes as well, so that gkmenu can set images for children */
    if(pid <= 0)
    {
        p = GetCurrentPProcessForCore();
    }
    else
    {
        if(!is_parent_of(pid, curp->id))
        {
            *_errno = EACCES;
            return -1;
        }
        p = ProcessList.Get(pid);
    }
    if(!p)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(scr_id < 0 || scr_id >= GK_OLED_COUNT)
    {
        // invalid screen
        *_errno = EINVAL;
        return -1;
    }

    if(pf >= GK_PIXELFORMAT_MAX)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(screen_get_bpp_for_pf(pf) == 0)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(screen_get_bpp_for_pf(pf) * w > stride)
    {
        *_errno = EINVAL;
        return -1;
    }

    CriticalGuard cg(p->oleds[scr_id].sl, curp->open_files.sl);
    if(fd < 0)
    {
        p->oleds[scr_id].file = nullptr;
    }
    else
    {
        if((size_t)fd >= curp->open_files.f.size())
        {
            *_errno = EBADF;
            return -1;
        }

        if(curp->open_files.f[fd] == nullptr || curp->open_files.f[fd]->GetType() != FileType::FT_DMABuf)
        {
            *_errno = EBADF;
            return -1;
        }

        if(curp->open_files.f[fd]->Flen(_errno) < (h * stride))
        {
            *_errno = EINVAL;
            return -1;
        }

        p->oleds[scr_id].file = curp->open_files.f[fd];
        p->oleds[scr_id].w = w;
        p->oleds[scr_id].h = h;
        p->oleds[scr_id].pf = pf;
        p->oleds[scr_id].stride = stride;
    }

    cg.unlock();

    if(p == GetFocusProcess())
    {
        p->ShowOled(scr_id);
    }

    return 0;
}
