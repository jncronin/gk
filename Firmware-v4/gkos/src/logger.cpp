#include <stm32mp2xx.h>
#include <cstdarg>
#include <cstdio>
#include "logger.h"
#include "clocks.h"
#include "osmutex.h"
#include "vmem.h"

static Spinlock sl_log;

int logger_printf(const timespec &now, const char *format, va_list va);

int klog(const char *format, ...)
{
    CriticalGuard cg(sl_log);
    auto cur = clock_cur();

    va_list args;
    va_start(args, format);

    auto ret = logger_printf(cur, format, args);
    
    va_end(args);

    return ret;
}
