#include "tusb.h"
#include "osfile.h"
#include <sys/stat.h>

ssize_t USBTTYFile::Write(const char *buf, size_t count, int *_errno)
{
    return tud_cdc_write(buf, count);
}

ssize_t USBTTYFile::Read(char *buf, size_t count, int *_errno)
{
    return tud_cdc_read(buf, count);
}

int USBTTYFile::Fstat(struct stat *st, int *_errno)
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

off_t USBTTYFile::Lseek(off_t offset, int whence, int *_errno)
{
    (void)offset;
    (void)whence;
    *_errno = ESPIPE;
    return static_cast<off_t>(-1);
}

int USBTTYFile::Isattty(int *_errno)
{
    (void)_errno;
    return 1;
}
