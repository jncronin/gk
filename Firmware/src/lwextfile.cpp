#include "osfile.h"
#include <ext4.h>
#include <cstring>
#include <sys/stat.h>
#include "ext4_thread.h"
#include "thread.h"

LwextFile::LwextFile(ext4_file fildes, std::string _fname) : f(fildes), fname(_fname)
{ 
    type = FileType::FT_Lwext;
}

ssize_t LwextFile::Read(char *buf, size_t count, int *_errno)
{
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_read_message(f, buf, count, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

ssize_t LwextFile::Write(const char *buf, size_t count, int *_errno)
{
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_write_message(f, const_cast<char *>(buf), count, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

int LwextFile::Fstat(struct stat *buf, int *_errno)
{
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_fstat_message(f, buf, fname.c_str(), t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

off_t LwextFile::Lseek(off_t offset, int whence, int *_errno)
{
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_lseek_message(f, offset, whence, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

int LwextFile::Close(int *_errno)
{
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_close_message(f, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}
