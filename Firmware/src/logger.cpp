#include "osringbuffer.h"
#include "clocks.h"
#include <stdarg.h>
#include "gk_conf.h"
#include "osmutex.h"
#include "logger.h"
#include "scheduler.h"
#include "thread.h"
#include "process.h"
#include "cache.h"

#if GK_LOG_USB
#include "tusb.h"
#endif

extern Process kernel_proc;

#if !GK_ENABLE_USB
#if GK_LOG_USB
#undef GK_LOG_USB
#define GK_LOG_USB 0
#endif
#endif

using klog_t = RingBuffer<char, GK_LOG_SIZE>;

typedef ssize_t (*log_direct_func)(const void *buf, size_t count);

struct klog_def
{
    klog_t *klog;
    log_direct_func klog_direct;
    Spinlock *s_log;
    BinarySemaphore *s_sem;
};

static SRAM4_DATA Spinlock s_log;

#if GK_LOG_PERSISTENT
static const constexpr unsigned int log_id_persistent = 0;
static SRAM4_DATA Spinlock s_log_persistent;
static ssize_t log_persistent(const void *buf, size_t count);
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_persistent
{
    .klog = nullptr,
    .klog_direct = log_persistent,
    .s_log = &s_log_persistent,
    .s_sem = nullptr,
};
#endif
#if GK_LOG_RTT
static const constexpr unsigned int log_id_rtt = GK_LOG_PERSISTENT;
static SRAM4_DATA Spinlock s_log_rtt;
static ssize_t log_rtt(const void *buf, size_t count);
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_rtt
{
    .klog = nullptr,
    .klog_direct = log_rtt,
    .s_log = &s_log_rtt,
    .s_sem = nullptr,
};
#endif
#if GK_LOG_USB
static const constexpr unsigned int log_id_usb = GK_LOG_PERSISTENT + GK_LOG_RTT;
static __attribute__((section(".sram_data"))) klog_t klog_usb;
static SRAM4_DATA Spinlock s_log_usb;
static SRAM4_DATA BinarySemaphore sem_log_usb;
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_usb
{
    .klog = &klog_usb,
    .klog_direct = nullptr,
    .s_log = &s_log_usb,
    .s_sem = &sem_log_usb,
};
#endif
#if GK_LOG_FILE
static const constexpr unsigned int log_id_file = GK_LOG_PERSISTENT + GK_LOG_RTT + GK_LOG_USB;
static __attribute__((section(".sram_data"))) klog_t klog_file;
static SRAM4_DATA Spinlock s_log_file;
static SRAM4_DATA BinarySemaphore sem_log_file;
static const constexpr __attribute__((section(".sram_rdata"))) klog_def klog_def_file
{
    .klog = &klog_file,
    .klog_direct = nullptr,
    .s_log = &s_log_file,
    .s_sem = &sem_log_file,
};
#endif

static const constexpr unsigned int n_logs = GK_LOG_PERSISTENT + GK_LOG_RTT + GK_LOG_USB + GK_LOG_FILE;

static const constexpr __attribute__((section(".sram_rdata"))) klog_def *klogs[] {
#if GK_LOG_PERSISTENT
    &klog_def_persistent,
#endif
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

static uint64_t _last_us = 0;

int klog(const char *format, ...)
{
    CriticalGuard cg(s_log);
    auto cur_us = clock_cur_us();
    if(cur_us < _last_us)
        BKPT_IF_DEBUGGER();
    auto ret = fprintf(stderr, "[%llu]: ", cur_us);
    va_list args;
    va_start(args, format);

    ret += vfprintf(stderr, format, args);
    fflush(stderr);
    
    va_end(args);

    return ret;
}

#if GK_LOG_PERSISTENT
static const constexpr unsigned int plog_len = 3*1024;
static __attribute__((section(".backupsram"))) __attribute__((aligned(16))) char plog[plog_len];
static __attribute__((section(".backupsram"))) __attribute__((aligned(16))) unsigned int plog_ptr;
__attribute__((section(".backupsram"))) __attribute__((aligned(16))) int plog_frozen;

static void unlock_sram()
{
    //PWR->CR1 |= PWR_CR1_DBP;
    //__DMB();
    //while(!(PWR->CR1 & PWR_CR1_DBP));
}

static void lock_sram()
{
    //PWR->CR1 &= ~PWR_CR1_DBP;
    //__DMB();
}

ssize_t log_persistent(const void *buf, size_t count)
{
    unlock_sram();
    if(plog_frozen)
    {
        lock_sram();
        return 0;
    }

    if(plog_ptr > plog_len) plog_ptr = 0;

    memcpy_split_dest(plog, buf, count, plog_ptr, plog_len, true);
    plog_ptr += count;
    while(plog_ptr > plog_len)
        plog_ptr -= plog_len;
    CleanOrInvalidateM7Cache((uint32_t)&plog_ptr, 16, CacheType_t::Data);
    lock_sram();
    return (ssize_t)count;
}

void log_freeze_persistent_log()
{
    unlock_sram();
    plog_frozen = 1;
    CleanOrInvalidateM7Cache((uint32_t)&plog_frozen, 16, CacheType_t::Data);
    __DMB();
    lock_sram();
}

void log_unfreeze_persistent_log()
{
    unlock_sram();
    plog_frozen = 0;
    CleanOrInvalidateM7Cache((uint32_t)&plog_frozen, 16, CacheType_t::Data);
    __DMB();
    lock_sram();
}

MemRegion log_get_persistent()
{
    CriticalGuard cg(s_log_persistent);
    if(plog_frozen)
    {
        auto ret = memblk_allocate(plog_len, MemRegionType::SRAM, "persistent log");
        if(!ret.valid)
            ret = memblk_allocate(plog_len, MemRegionType::AXISRAM, "persistent log");
        if(!ret.valid)
            ret = memblk_allocate(plog_len, MemRegionType::SDRAM, "persistent log");

        if(ret.valid)
        {
            memcpy_split_src((void *)ret.address, plog, plog_len, plog_ptr, plog_len);
            CleanOrInvalidateM7Cache(ret.address, plog_len, CacheType_t::Data);
        }

        plog_frozen = 0;
        CleanOrInvalidateM7Cache((uint32_t)&plog_frozen, 16, CacheType_t::Data);
        plog_ptr = 0;
        CleanOrInvalidateM7Cache((uint32_t)&plog_ptr, 16, CacheType_t::Data);

        return ret;
    }
    else
    {
        plog_ptr = 0;
        CleanOrInvalidateM7Cache((uint32_t)&plog_ptr, 16, CacheType_t::Data);
        return InvalidMemregion();
    }
}

#else
void log_freeze_persistent_log() {}
void log_unfreeze_persistent_log() {}
MemRegion log_get_persistent() { return InvalidMemregion(); }
#endif

#if GK_LOG_RTT
ssize_t log_rtt(const void *buf, size_t count)
{
#if 0
    void SWO_PrintChar(char c, uint8_t portNo);
    void uart_sendchar(char c);

    for(unsigned int i = 0; i < count; i++)
    {
        SWO_PrintChar(((const char *)buf)[i], 0);
        uart_sendchar(((const char *)buf)[i]);
    }
#endif
    return SEGGER_RTT_Write(0, buf, count);
}
#endif

#if GK_LOG_USB
static void *usb_logger_task(void *param)
{
    auto clog = reinterpret_cast<klog_def *>(param);
    while(true)
    {
        if(clog->s_sem->Wait())
        {
            CriticalGuard cg(s_log);
            char c;
            bool sent = false;
            while(tud_cdc_n_connected(1) &&
                tud_cdc_n_write_available(1) &&
                clog->klog->Read(&c))
            {
                if(c == '\n')
                {
                    char cr[] = { '\r', '\n' };
                    tud_cdc_n_write(1, cr, 2);
                }
                else
                {
                    tud_cdc_n_write(1, &c, 1);
                }
                sent = true;
            }
            if(sent)
                tud_cdc_n_write_flush(1);
        }
    }
}
#endif

#if GK_LOG_FILE
// buffer in AXISRAM for SDMMC to read
static const constexpr unsigned int file_buf_size = 512;
__attribute__((aligned(32))) char file_buf[file_buf_size];

#include "syscalls_int.h"

extern SimpleSignal fs_provision_complete;

static void *file_logger_task(void *param)
{
    int fno = -1;
    const char log_fname[] = "/syslog";
    const char log_fname_old[] = "/syslog.prev";

    while(!fs_provision_complete.Wait());

    // first try and copy any existing syslog to the previous file
    auto fno_syslog_cur = deferred_call(syscall_open, log_fname, O_RDONLY, 0);
    if(fno_syslog_cur >= 0)
    {
        auto fno_syslog_prev = deferred_call(syscall_open, log_fname_old, O_WRONLY | O_CREAT | O_TRUNC, 0);
        if(fno_syslog_prev)
        {
            static char flog_buf[512];

            while(true)
            {
                auto br = deferred_call(syscall_read, fno_syslog_cur, flog_buf, sizeof(flog_buf));
                if(br > 0)
                {
                    auto bw = deferred_call(syscall_write, fno_syslog_prev, flog_buf, br);
                    if(bw != br)
                    {
                        klog("log: error writing syslog to syslog.prev\n");
                        break;
                    }
                }
                else if(br == 0)
                    break;
                else
                {
                    klog("log: error reading from syslog\n");
                }
            }

            close(fno_syslog_prev);
        }
        close(fno_syslog_cur);
    }

    while(fno < 0)
    {
        fno = deferred_call(syscall_open, log_fname, O_WRONLY | O_CREAT | O_TRUNC, 0);
    }

    auto clog = reinterpret_cast<klog_def *>(param);
    while(true)
    {
        if(clog->s_sem->Wait())
        {
            while(true)
            {
                int bw = 0;
                {
                    CriticalGuard cg(s_log);
                    bw = clog->klog->Read(file_buf, file_buf_size);
                }
                if(bw)
                {
                    deferred_call(syscall_write, fno, file_buf, bw);
                }
                else
                    break;
            }
        }
    }
}
#endif


int init_log()
{
#if GK_LOG_USB
    Schedule(Thread::Create("log_usb", usb_logger_task, (void *)&klog_def_usb, true, GK_PRIORITY_VHIGH, kernel_proc,
        CPUAffinity::PreferM4));
#endif
#if GK_LOG_FILE
    Schedule(Thread::Create("log_file", file_logger_task, (void *)&klog_def_file, true, GK_PRIORITY_VHIGH, kernel_proc,
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
        int cwritten_dir = clog->klog_direct ? clog->klog_direct(buf, count) : 0;
        int cwritten_indir = clog->klog ? clog->klog->Write((char *)buf, count) : 0;
        if(cwritten_dir > nwritten) nwritten = cwritten_dir;
        if(cwritten_indir > nwritten) nwritten = cwritten_indir;
        if(clog->s_sem) clog->s_sem->Signal();
    }
    return nwritten;
}
