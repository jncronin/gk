/*******************************************************************************
* \file cybt_platform_task.c
*
* \brief
* Provides API for OS task communication setup.
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


#ifndef CYBT_PLATFORM_TASK_H
#define CYBT_PLATFORM_TASK_H

#include <stdint.h>
#include <stdbool.h>

#include "cyabs_rtos.h"

#include "cybt_result.h"

/*****************************************************************************
 *                                Macros
 *****************************************************************************/
#define BT_TASK_ID_BTU                     (0)
#define BT_TASK_NUM                        (1)
#define BT_TASK_NAME_BTU                   "CYBT_BT_Task"
#define BTU_TASK_PRIORITY                  (CY_RTOS_PRIORITY_HIGH)
#define BTU_TASK_STACK_SIZE                (0x2400)
#define CYBT_TASK_DEFAULT_POOL_SIZE        (2048)
#define CYBT_INVALID_QUEUE_UTILIZATION     (0xFF)

#define BT_EVT_STATIC_GROUP                (0x8000)
#define BT_EVT_DYNAMIC_GROUP               (0x0000)

#define BT_EVT_ANY                         (0x0)
#define BT_EVT_TIMEOUT                     (BT_EVT_STATIC_GROUP | 0x0401)
#define BT_EVT_CORE_HCI                    (BT_EVT_STATIC_GROUP | 0x0201)
#define BT_EVT_CORE_HCI_WRITE_BUF_AVAIL    (BT_EVT_STATIC_GROUP | 0x0202)
#define BT_EVT_TASK_BOOT_COMPLETES         (BT_EVT_STATIC_GROUP | 0x0601)
#define BT_EVT_TASK_SHUTDOWN               (BT_EVT_STATIC_GROUP | 0x0101)
#define BT_EVT_TASK_RESET_COMPLETE         (BT_EVT_STATIC_GROUP | 0x0102)


#define IS_BT_EVT_STATIC(e)                ( ( (e) & (BT_EVT_STATIC_GROUP) ) ? true : false )

/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/

/**
 * Task queue related declaration
 */

#define BTU_TASK_QUEUE_COUNT        (64)

/* Queue will have pointer, hence item size is sizeof(uint32_t) */
#define BTU_TASK_QUEUE_ITEM_SIZE    (sizeof(uint32_t))

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 *                           Function Declarations
 ****************************************************************************/

/**
 * Initialize Bluetooth related OS tasks.
 *
 * @param[in] p_arg: Pointer to an argument if any
 *
 * @returns  CYBT_SUCCESS
 *           CYBT_ERR_INIT_MEMPOOL_FAILED
 *           CYBT_ERR_INIT_QUEUE_FAILED
 *           CYBT_ERR_CREATE_TASK_FAILED
 */
cybt_result_t cybt_platform_task_init(void *p_arg);


/**
 * Delete Bluetooth related OS tasks.
 *
 * @returns  CYBT_SUCCESS
 *           CYBT_ERR_OUT_OF_MEMORY
 *           CYBT_ERR_SEND_QUEUE_FAILED
 */
cybt_result_t cybt_platform_task_deinit(void);


/**
 * Initialize task memory pool.
 *
 * @param[in] total_size: the request size of memory pool
 *
 * @returns  CYBT_SUCCESS
 *           CYBT_ERR_OUT_OF_MEMORY
 *
 */
cybt_result_t cybt_platform_task_mempool_init(uint32_t total_size);


/**
 * Allocate the memory block from task memory pool.
 *
 * @param[in] req_size: the request size of memory block
 *
 * @returns  the pointer of memory block
 *
 */
void *cybt_platform_task_mempool_alloc(uint32_t req_size);


/**
 * Free and return the memory block to task memory pool.
 *
 * @param[in]   p_mem_block: the pointer of memory block
 *
 * @returns     void
 *
 */
void cybt_platform_task_mempool_free(void *p_mem_block);


/**
 * Release task memory pool.
 *
 * @returns     void
 */
void cybt_platform_task_mempool_deinit(void);


/**
 * Send message to BT task.
 *
 * @param[in] p_bt_msg    : the pointer of the message
 * @param[in] is_from_isr : true if this function is called from isr context
 *                         otherwise false
 *
 * @returns  CYBT_SUCCESS
 *           CYBT_ERR_BADARG
 *           CYBT_ERR_QUEUE_ALMOST_FULL
 *           CYBT_ERR_SEND_QUEUE_FAILED
 */
cybt_result_t cybt_send_msg_to_bt_task(void *p_bt_msg,
                                       bool is_from_isr
                                       );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

