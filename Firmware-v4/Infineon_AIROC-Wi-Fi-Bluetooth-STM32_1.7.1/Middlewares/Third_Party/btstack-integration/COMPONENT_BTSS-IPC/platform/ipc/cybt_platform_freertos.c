/***************************************************************************//**
* \file cybt_platform_freertos.c
*
* \brief
* Implementation for BT porting interface on FreeRTOS
*
********************************************************************************
* \copyright
* Copyright 2018-2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "cybt_platform_interface.h"
#include "cybt_platform_trace.h"

#ifdef ENABLE_DEBUG_UART
#include "cybt_debug_uart.h"
#endif // ENABLE_DEBUG_UART

/******************************************************************************
 *                                Constants
 ******************************************************************************/


/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/


/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/



/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
extern void cybt_platform_stack_timer_init(void);
extern void cybt_platform_stack_timer_deinit(void);

/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
void *cybt_platform_malloc(uint32_t req_size)
{
    return malloc((size_t) req_size);
}

void cybt_platform_free(void *p_mem_block)
{
    free(p_mem_block);
}

void cybt_platform_disable_irq(void)
{
    __disable_irq();
}

void cybt_platform_enable_irq(void)
{
    __enable_irq();
}

void cybt_platform_init(void)
{
    cybt_platform_stack_timer_init();
}

void cybt_platform_deinit(void)
{
    MAIN_TRACE_DEBUG("cybt_platform_deinit()");
    cybt_platform_stack_timer_deinit();
}

void cybt_platform_log_print(const char *fmt_str, ...)
{
    static char buffer[CYBT_TRACE_BUFFER_SIZE];
    va_list ap;
    int len;
    cy_time_t time;

    cy_rtos_get_time(&time);
    va_start(ap, fmt_str);
    len = vsnprintf(buffer, CYBT_TRACE_BUFFER_SIZE, fmt_str, ap);
    va_end(ap);

#ifdef ENABLE_DEBUG_UART
    cybt_debug_uart_send_trace(len, (uint8_t*)buffer);
#else // ENABLE_DEBUG_UART
    UNUSED_VARIABLE(len);
    printf("[%u] %s\r\n", (unsigned int)time, buffer);
#endif // ENABLE_DEBUG_UART
}
