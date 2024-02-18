#include "syscalls_int.h"
#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "thread.h"

#include <cstring>
#include <fcntl.h>
#include <ext4.h>

extern Spinlock s_rtt;

int syscall_fstat(int file, struct stat *st, int *_errno)
{
    auto p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p.open_files[file]->Fstat(st, _errno);
}

int syscall_write(int file, char *buf, int nbytes, int *_errno)
{
    auto p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p.open_files[file]->Write(buf, nbytes, _errno);
}

int syscall_read(int file, char *buf, int nbytes, int *_errno)
{
    auto p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p.open_files[file]->Read(buf, nbytes, _errno);
}

int syscall_isatty(int file, int *_errno)
{
    auto p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p.open_files[file]->Isatty(_errno);
}

off_t syscall_lseek(int file, off_t offset, int whence, int *_errno)
{
    auto p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return (off_t)-1;
    }

    return p.open_files[file]->Lseek(offset, whence, _errno);
}

int syscall_open(const char *pathname, int flags, int mode, int *_errno)
{
    // try and get free process file handle
    auto p = GetCurrentThreadForCore()->p;
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
        return -1;
    }

    // special case /dev files
    if(strcmp("/dev/stdin", pathname) == 0)
    {
        p.open_files[fd] = new SeggerRTTFile(0, true, false);
        return fd;
    }
    if(strcmp("/dev/stdout", pathname) == 0)
    {
        p.open_files[fd] = new SeggerRTTFile(0, false, true);
        return fd;
    }
    if(strcmp("/dev/stderr", pathname) == 0)
    {
        p.open_files[fd] = new SeggerRTTFile(0, false, true);
        return fd;
    }

    // try and load in file system
    ext4_file f;
    char fmode[8];

    // convert newlib flags to lwext4 flags
    auto pflags = flags & (O_RDONLY | O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_RDWR);
    switch(pflags)
    {
        case O_RDONLY:
            strcpy(fmode, "r");
            break;

        case O_WRONLY | O_CREAT | O_TRUNC:
            strcpy(fmode, "w");
            break;

        case O_WRONLY | O_CREAT | O_APPEND:
            strcpy(fmode, "a");
            break;

        case O_RDWR:
            strcpy(fmode, "r+");
            break;

        case O_RDWR | O_CREAT | O_TRUNC:
            strcpy(fmode, "w+");
            break;

        case O_RDWR | O_CREAT | O_APPEND:
            strcpy(fmode, "a+");
            break;

        default:
            *_errno = EINVAL;
            return -1;
    }
    
    auto extret = ext4_fopen(&f, pathname, fmode);
    if(extret == EOK)
    {
        p.open_files[fd] = new LwextFile(f, pathname);
        return fd;
    }

    *_errno = extret;
    return -1;
}

int syscall_close(int file, int *_errno)
{
    auto p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    delete p.open_files[file];
    p.open_files[file] = nullptr;

    return 0;
}
