#include "osfile.h"
#include "SEGGER_RTT.h"
#include <sys/stat.h>

SeggerRTTFile::SeggerRTTFile(unsigned int buf, bool for_read, bool for_write) :
    buf_idx(buf), can_read(for_read), can_write(for_write)
{
    type = FileType::FT_SeggerRTT;
}

ssize_t SeggerRTTFile::Write(const char *buf, size_t count, int *_errno)
{
    if(!can_write)
    {
        *_errno = EBADF;
        return -1;
    }

    return static_cast<ssize_t>(SEGGER_RTT_Write(buf_idx, buf, count));
}

ssize_t SeggerRTTFile::Read(char *buf, size_t count, int *_errno)
{
    if(!can_read)
    {
        *_errno = EBADF;
        return -1;
    }

    return static_cast<ssize_t>(SEGGER_RTT_Read(buf_idx, buf, count));
}

int SeggerRTTFile::Fstat(struct stat *st, int *_errno)
{
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_mode = 0x2190;   // as returned by linux
    st->st_nlink = 0;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    st->st_atim = { 0, 0 };
    st->st_mtim = { 0, 0 };
    st->st_ctim = { 0, 0 };
    return 0;
}

off_t SeggerRTTFile::Lseek(off_t offset, int whence, int *_errno)
{
    (void)offset;
    (void)whence;
    *_errno = ESPIPE;
    return static_cast<off_t>(-1);
}

int SeggerRTTFile::Isattty(int *_errno)
{
    (void)_errno;
    return 1;
}
