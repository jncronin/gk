/***********************************************************************************************//**
 * \file cyabs_rtos_impl.h
 *
 * \brief
 * Internal definitions for RTOS abstraction layer
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2019-2021 Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation
 *
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
 **************************************************************************************************/

#pragma once

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <timers.h>
#include "stdbool.h"
#if defined(CY_USING_HAL)
#include "cyhal.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
*                 Constants
******************************************************/
#define CY_RTOS_MIN_STACK_SIZE      300            /**< Minimum stack size in bytes */
#define CY_RTOS_ALIGNMENT           0x00000008UL   /**< Minimum alignment for RTOS objects */
#define CY_RTOS_ALIGNMENT_MASK      0x00000007UL   /**< Mask for checking the alignment of
                                                        created RTOS objects */
#if !defined(CY_RTOS_MAX_SUSPEND_NESTING)
#define CY_RTOS_MAX_SUSPEND_NESTING 3              /**< Maximum nesting allowed for calls
                                                        to scheduler suspend from ISR */
#endif
/******************************************************
*                   Enumerations
******************************************************/

typedef enum cy_thread_priority
{
    CY_RTOS_PRIORITY_MIN         = 0,
    CY_RTOS_PRIORITY_LOW         = (configMAX_PRIORITIES * 1 / 7),
    CY_RTOS_PRIORITY_BELOWNORMAL = (configMAX_PRIORITIES * 2 / 7),
    CY_RTOS_PRIORITY_NORMAL      = (configMAX_PRIORITIES * 3 / 7),
    CY_RTOS_PRIORITY_ABOVENORMAL = (configMAX_PRIORITIES * 4 / 7),
    CY_RTOS_PRIORITY_HIGH        = (configMAX_PRIORITIES * 5 / 7),
    CY_RTOS_PRIORITY_REALTIME    = (configMAX_PRIORITIES * 6 / 7),
    CY_RTOS_PRIORITY_MAX         = configMAX_PRIORITIES - 1
} cy_thread_priority_t;

/******************************************************
*                 Type Definitions
******************************************************/

typedef struct
{
    SemaphoreHandle_t mutex_handle;
    bool              is_recursive;
} cy_mutex_t;

typedef QueueHandle_t      cy_queue_t;
typedef SemaphoreHandle_t  cy_semaphore_t;
typedef TaskHandle_t       cy_thread_t;
typedef EventGroupHandle_t cy_event_t;
typedef TimerHandle_t      cy_timer_t;
typedef uint32_t           cy_timer_callback_arg_t;
typedef void*              cy_thread_arg_t;
typedef uint32_t           cy_time_t;
typedef BaseType_t         cy_rtos_error_t;

#if defined(CY_USING_HAL)
/** Stores a reference to an lptimer instance for use with vApplicationSleep().
 *
 * @param[in] timer  Pointer to the lptimer handle
 */
void cyabs_rtos_set_lptimer(cyhal_lptimer_t* timer);

/** Gets a reference to the lptimer instance object used by vApplicationSleep(). This instance is
 * what was explicitly set by @ref cyabs_rtos_set_lptimer or, if none was set, what was
 * automatically allocated by the first call to vApplicationSleep().
 *
 * @return Pointer to the lptimer handle
 */
cyhal_lptimer_t* cyabs_rtos_get_lptimer(void);

/** If the interrupt is in pending state and disabled need to remove it from NVIC.
 * NOTE: this function if for internal use
 */
extern void _cyabs_rtos_clear_disabled_irq_in_pending(void);

#endif //defined(CY_USING_HAL)


cy_time_t convert_ms_to_ticks(cy_time_t timeout_ms);

#ifdef __cplusplus
} // extern "C"
#endif
