#include <stm32mp2xx.h>
#include <cstdarg>
#include <cstdio>
#include "logger.h"
#include "clocks.h"
#include "osmutex.h"
#include "vmem.h"

static Spinlock sl_log;

int logger_printf(const timespec &now, const char *format, va_list va);

#define USART6_VMEM ((USART_TypeDef *)PMEM_TO_VMEM(USART6_BASE))

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
    // TODO: make this less stop-the-world - use buffers as per gkv3 and a UART thread to output
    uart_log((const char *)buf, count);
    return (ssize_t)count;
}