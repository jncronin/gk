#include "osfile.h"

int File::Isatty(int *_errno)
{
    *_errno = ENOTTY;
    return 0;
}

int File::Close(int *_errno)
{
    (void)_errno;
    return 0;
}

int File::Close2(int *_errno)
{
    (void)_errno;
    return 0;
}

int File::Bind(void *addr, unsigned int addrlen, int *_errno)
{
    *_errno = EBADF;
    return -1;
}

int File::Fstat(struct stat *buf, int *_errno)
{
    *_errno = EBADF;
    return -1;
}

off_t File::Lseek(off_t offset, int whence, int *_errno)
{
    *_errno = EBADF;
    return (off_t)-1;
}

int File::Listen(int backlog, int *_errno)
{
    *_errno = EBADF;
    return -1;
}

int File::Accept(void *addr, unsigned int *addrlen, int *_errno)
{
    *_errno = EBADF;
    return -1;
}

FileType File::GetType() const
{
    return type;
}
