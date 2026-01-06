/***********************************************************************************************//**
 * \file cyabs_rtos_impl.h
 *
 * \brief
 * Template file for internal definitions for RTOS abstraction layer.
 * Replace all TODO items with the proper values for the RTOS that is
 * being wrapped.
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

/*
 * Exclude template files except for documentation purposes.
 * This should be removed when using the template to support a new RTOS.
 */
#if defined(DOXYGEN)

// #include "TODO: RTOS HEADER"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \addtogroup group_abstraction_rtos_port RTOS Specific Types and Defines
 * \ingroup group_abstraction_rtos_common
 * \{
 * The following defines and types have values that are specific to each RTOS port.
 * The define values are specific to each RTOS. The types are simple aliases that
 * wrap RTOS specifc types. Code cannot assume anything about the values or internals
 * of any types.
 */

/******************************************************
*                 Constants
******************************************************/
// TODO: Replace these with proper values for the target RTOS
#define CY_RTOS_MIN_STACK_SIZE      300                     /**< Minimum stack size */
#define CY_RTOS_ALIGNMENT_MASK      0x00000007UL            /**< Checks for 8-bit alignment */


/******************************************************
*                 Type Definitions
******************************************************/

// TODO: Replace all priority values with values specific to the RTOS
/** RTOS thread priority.
 * Note: Depending on the RTOS and interrupt options for the device, some of these priorities may
 * end up being the same priority level in practice. Even if this happens, the relative ordering
 * of priorities is still maintained. eg:
 * MAX >= REALTIME >= HIGH >= ABOVENORMAL >= NORMAL >= BELOWNORMAL >= LOW >= MIN
 */
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

/** Alias for the RTOS specific definition of a thread handle */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_thread_t;
/** Alias for the RTOS specific argument passed to the entry function of a thread */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_thread_arg_t;
/** Alias for the RTOS specific definition of a mutex */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_mutex_t;
/** Alias for the RTOS specific definition of a semaphore */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_semaphore_t;
/** Alias for the RTOS specific definition of an event */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_event_t;
/** Alias for the RTOS specific definition of a message queue */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_queue_t;
/** Alias for the RTOS specific definition of a timer */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_timer_t;
/** Alias for the RTOS specific argument passed to the timer callback function */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_timer_callback_arg_t;
/** Alias for the RTOS specific time unit (in milliseconds) */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_time_t;
/** Alias for the RTOS specific definition of a error status */
typedef void* /* TODO: Replace with RTOS specific type*/ cy_rtos_error_t;

/** \} group_abstraction_rtos_port */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // defined(DOXYGEN)
