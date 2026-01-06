/***********************************************************************************************//**
 * \file cyabs_rtos_impl_cat5.h
 *
 * \brief
 * Internal definitions for RTOS abstraction layer specific to CAT5.
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2022 Cypress Semiconductor Corporation (an Infineon company) or
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "wiced_memory.h"
#include "cyhal_system.h"


/******************************************************
*                 Type Definitions
******************************************************/

// RTOS thread priority
typedef enum
{
    CY_RTOS_PRIORITY_MIN         = 0,   /**< Minumum allowable Thread priority */
    CY_RTOS_PRIORITY_LOW         = 1,   /**< A low priority Thread */
    CY_RTOS_PRIORITY_BELOWNORMAL = 2,   /**< A slightly below normal Thread priority */
    CY_RTOS_PRIORITY_NORMAL      = 3,   /**< The normal Thread priority */
    CY_RTOS_PRIORITY_ABOVENORMAL = 4,   /**< A slightly elevated Thread priority */
    CY_RTOS_PRIORITY_HIGH        = 5,   /**< A high priority Thread */
    CY_RTOS_PRIORITY_REALTIME    = 6,   /**< Realtime Thread priority */
    CY_RTOS_PRIORITY_MAX         = 7    /**< Maximum allowable Thread priority */
} cy_thread_priority_t;

/******************************************************
*                 Function Declarations
******************************************************/

UINT thread_ap_tx_thread_create(TX_THREAD* thread_ptr, CHAR* name_ptr, VOID (* entry_function)(
                                    ULONG id), ULONG entry_input,
                                VOID* stack_start, ULONG stack_size, UINT priority, UINT preempt_threshold,
                                ULONG time_slice, UINT auto_start);
UINT thread_ap_tx_thread_terminate(TX_THREAD* thread_ptr);
UINT thread_ap_tx_thread_delete(TX_THREAD* thread_ptr);
UINT thread_ap_tx_event_flags_create(TX_EVENT_FLAGS_GROUP* group_ptr, CHAR* name_ptr);
UINT thread_ap_tx_event_flags_delete(TX_EVENT_FLAGS_GROUP* group_ptr);
UINT thread_ap_tx_mutex_create(TX_MUTEX* mutex_ptr, CHAR* name_ptr, UINT inherit);
UINT thread_ap_tx_mutex_delete(TX_MUTEX* mutex_ptr);
UINT thread_ap_tx_semaphore_create(TX_SEMAPHORE* semaphore_ptr, CHAR* name_ptr,
                                   ULONG initial_count);
UINT thread_ap_tx_semaphore_delete(TX_SEMAPHORE* semaphore_ptr);

#define _tx_thread_create      thread_ap_tx_thread_create
#define _tx_thread_terminate   thread_ap_tx_thread_terminate
#define _tx_thread_delete      thread_ap_tx_thread_delete
#define _tx_event_flags_create thread_ap_tx_event_flags_create
#define _tx_event_flags_delete thread_ap_tx_event_flags_delete
#define _tx_mutex_create       thread_ap_tx_mutex_create
#define _tx_mutex_delete       thread_ap_tx_mutex_delete
#define _tx_semaphore_create   thread_ap_tx_semaphore_create
#define _tx_semaphore_delete   thread_ap_tx_semaphore_delete

#define malloc(A)              wiced_bt_get_buffer(A)
#define free(A)                wiced_bt_free_buffer(A)

//--------------------------------------------------------------------------------------------------
// _cyabs_rtos_tx_thread_interrupt_control
//--------------------------------------------------------------------------------------------------
static inline UINT _cyabs_rtos_tx_thread_interrupt_control(UINT new_posture)
{
    static UINT32 int_val = 0;

    if (new_posture == TX_INT_DISABLE)
    {
        int_val = cyhal_system_critical_section_enter();
    }
    else
    {
        cyhal_system_critical_section_exit(int_val);
    }

    return int_val;
}


#define _tx_thread_interrupt_control(A)                 _cyabs_rtos_tx_thread_interrupt_control(A)

#if defined(__cplusplus)
}
#endif
