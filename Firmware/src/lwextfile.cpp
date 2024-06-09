#include "osfile.h"
#include <ext4.h>
#include <cstring>
#include <sys/stat.h>
#include "ext4_thread.h"
#include "thread.h"
#include "process.h"

LwextFile::LwextFile(ext4_file fildes, std::string _fname) : f(fildes)
{ 
    fname = _fname;
    type = FileType::FT_Lwext;
}

ssize_t LwextFile::Read(char *buf, size_t count, int *_errno)
{
    if(is_dir)
    {
        *_errno = EBADF;
        return -1;
    }
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_read_message(f, buf, count, t->ss, t->ss_p);
    check_buffer(buf);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

ssize_t LwextFile::Write(const char *buf, size_t count, int *_errno)
{
    if(is_dir)
    {
        *_errno = EBADF;
        return -1;
    }
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_write_message(f, const_cast<char *>(buf), count, t->ss, t->ss_p);
    check_buffer(buf);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

int LwextFile::Fstat(struct stat *buf, int *_errno)
{
    if((is_dir && !d.f.mp) || (!is_dir && !f.mp))
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_fstat_message(f, d, is_dir, buf, fname.c_str(), t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

off_t LwextFile::Lseek(off_t offset, int whence, int *_errno)
{
    if(is_dir)
    {
        *_errno = EBADF;
        return -1;
    }
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

int LwextFile::Ftruncate(off_t length, int *_errno)
{
    if(is_dir)
    {
        *_errno = EBADF;
        return -1;
    }
    if(!f.mp)
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_ftruncate_message(f, length, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

int LwextFile::Close(int *_errno)
{
    if((is_dir && !d.f.mp) || (!is_dir && !f.mp))
    {
        *_errno = EBADF;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_close_message(is_dir ? d.f : f, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}

int LwextFile::ReadDir(dirent *de, int *_errno)
{
    if(!is_dir)
    {
        *_errno = ENOTDIR;
        return -1;
    }
    if(!de)
    {
        *_errno = EINVAL;
        return -1;
    }
    auto t = GetCurrentThreadForCore();
    auto msg = ext4_readdir_message(d, de, t->ss, t->ss_p);
    if(!ext4_send_message(msg))
    {
        *_errno = ENOMEM;
        return -1;
    }
    return -2;  // deferred return
}
