#include "osfile.h"
#include <ext4.h>
#include <cstring>
#include <sys/stat.h>

LwextFile::LwextFile(ext4_file fildes, std::string _fname) : f(fildes), fname(_fname)
{ }

ssize_t LwextFile::Read(char *buf, size_t count, int *_errno)
{
    size_t br;
    auto extret = ext4_fread(&f, buf, count, &br);
    if(extret == EOK)
    {
        return static_cast<ssize_t>(br);
    }
    *_errno = extret;
    return -1;
}

ssize_t LwextFile::Write(const char *buf, size_t count, int *_errno)
{
    size_t bw;
    auto extret = ext4_fwrite(&f, buf, count, &bw);
    if(extret == EOK)
    {
        return static_cast<ssize_t>(bw);
    }
    *_errno = extret;
    return -1;
}

struct timespec lwext_time_to_timespec(uint32_t t)
{
    timespec ret;
    ret.tv_nsec = 0;
    ret.tv_sec = t;
    return ret;
}

int LwextFile::Fstat(struct stat *buf, int *_errno)
{
    *buf = { 0 };

    int extret;

    uint32_t t;

    if((extret = ext4_atime_get(fname.c_str(), &t)) != EOK)
        goto _err;
    buf->st_atim = lwext_time_to_timespec(t);

    if((extret = ext4_ctime_get(fname.c_str(), &t)) != EOK)
        goto _err;
    buf->st_ctim = lwext_time_to_timespec(t);

    if((extret = ext4_mtime_get(fname.c_str(), &t)) != EOK)
        goto _err;
    buf->st_mtim = lwext_time_to_timespec(t);

    buf->st_dev = 0;
    buf->st_ino = f.inode;
    buf->st_mode = _IFREG;
    
    uint32_t mode;
    if((extret = ext4_mode_get(fname.c_str(), &mode)) != EOK)
        goto _err;
    buf->st_mode |= mode;
    buf->st_nlink = 1;
    
    uint32_t uid;
    uint32_t gid;
    if((extret = ext4_owner_get(fname.c_str(), &uid, &gid)) != EOK)
        goto _err;
    buf->st_uid = static_cast<uid_t>(uid);
    buf->st_gid = static_cast<gid_t>(gid);

    buf->st_rdev = 0;
    buf->st_size = f.fsize;
    buf->st_blksize = 512;
    buf->st_blocks = (f.fsize + 511) / 512; // round up
    
    return 0;

_err:
    *_errno = extret;
    return -1;
}

off_t LwextFile::Lseek(off_t offset, int whence, int *_errno)
{
    auto extret = ext4_fseek(&f, offset, whence);
    if(extret != EOK)
    {
        *_errno = extret;
        return (off_t)-1;
    }
    return (off_t)f.fpos;
}

LwextFile::~LwextFile()
{
    ext4_fclose(&f);
}
