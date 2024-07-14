#include "osringbuffer.h"
#include "clocks.h"
#include <stdarg.h>
#include "gk_conf.h"
#include "osmutex.h"
#include "logger.h"

static __attribute__((section(".sram_data"))) RingBuffer<char, 16*1024> kernel_log;
static SRAM4_DATA Spinlock s_log;
static SRAM4_DATA BinarySemaphore sem_log;

int klog(const char *format, ...)
{
    CriticalGuard cg(s_log);
    auto ret = fprintf(stderr, "[%llu]: ", clock_cur_us());
    va_list args;
    va_start(args, format);

    ret += vfprintf(stderr, format, args);
    
    va_end(args);

    sem_log.Signal();
    return ret;
}

void *logger_task(void *param)
{
    while(true)
    {
        if(sem_log.Wait())
        {
            CriticalGuard cg(s_log);
            char c;
            while(kernel_log.Read(&c))
                SEGGER_RTT_PutChar(0, c);
        }
    }
}

ssize_t log_fwrite(const void *buf, size_t count)
{
    return kernel_log.Write((char *)buf, (int)count);
}
