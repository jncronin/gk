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
    *_errno = ENOTSUP;
    return -1;
}

ssize_t SocketFile::Read(char *buf, size_t count, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int SocketFile::Bind(void *addr, unsigned int addrlen, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int SocketFile::Close(int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int SocketFile::Listen(int backlog, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int SocketFile::Accept(void *addr, unsigned int *addrlen, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}
