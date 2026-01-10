#ifndef CYHAL_HW_TYPES_H
#define CYHAL_HW_TYPES_H

#include <stddef.h>

#define CYHAL_API_VERSION               (2)

typedef unsigned int cyhal_gpio_t;
#define NC 0xffffffffU

typedef unsigned int cyhal_signal_type_t;
typedef unsigned int cyhal_source_t;
typedef void *cyhal_sdio_t;
typedef void *cyhal_uart_t;
typedef void *cyhal_lptimer_t;
typedef void *cyhal_clock_t;

typedef struct
{

} cyhal_sdio_configurator_t;

typedef struct
{

} cyhal_uart_configurator_t;

#endif
