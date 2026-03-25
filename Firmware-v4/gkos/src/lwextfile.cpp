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
    return gk_ext4_read(f, buf, count, _errno);
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
    return gk_ext4_write(f, buf, count, _errno);
}

int LwextFile::Fstat(struct stat *buf, int *_errno)
{
    if((is_dir && !d.f.mp) || (!is_dir && !f.mp))
    {
        *_errno = EBADF;
        return -1;
    }
    return gk_ext4_fstat(f, d, is_dir, buf, fname.c_str(), _errno);
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
    return gk_ext4_lseek(f, offset, whence, _errno);
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
    return gk_ext4_ftruncate(f, length, _errno);
}

int LwextFile::Close(int *_errno)
{
    if((is_dir && !d.f.mp) || (!is_dir && !f.mp))
    {
        *_errno = EBADF;
        return -1;
    }
    return gk_ext4_close(is_dir ? d.f : f, _errno);
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
    return gk_ext4_readdir(d, de, _errno);
}
