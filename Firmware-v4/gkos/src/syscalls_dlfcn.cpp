#include "syscalls_int.h"
#include "process.h"

int syscall_getndl(int *_errno)
{
    auto p = GetCurrentProcessForCore();

    if(!p)
    {
        *_errno = ENOSYS;
        return -1;
    }

    CriticalGuard cg(p->imgs.sl);
    return (int)p->imgs.imgs.size();
}

int syscall_getdl(int dl_id, int *fd, char *name, size_t *namelen, void **img, void **baseaddr, int *_errno)
{
    if(!fd || !name || !namelen || !img || !baseaddr)
    {
        *_errno = EINVAL;
        return -1;
    }

    ADDR_CHECK_STRUCT_W(fd);
    ADDR_CHECK_STRUCT_W(namelen);
    ADDR_CHECK_BUFFER_W(name, *namelen);
    ADDR_CHECK_STRUCT_W(img);
    ADDR_CHECK_STRUCT_W(baseaddr);

    auto p = GetCurrentProcessForCore();

    if(!p)
    {
        *_errno = ENOSYS;
        return -1;
    }

    CriticalGuard cg(p->imgs.sl);
    if(dl_id < 0 || ((size_t)dl_id >= p->imgs.imgs.size()))
    {
        *namelen = 0;
        *_errno = EINVAL;
        return -1;
    }

    const auto &dl = p->imgs.imgs[(size_t)dl_id];
    *fd = dl.fd;
    *img = dl.img;
    *baseaddr = dl.baseaddr;

    if(dl.path.length() > (*namelen - 1))
    {
        *namelen = dl.path.length() + 1;
        *_errno = EAGAIN;
        return -1;
    }

    *namelen = dl.path.length() + 1;
    strcpy(name, dl.path.c_str());

    return 0;
}
