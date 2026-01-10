#include <stm32mp2xx.h>
#include <cstdio>
#include "clocks.h"
#include <cstdarg>
#include "osspinlock.h"
#include "vmem.h"
#include "logger.h"

int logger_printf(const timespec &now, const char *format, va_list va);

#define USART6_VMEM PMEM_TO_VMEM(USART6)

#ifndef GK_NO_UART_LOCK
Spinlock s_log;
#endif

static void uart_log(char c)
{
    if(c == '\n')
    {
        while((USART6_VMEM->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6_VMEM->TDR = '\r';
        while((USART6_VMEM->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6_VMEM->TDR = '\n';
    }
    else
    {
        while((USART6_VMEM->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6_VMEM->TDR = c;
    }
}

static void uart_log(const char *buf, size_t count)
{
    while(count--)
    {
        uart_log(*buf++);
    }
}

ssize_t log_fwrite(const void *buf, size_t count)
{
    uart_log((const char *)buf, count);
    return (ssize_t)count;
}

int klogv(const char *format, va_list va)
{
#ifndef GK_NO_UART_LOCK
    CriticalGuard cg(s_log);
#endif
    auto cur = clock_cur();

    auto ret = logger_printf(cur, format, va);
    
    return ret;
}

int klog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    auto ret = klogv(format, args);
    va_end(args);
    return ret;
}
