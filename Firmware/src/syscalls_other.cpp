#include "clocks.h"
#include <sys/time.h>
#include "syscalls.h"
#include <errno.h>

int syscall_gettimeofday(timeval *tv, timezone *tz, int *_errno)
{
    if(!tv)
    {
        *_errno = EINVAL;
        return -1;
    }

    timespec ts;
    clock_get_now(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;

    if(tz)
    {
        // just return UTC for now
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = DST_NONE;
    }

    return 0;
}
