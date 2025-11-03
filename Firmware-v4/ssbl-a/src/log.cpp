#include <stm32mp2xx.h>

void log(char c)
{
    while(USART6->ISR & USART_ISR_TXFNF);
    USART6->TDR = c;
}

void log(const char *s)
{
    while(*s)
    {
        if(*s == '\n')
        {
            log('\r');
            log('\n');
        }
        else
        {
            log(*s);
        }
        s++;
    }
}
