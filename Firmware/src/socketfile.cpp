#include "osfile.h"
#include <lwip/sockets.h>
#include <errno.h>

SocketFile::SocketFile(int lwfildes)
{
    lwip_fildes = lwfildes;
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
