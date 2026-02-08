#include "syscalls_int.h"
#include <sys/time.h>
#include <sys/times.h>
#include "clocks.h"
#include "process.h"
#include "thread.h"

int syscall_gettimeofday(timeval *tv, timezone *tz, int *_errno)
{
    if(!tv)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(tv);

    timespec ts;
    clock_get_realtime(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;

    if(tz)
    {
        ADDR_CHECK_STRUCT_W(tz);
        // just return UTC for now
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = DST_NONE;
    }

    return 0;
}

clock_t syscall_times(struct tms *buf, int *_errno)
{
    ADDR_CHECK_STRUCT_W(buf);

    auto p = GetCurrentProcessForCore();
    if(p)
    {
        unsigned long user_dur = 0;

        {
            CriticalGuard cg(p->sl, ThreadList.sl);
            for(auto tid : p->threads)
            {
                auto t = ThreadList._get(tid);
                if(t.v)
                {
                    user_dur += t.v->thread_time_us;
                }
            }
        }

        buf->tms_utime = (clock_t)user_dur;
        buf->tms_stime = 0;
        buf->tms_cutime = 0;
        buf->tms_cstime = 0;
    }
    else
    {
        memset(buf, 0, sizeof(*buf));
    }

    return (clock_t)clock_cur_us();
}
