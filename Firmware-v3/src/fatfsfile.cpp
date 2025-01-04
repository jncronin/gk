#include "ff.h"
#include "osfile.h"

FatfsFile::FatfsFile(FIL *_fp, std::string _fname)
{
    f = _fp;
    fname = _fname;
}

ssize_t FatfsFile::Write(const char *buf, size_t count, int *_errno)
{
    *_errno = ENOSYS;
    return -1;
}

ssize_t FatfsFile::Read(char *buf, size_t count, int *_errno)
{
    UINT br;
    auto fr = f_read(f, buf, count, &br);
    if(fr == FR_OK)
    {
        return br;
    }
    else
    {
        *_errno = EFAULT;
        return -1;
    }
}

int FatfsFile::Fstat(struct stat *buf, int *_errno)
{
    *_errno = ENOSYS;
    return -1;
}

off_t FatfsFile::Lseek(off_t offset, int whence, int *_errno)
{
    off_t loc = 0;
    switch(whence)
    {
        case SEEK_SET:
            loc = offset;
            break;
        case SEEK_CUR:
            loc = f_tell(f) + offset;
            break;
        case SEEK_END:
            loc = f_size(f) + offset;
            break;
        default:
            *_errno = EINVAL;
            return -1;
    }

    auto fr = f_lseek(f, loc);
    if(fr == FR_OK)
    {
        return f_tell(f);
    }
    else
    {
        *_errno = EFAULT;
        return -1;
    }
}

int FatfsFile::Close(int *_errno)
{
    auto fr = f_close(f);
    if(fr == FR_OK)
    {
        return 0;
    }
    else
    {
        *_errno = EFAULT;
        return -1;
    }
}

