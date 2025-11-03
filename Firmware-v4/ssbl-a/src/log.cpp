#include <stm32mp2xx.h>

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
