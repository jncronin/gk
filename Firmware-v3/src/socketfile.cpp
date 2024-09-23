#include "osfile.h"
#include <errno.h>
#include "osnet.h"

SocketFile::SocketFile(Socket *_sck)
{
    sck = _sck;
    type = FileType::FT_Socket;
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
    // call Close on socket, actual deletion of socket happens later after fin-finack-ack sequence
    return sck->CloseAsync(_errno);
}

int SocketFile::Close2(int *_errno)
{
    // delete underlying socket structure
    if(sck)
    {
        delete sck;
        sck = nullptr;
    }
    return 0;
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
