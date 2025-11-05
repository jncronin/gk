#include <stm32mp2xx.h>
#include <cstdio>
#include "clocks.h"
#include <cstdarg>

void log(char c)
{
    if(c == '\n')
    {
        while((USART6->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6->TDR = '\r';
        while((USART6->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6->TDR = '\n';
    }
    else
    {
        while((USART6->ISR & USART_ISR_TXFNF_Msk) == 0);
        USART6->TDR = c;
    }
}

void log(const char *s)
{
    while(*s)
    {
        log(*s++);
    }
}

int klog(const char *format, ...)
{
    auto now = clock_cur();
    auto ret = fprintf(stderr, "[%ld.%09ld]: ", now.tv_sec, now.tv_nsec);
    va_list args;
    va_start(args, format);

    ret += vfprintf(stderr, format, args);
    fflush(stderr);
    
    va_end(args);

    return ret;
}
