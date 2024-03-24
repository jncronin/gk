#include "tusb.h"
#include "osfile.h"
#include <sys/stat.h>
#include "thread.h"

ssize_t USBTTYFile::Write(const char *buf, size_t count, int *_errno)
{
    auto ret = tud_cdc_write(buf, count);
    if(ret)
    {
        tud_cdc_write_flush();
    }
    return ret;
}

// USB TTY is not thread-safe
SRAM4_DATA static char *rbuf;
SRAM4_DATA static size_t rcount;
SRAM4_DATA static Thread *rt = nullptr;

ssize_t USBTTYFile::Read(char *buf, size_t count, int *_errno)
{
    // need to block here or newlib thinks we are returning EOF and
    //  never calls read again
    auto ret = tud_cdc_read(buf, count);
    if(ret == 0)
    {
        rbuf = buf;
        rcount = count;
        rt = GetCurrentThreadForCore();
        return -2;
    }
    else
    {
        return ret;
    }
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

extern "C" void tud_cdc_rx_cb(uint8_t itf)
{
    // see if we have a waiting thread
    if(rt)
    {
        auto ret = tud_cdc_read(rbuf, rcount);
        if(ret)
        {
            auto crt = rt;
            rt = nullptr;
            crt->ss_p.uval1 = ret;
            crt->ss.Signal();
        }
    }
}
