#include "thread.h"
#include "process.h"
#include "klog_file.h"
#include "osmutex.h"
#include "syscalls_int.h"
#include <fcntl.h>

static void *klogfile_thread(void *);
static BinarySemaphore sem_klogfile;
static LockFreeBuffer *buf = nullptr;

void init_klogfile()
{
    sched.Schedule(Thread::Create("klogfile", klogfile_thread, nullptr, true, GK_PRIORITY_IDLE + 1,
        p_kernel));
}

int klogbuffer_purge_file(LockFreeBuffer &_buf)
{
    buf = &_buf;
    sem_klogfile.Signal();
    return 0;
}

void *klogfile_thread(void *)
{
    int fd = -1;

    int _errno;

    kernel_time last_write = kernel_time_invalid();
    const auto batch_time = kernel_time_from_ms(500);

    while(true)
    {
        if(!sem_klogfile.Wait(clock_cur() + kernel_time_from_ms(500)))
            continue;
        if(buf == nullptr)
            continue;
        if(buf->size() == 0)
            continue;
        
        if(fd == -1)
        {
            // check we have a valid system time before creating the file
            timespec ts;
            clock_get_realtime(&ts);
            
            const timespec y2k { .tv_sec = 946684800, .tv_nsec = 0 };

            if(ts < y2k)
                continue;

            // get it as a timestamp
            tm t;
            if(localtime_r(&ts.tv_sec, &t) == nullptr)
                continue;
            
            char fname[1024];
            if(!strftime(fname, 1023, "/var/log/syslog_%Y%m%d_%H%M%S", &t))
            {
                continue;
            }

            fd = syscall_open(fname, O_CREAT | O_RDWR, 0644, &_errno);
            if(fd < 0)
            {
                klog("klogfile: cannot open %s: %d\n", fname, _errno);
                continue;
            }
        }

        // batch up writes so we don't wear out the SD
        if(!kernel_time_is_valid(last_write) || clock_cur() >= last_write + batch_time)
        {
            while(true)
            {
                char rbuf[512];

                auto bret = buf->read(rbuf, sizeof(rbuf));
                if(bret > 0)
                {
                    auto fwritten = syscall_write(fd, rbuf, bret, &_errno);
                    if(fwritten < 0)
                    {
                        klog("klogfile: failed to write %zd bytes: %d (%d)\n", bret, fwritten, _errno);
                        break;
                    }
                }

                last_write = clock_cur();

                if(bret == 0 || bret != sizeof(rbuf))
                {
                    break;
                }
            }
        }
    }
}
