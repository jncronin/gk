#include "syscalls.h"

#include <errno.h>

#include "thread.h"

#define SOCK_NONBLOCK 0x20000000

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
