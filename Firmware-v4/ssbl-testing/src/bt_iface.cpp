#include <cstdint>
#include "logger.h"
#include "cybt_platform_hci.h"
#include <stm32mp2xx.h>

#define uart USART1

extern "C" void cybt_platform_log_print(const char *fmt_str, ...)
{
    va_list args;
    va_start(args, fmt_str);
    klogv(fmt_str, args);
    va_end(args);
}

cybt_result_t cybt_platform_hci_set_baudrate(uint32_t baudrate)
{
    if(baudrate > 64000000U)
        return CYBT_ERR_HCI_UNSUPPORTED_BAUDRATE;
    
    uart->CR1 &= ~USART_CR1_UE;
    __asm__ volatile("dsb sy\n" ::: "memory");
    uart->BRR = 64000000U / baudrate;
    __asm__ volatile("dsb sy\n" ::: "memory");
    uart->CR1 |= USART_CR1_UE;

    return CYBT_SUCCESS;
}
