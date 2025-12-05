#include "fatfs_file.h"
#include "ff.h"
#include "logger.h"

int fatfs_open(const std::string &fname, PFile *f, bool for_read, bool for_write)
{
    FIL fil = { 0 };
    BYTE mode = 0;
    if(for_read)
        mode |= FA_READ;
    if(for_write)
        mode |= FA_WRITE;
    auto ffret = f_open(&fil, fname.c_str(), mode);
    if(ffret != FR_OK)
    {
        klog("fatfs: open %s failed: %d\n", fname.c_str(), ffret);
        return -1;
    }
    
    *f = std::make_shared<FatFsFile>(&fil, for_read, for_write);
    return -0;
}

FatFsFile::FatFsFile(FIL *fil, bool for_read, bool for_write)
{
    f = *fil;
    can_read = for_read;
    can_write = for_write;
}

ssize_t FatFsFile::Write(const char *buf, size_t count, int *_errno)
{
    if(!can_write)
    {
        *_errno = EBADF;
        return -1;
    }

    UINT bw;
    auto ffret = f_write(&f, buf, count, &bw);
    if(ffret == FR_OK)
    {
        return (ssize_t)bw;
    }
    else
    {
        *_errno = EFAULT;
        return -1;
    }
}

ssize_t FatFsFile::Read(char *buf, size_t count, int *_errno)
{
    if(!can_read)
    {
        *_errno = EBADF;
        return -1;
    }

    UINT br;
    auto ffret = f_read(&f, buf, count, &br);
    if(ffret == FR_OK)
    {
        return (ssize_t)br;
    }
    else
    {
        *_errno = EFAULT;
        return -1;
    }
}

int FatFsFile::Fstat(struct stat *buf, int *_errno)
{
    *_errno = ENOSYS;
    return -1;
}

off_t FatFsFile::Lseek(off_t offset, int whence, int *_errno)
{
    off_t loc = 0;
    switch(whence)
    {
        case SEEK_SET:
            loc = offset;
            break;
        case SEEK_CUR:
            loc = f_tell(&f) + offset;
            break;
        case SEEK_END:
            loc = f_size(&f) + offset;
            break;
        default:
            *_errno = EINVAL;
            return -1;
    }

    auto fr = f_lseek(&f, loc);
    if(fr == FR_OK)
    {
        return f_tell(&f);
    }
    else
    {
        *_errno = EFAULT;
        return -1;
    }
}

int FatFsFile::Close(int *_errno)
{
    auto fr = f_close(&f);
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
