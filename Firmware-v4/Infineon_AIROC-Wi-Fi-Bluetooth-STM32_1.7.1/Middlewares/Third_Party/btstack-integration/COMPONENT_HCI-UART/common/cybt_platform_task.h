/*******************************************************************************
* \file cybt_platform_task.c
*
* \brief
* Provides API for OS task communication setup.
*
********************************************************************************
* \copyright
* Copyright 2018-2021 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
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
*******************************************************************************/


#ifndef CYBT_PLATFORM_TASK_H
#define CYBT_PLATFORM_TASK_H

#include <stdint.h>
#include <stdbool.h>

#include "cyabs_rtos.h"

#include "cybt_result.h"

/*****************************************************************************
 *                                Constants
 *****************************************************************************/
#define BT_TASK_ID_HCI_RX                   (0)
#define BT_TASK_ID_HCI_TX                   (1)
#define BT_TASK_NUM                         (2)

#define BT_TASK_NAME_HCI_RX                 "CYBT_HCI_RX_Task"
#define BT_TASK_NAME_HCI_TX                 "CYBT_HCI_TX_Task"

#define HCI_TX_TASK_PRIORITY                (CY_RTOS_PRIORITY_HIGH)
#define HCI_RX_TASK_PRIORITY                (CY_RTOS_PRIORITY_HIGH)

#define HCI_RX_TASK_STACK_SIZE              (0x1400)
#define HCI_TX_TASK_STACK_SIZE              (0x1000)

#define CYBT_INVALID_HEAP_UTILIZATION       (0xFF)
#define CYBT_INVALID_QUEUE_UTILIZATION      (0xFF)

#define HCI_TX_UNLOCK_THRESHOLD_TX_HEAP_IN_PERCENT    (50)
#define HCI_TX_UNLOCK_THRESHOLD_HCITX_Q_IN_PERCENT    (50)

/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/

/**
 * BT event (with message header and payload)
 */
#define BT_EVT_TO_BTD_EVENT                 (0x0201)
#define BT_EVT_TO_BTD_ACL                   (0x0202)
#define BT_EVT_TO_BTD_SCO                   (0x0203)

#define BT_EVT_TO_HCI_COMMAND               (0x0401)
#define BT_EVT_TO_HCI_ACL                   (0x0402)
#define BT_EVT_TO_HCI_SCO                   (0x0403)
#define BT_EVT_INVALID                      (0xFFFF)
typedef uint16_t bt_task_event_t;

/**
 * BT indication (without header/payload)
 */
#define BT_IND_BASE                         (0xDADBF000)

#define OFFSET_TASK_SHUTDOWN                (0)
#define OFFSET_DATA_READY_UNKNOWN           (1)
#define OFFSET_DATA_READY_ACL               (2)
#define OFFSET_DATA_READY_SCO               (3)
#define OFFSET_DATA_READY_EVT               (4)
#define OFFSET_TIMER                        (5)
#define BT_IND_TOTAL_NUM                    (6)

#define BT_IND_ID_MASK                      (0x00000FFF)

#define BT_IND_TASK_SHUTDOWN                (BT_IND_BASE + OFFSET_TASK_SHUTDOWN)
#define BT_IND_TO_HCI_DATA_READY_UNKNOWN    (BT_IND_BASE + OFFSET_DATA_READY_UNKNOWN)
#define BT_IND_TO_HCI_DATA_READY_ACL        (BT_IND_BASE + OFFSET_DATA_READY_ACL)
#define BT_IND_TO_HCI_DATA_READY_SCO        (BT_IND_BASE + OFFSET_DATA_READY_SCO)
#define BT_IND_TO_HCI_DATA_READY_EVT        (BT_IND_BASE + OFFSET_DATA_READY_EVT)
#define BT_IND_TO_BTS_TIMER                 (BT_IND_BASE + OFFSET_TIMER)
#define BT_IND_END                          (BT_IND_BASE + BT_IND_TOTAL_NUM)

#define BT_IND_INVALID                      (0xFFFFFFFF)
typedef uint32_t bt_task_ind_t;

/**
 * Message structure is used to communicate between tasks
 */
typedef struct
{
    bt_task_event_t  event;    /**< event id */
    uint16_t         length;   /**< payload length */
} BT_MSG_HDR;

/**
 * Message header size
 */
#define BT_MSG_HDR_SIZE             (sizeof(BT_MSG_HDR))

#define HCI_EVT_MSG_HDR_SIZE        (BT_MSG_HDR_SIZE + HCIE_PREAMBLE_SIZE)
#define HCI_ACL_MSG_HDR_SIZE        (BT_MSG_HDR_SIZE + HCI_DATA_PREAMBLE_SIZE)
#define HCI_SCO_MSG_HDR_SIZE        (BT_MSG_HDR_SIZE + HCI_SCO_PREAMBLE_SIZE)


extern cy_queue_t  cybt_task_queue[];

/**
 * Task queue related declaration
 */
#define HCI_RX_TASK_QUEUE            (cybt_task_queue[BT_TASK_ID_HCI_RX])
#define HCI_TX_TASK_QUEUE            (cybt_task_queue[BT_TASK_ID_HCI_TX])

#define HCI_RX_TASK_QUEUE_COUNT      (32)
#define HCI_TX_TASK_QUEUE_COUNT      (32)
#define HCI_RX_TASK_QUEUE_ITEM_SIZE  (sizeof(bt_task_ind_t))
#define HCI_TX_TASK_QUEUE_ITEM_SIZE  (sizeof(BT_MSG_HDR *))

#define CYBT_HCI_TX_NORMAL                    (0)
#define CYBT_HCI_TX_BLOCKED_HEAP_RAN_OUT      (1 << 0)
#define CYBT_HCI_TX_BLOCKED_QUEUE_FULL_CMD    (1 << 1)
#define CYBT_HCI_TX_BLOCKED_QUEUE_FULL_ACL    (1 << 2)
typedef uint8_t cybt_hci_tx_status_t;

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
 * @param[in] p_arg: pointer to an argument if any
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
void *cybt_platform_task_tx_mempool_alloc(uint32_t req_size);


/**
 * Get the start address of pre-allocated memory block for HC transmitting packet.
 *
 * @returns  the pointer of memory block
 *
 */
void *cybt_platform_task_get_tx_cmd_mem(void);


/**
 * Get the start address of pre-allocated memory block for HCI receiving packet.
 *
 * @returns  the pointer of memory block
 *
 */
void *cybt_platform_task_get_rx_mem(void);


/**
 * Free and return the memory block to task tx/rx memory pool.
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
 * Send message to HCI RX task.
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
cybt_result_t cybt_send_msg_to_hci_rx_task(bt_task_ind_t bt_ind_msg,
                                                         bool is_from_isr
                                                        );


/**
 * Send message to HCI TX task.
 *
 * @param[in] p_bt_msg    : the pointer of the message
 * @param[in] is_from_isr : true if this function is called from isr context
 *                         otherwise false
 *
 * @returns  CYBT_SUCCESS
 *           CYBT_ERR_BADARG
 *           CYBT_ERR_SEND_QUEUE_FAILED
 */
cybt_result_t cybt_send_msg_to_hci_tx_task(BT_MSG_HDR *p_bt_msg,
                                                         bool is_from_isr
                                                        );


/**
 * Get the usage rate of task message queue.
 *
 * @param[in]  task_id: the task id to query
 *
 * @returns    the utilization in percentage
 */
uint8_t cybt_platform_task_get_queue_utilization(uint8_t task_id);


/**
 * Get the usage rate of TX heap.
 *
 * @param[out]  p_largest_free_size: the size of largest free block in TX heap currently
 *
 * @returns    the utilization in percentage
 */
uint8_t cybt_platform_task_get_tx_heap_utilization(uint16_t *p_largest_free_size);


/**
 * Get hci tx task status
 *
 * @returns  CYBT_HCI_TX_NORMAL
 *           CYBT_HCI_TX_BLOCKED_TX_HEAP_RAN_OUT
 *           CYBT_HCI_TX_BLOCKED_HCI_TX_QUEUE_FULL

 */
cybt_hci_tx_status_t cybt_get_hci_tx_status(void);


/**
 * Lock hci tx task to stop transmission.
 *
 * @param[out]  reason: the reason to lock hci tx
 *
 * @returns    none
 */
void cybt_lock_hci_tx(cybt_hci_tx_status_t reason);


/**
 * Termiate HCI-TX task.
 *
 * @returns     void
 */
void cybt_platform_terminate_hci_tx_thread(void);


/**
 * Termiate HCI-RX task.
 *
 * @returns     void
 */
void cybt_platform_terminate_hci_rx_thread(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

