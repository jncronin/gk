#include "gk_conf.h"
#include "lockfreebuffer.h"
#include "usb.h"
#include "klog_usb.h"

#if GK_LOG_USB
static uint8_t log_usb_buf_d[GK_LOG_SIZE] = { 0 };

LockFreeBuffer log_usb_buf(log_usb_buf_d, GK_LOG_SIZE);

BinarySemaphore usb_log_sem;

ssize_t usb_log_write(const void *d, size_t len)
{
    auto ret = log_usb_buf.write(d, len);
    if(ret > 0)
    {
        usb_log_sem.Signal();
    }
    return ret;
}

#endif
