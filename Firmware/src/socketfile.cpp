#include "osfile.h"
#include <lwip/sockets.h>
#include <errno.h>

SocketFile::SocketFile(int lwfildes, bool _is_non_block)
{
    lwip_fildes = lwfildes;
    is_non_block = _is_non_block;
}

ssize_t SocketFile::Write(const char *buf, size_t count, int *_errno)
{
    auto lwret = lwip_write(lwip_fildes, buf, count);
    if(lwret < 0)
    {
        *_errno = errno;
    }
    return lwret;
}

ssize_t SocketFile::Read(char *buf, size_t count, int *_errno)
{
    auto lwret = lwip_read(lwip_fildes, buf, count);
    if(lwret < 0)
    {
        if(errno == EWOULDBLOCK && is_non_block == false)
        {
            return -3;  // signal need to try again
        }
        *_errno = errno;
    }
    return lwret;
}

int SocketFile::Bind(void *addr, unsigned int addrlen, int *_errno)
{
    auto lwret = lwip_bind(lwip_fildes, (const sockaddr *)addr, addrlen);
    if(lwret < 0)
    {
        *_errno = errno;
    }
    return lwret;
}

int SocketFile::Close(int *_errno)
{
    auto lwret = lwip_close(lwip_fildes);
    if(lwret < 0)
    {
        *_errno = errno;
    }
    return lwret;
}

int SocketFile::Listen(int backlog, int *_errno)
{
    auto lwret = lwip_listen(lwip_fildes, backlog);
    if(lwret < 0)
    {
        *_errno = errno;
    }
    return lwret;
}

int SocketFile::Accept(void *addr, unsigned int *addrlen, int *_errno)
{
    auto lwret = lwip_accept(lwip_fildes, (sockaddr *)addr, (socklen_t *)addrlen);
    if(lwret < 0)
    {
        if(errno == EWOULDBLOCK && is_non_block == false)
        {
            return -3;  //userspace try-again
        }
        *_errno = errno;
    }
    return lwret;
}
