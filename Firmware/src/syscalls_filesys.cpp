#include "syscalls_int.h"
#include "osmutex.h"
#include "SEGGER_RTT.h"
#include "thread.h"
#include "process.h"

#include <cstring>
#include <fcntl.h>
#include <ext4.h>

#include "ext4_thread.h"

extern Spinlock s_rtt;

int syscall_fstat(int file, struct stat *st, int *_errno)
{
    auto &p = GetCurrentThreadForCore()->p;
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
    auto &p = GetCurrentThreadForCore()->p;
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
    auto &p = GetCurrentThreadForCore()->p;
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
    auto &p = GetCurrentThreadForCore()->p;
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
    auto &p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return (off_t)-1;
    }

    return p.open_files[file]->Lseek(offset, whence, _errno);
}

int get_free_fildes(Process &p)
{
    // try and get free process file handle
    int fd = -1;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
    {
        if(p.open_files[i] == nullptr)
        {
            fd = i;
            break;
        }
    }
    return fd;
}

int syscall_open(const char *pathname, int flags, int mode, int *_errno)
{
    // try and get free process file handle
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    bool is_opendir = (mode == S_IFDIR) && (flags == O_RDONLY);
    CriticalGuard cg(p.sl);
    int fd = get_free_fildes(p);
    if(fd == -1)
    {
        *_errno = EMFILE;
        return -1;
    }

    // special case /dev files
    if(strcmp("/dev/stdin", pathname) == 0)
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p.open_files[fd] = new SeggerRTTFile(0, true, false);
        return fd;
    }
    if(strcmp("/dev/stdout", pathname) == 0)
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p.open_files[fd] = new SeggerRTTFile(0, false, true);
        return fd;
    }
    if(strcmp("/dev/stderr", pathname) == 0)
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p.open_files[fd] = new SeggerRTTFile(0, false, true);
        return fd;
    }
    if(strcmp("/dev/ttyUSB0", pathname) == 0)
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p.open_files[fd] = new USBTTYFile();
        return fd;
    }

    // use lwext4
    auto lwf = new LwextFile({ 0 }, pathname);
    p.open_files[fd] = lwf;
    check_buffer(lwf->fname.c_str());
    auto msg = ext4_open_message(lwf->fname.c_str(), flags, mode,
        p, fd, t->ss, t->ss_p);
    if(ext4_send_message(msg))
        return -2;  // deferred return
    else
    {
        delete p.open_files[fd];
        p.open_files[fd] = nullptr;
        *_errno = ENOMEM;
        return -1;
    }
}

int syscall_opendir(const char *pathname, int *_errno)
{
    return syscall_open(pathname, O_RDONLY, S_IFDIR, _errno);
}

int syscall_close1(int file, int *_errno)
{
    auto &p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p.open_files[file]->Close(_errno);
}

int syscall_close2(int file, int *_errno)
{
    auto &p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    int ret = p.open_files[file]->Close2(_errno);
    delete p.open_files[file];
    p.open_files[file] = nullptr;

    return ret;
}

int syscall_mkdir(const char *pathname, mode_t mode, int *_errno)
{
    if(!pathname)
    {
        *_errno = EFAULT;
        return -1;
    }

    // check len as malloc'ing in sram4
    auto pnlen = strlen(pathname);
    if(pnlen > 4096)
    {
        *_errno = ENAMETOOLONG;
        return -1;
    }

    auto npname = (char *)malloc(pnlen + 1);
    if(!npname)
    {
        *_errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(npname, pathname);

    auto t = GetCurrentThreadForCore();
    auto msg = ext4_mkdir_message(npname, mode, t->ss, t->ss_p);
    if(ext4_send_message(msg))
        return -2;  // deferred return
    else
    {
        *_errno = ENOMEM;
        return -1;
    }
}

int syscall_readdir(int file, dirent *de, int *_errno)
{
    auto &p = GetCurrentThreadForCore()->p;
    CriticalGuard(p.sl);
    if(file < 0 || file >= GK_MAX_OPEN_FILES || !p.open_files[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p.open_files[file]->ReadDir(de, _errno);
}
