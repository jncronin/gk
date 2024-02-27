#include "syscalls.h"

#include <lwip/sockets.h>
#include <errno.h>

#include "thread.h"

#define SOCK_NONBLOCK 0x20000000

int syscall_socket(int domain, int type, int protocol, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
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

int syscall_listen(int sockfd, int backlog, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto p = t->p;
    if(sockfd < 0 || sockfd >= GK_MAX_OPEN_FILES || p.open_files[sockfd] == nullptr)
    {
        *_errno = EBADF;
        return -1;
    }
    return p.open_files[sockfd]->Listen(backlog, _errno);
}

int syscall_accept(int sockfd, void *addr, unsigned int *addrlen, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto p = t->p;
    if(sockfd < 0 || sockfd >= GK_MAX_OPEN_FILES || p.open_files[sockfd] == nullptr)
    {
        *_errno = EBADF;
        return -1;
    }
    return p.open_files[sockfd]->Accept(addr, addrlen, _errno);
}
