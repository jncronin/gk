#include "syscalls_int.h"
#include "osmutex.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

int syscall_fstat(int file, struct stat *st)
{
    switch(file)
    {
        case 0:
        case 1:
        case 2:
            st->st_dev = 0;
            st->st_ino = 0;
            st->st_mode = S_IFIFO;
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

    return -1;
}

int syscall_write(int file, char *buf, int nbytes)
{
    if(file == 1)
    {
        // write to stdout
        CriticalGuard cg(s_rtt);
        auto ret = SEGGER_RTT_Write(0, buf, nbytes);
        return (int)ret;
    }

    return -1;
}
