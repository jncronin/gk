#include "syscalls.h"

#include <lwip/sockets.h>
#include <errno.h>

#include "thread.h"

int syscall_socket(int domain, int type, int protocol, int *_errno)
{
    // get a lwip sockid
    auto lwret = lwip_socket(domain, type, protocol);
    if(lwret < 0)
    {
        *_errno = errno;
        return lwret;
    }

    // now convert this to a gk fildes
    // try and get free process file handle
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    CriticalGuard cg(p.sl);
    int fd = -1;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
    {
        if(p.open_files[i] == nullptr)
        {
            fd = i;
            break;
        }
    }
    if(fd == -1)
    {
        *_errno = EMFILE;
        lwip_close(lwret);
        return -1;
    }

    p.open_files[fd] = new SocketFile(lwret);

    return fd;
}

int syscall_bind(int sockfd, void *addr, unsigned int addrlen, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto p = t->p;
    if(sockfd < 0 || sockfd >= GK_MAX_OPEN_FILES || p.open_files[sockfd] == nullptr)
    {
        *_errno = EBADF;
        return -1;
    }
    return p.open_files[sockfd]->Bind(addr, addrlen, _errno);
}
