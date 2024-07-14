#include "osringbuffer.h"
#include "clocks.h"
#include <stdarg.h>
#include "gk_conf.h"
#include "osmutex.h"
#include "logger.h"
#include "scheduler.h"
#include "thread.h"
#include "process.h"

extern Process kernel_proc;

#if !GK_ENABLE_USB
#if GK_LOG_USB
#undef GK_LOG_USB
#define GK_LOG_USB 0
#endif
#endif

using klog_t = RingBuffer<char, GK_LOG_SIZE>;

struct klog_def
{
    klog_t *klog;
    Spinlock *s_log;
    BinarySemaphore *s_sem;
};

static SRAM4_DATA Spinlock s_log;

#if GK_LOG_RTT
static const constexpr unsigned int log_id_rtt = 0;
static __attribute__((section(".sram_data"))) klog_t klog_rtt;
static SRAM4_DATA Spinlock s_log_rtt;
static SRAM4_DATA BinarySemaphore sem_log_rtt;
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_rtt
{
    .klog = &klog_rtt,
    .s_log = &s_log_rtt,
    .s_sem = &sem_log_rtt
};
#endif
#if GK_LOG_USB
static const constexpr unsigned int log_id_usb = GK_LOG_RTT;
static __attribute__((section(".sram_data"))) klog_t klog_usb;
static SRAM4_DATA Spinlock s_log_usb;
static SRAM4_DATA BinarySemaphore sem_log_usb;
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_usb
{
    .klog = &klog_usb,
    .s_log = &s_log_usb,
    .s_sem = &sem_log_usb
};
#endif
#if GK_LOG_FILE
static const constexpr unsigned int log_id_file = GK_LOG_RTT + GK_LOG_USB;
static __attribute__((section(".sram_data"))) klog_t klog_file;
static SRAM4_DATA Spinlock s_log_file;
static SRAM4_DATA BinarySemaphore sem_log_file;
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_file
{
    .klog = &klog_file,
    .s_log = &s_log_file,
    .s_sem = &sem_log_file
};
#endif

static const constexpr unsigned int n_logs = GK_LOG_RTT + GK_LOG_USB + GK_LOG_FILE;

static const constexpr __attribute__((section(".sram_rdata"))) klog_def *klogs[] {
#if GK_LOG_RTT
    &klog_def_rtt,
#endif
#if GK_LOG_USB
    &klog_def_usb,
#endif
#if GK_LOG_FILE
    &klog_def_file
#endif
};

int klog(const char *format, ...)
{
    CriticalGuard cg(s_log);
    auto ret = fprintf(stderr, "[%llu]: ", clock_cur_us());
    va_list args;
    va_start(args, format);

    ret += vfprintf(stderr, format, args);
    
    va_end(args);

    return ret;
}

static void *rtt_logger_task(void *param)
{
    auto clog = reinterpret_cast<klog_def *>(param);
    while(true)
    {
        if(clog->s_sem->Wait())
        {
            CriticalGuard cg(s_log);
            char c;
            while(clog->klog->Read(&c))
                SEGGER_RTT_PutChar(0, c);
        }
    }
}

int init_log()
{
#if GK_LOG_RTT
    Schedule(Thread::Create("logger", rtt_logger_task, (void *)&klog_def_rtt, true, GK_PRIORITY_VHIGH, kernel_proc,
        CPUAffinity::PreferM4));
#endif

    return 0;
}

ssize_t log_fwrite(const void *buf, size_t count)
{
    int nwritten = 0;
    for(unsigned int i = 0; i < n_logs; i++)
    {
        auto clog = klogs[i];
        CriticalGuard cg(*clog->s_log);
        auto cwritten = clog->klog->Write((char *)buf, count);
        if(cwritten > nwritten) nwritten = cwritten;
        clog->s_sem->Signal();
    }
    return nwritten;
}
