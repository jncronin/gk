/*******************************************************************************
* \file cybt_bt_task.c
*
* \brief
* Implement BT task which handles HCI packet from HCI task, and timer interrupt.
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

#include "stdbool.h"

#include "cyabs_rtos.h"

#include "cybt_platform_task.h"
#include "cybt_platform_hci.h"
#include "cybt_platform_trace.h"

#include "wiced_bt_stack_platform.h"
#include "cybt_platform_config.h"

/******************************************************************************
 *                                Constants
 ******************************************************************************/
#define BT_TASK_QUEUE_UTIL_HIGH_THRESHHOLD    (90)
#define DO_NOT_CLEAR                          (false)
#define CLEAR_CAUSE_EVENT                     (true)
#define WAIT_FOR_ANY                          (false)
#define WAIT_FOR_ALL                          (true)
#define NON_BLOCKING                          ((cy_time_t)0)
#define NOT_IN_ISR                            (false)
#define IN_ISR                                (true)
#define BT_TASK_EVENT                         (0x1)

/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
__attribute__((aligned(4))) uint8_t packet_buffer[1024];
static uint32_t bt_task_dropped_packet_cnt = 0;
static cy_thread_t bt_task = 0;
static cy_queue_t  bt_task_queue = 0;
static cy_event_t bt_task_event;

/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
extern void cybt_core_stack_init(void);
extern void host_stack_platform_interface_deinit(void);
extern uint16_t cybt_platform_get_event_id(void *event);
extern void cybt_platform_hci_post_stack_init(void);
extern bool cybt_platform_hci_process_if_coredump(uint8_t *p_data, uint32_t length);
extern void cybt_platform_exception_handler(cybt_exception_t error, uint8_t *info, uint32_t length);

/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/

static uint8_t task_queue_utilization(void)
{
    size_t item_cnt_in_queue = 0;
    cy_rslt_t result;

    result = cy_rtos_count_queue(&bt_task_queue, &item_cnt_in_queue);
    if(CY_RSLT_SUCCESS != result)
    {
        return CYBT_INVALID_QUEUE_UTILIZATION;
    }

    return (item_cnt_in_queue * 100 / BTU_TASK_QUEUE_COUNT);
}

cybt_result_t cybt_send_msg_to_bt_task(void *p_bt_msg,
                                       bool is_from_isr
                                       )
{
    cy_rslt_t result;
    uint8_t util_in_percentage = task_queue_utilization();

    if(NULL == p_bt_msg || bt_task_queue == 0)
    {
        return CYBT_ERR_BADARG;
    }

    if((cybt_platform_get_event_id(p_bt_msg) == BT_EVT_CORE_HCI) && (BT_TASK_QUEUE_UTIL_HIGH_THRESHHOLD < util_in_percentage))
    {
        bt_task_dropped_packet_cnt++;
        return CYBT_ERR_QUEUE_ALMOST_FULL;
    }

    result = cy_rtos_put_queue(&bt_task_queue, (void *) &p_bt_msg, 0, is_from_isr);

    if(CY_RSLT_SUCCESS != result)
    {
        bt_task_dropped_packet_cnt++;
        return CYBT_ERR_SEND_QUEUE_FAILED;
    }

    cy_rtos_setbits_event(&bt_task_event, BT_TASK_EVENT, is_from_isr);

    return CYBT_SUCCESS;
}

static void handle_hci_rx_packet(void *event)
{
    hci_packet_type_t pti;
    uint32_t length;

    if(0 == cybt_platfrom_hci_get_rx_fifo_count())
        return;

    if(CYBT_SUCCESS !=
            cybt_platform_hci_read(event, &pti, &packet_buffer[0], &length))
        return;

    switch(pti)
    {
        case HCI_PACKET_TYPE_EVENT:
            if(false == cybt_platform_hci_process_if_coredump(&packet_buffer[0], length))
            {
                /* If not core dump packet process through btstack */
                wiced_bt_process_hci_events(&packet_buffer[0], length);
            }
            break;

        case HCI_PACKET_TYPE_ACL:
            wiced_bt_process_acl_data(&packet_buffer[0], length);
            break;

        case HCI_PACKET_TYPE_SCO:
            //NA
            break;
        case HCI_PACKET_TYPE_ISO:
            wiced_bt_process_isoc_data(&packet_buffer[0], length);
            break;

#ifdef ENABLE_DEBUG_UART
        case HCI_PACKET_TYPE_DIAG:
            //sent packet to tracing uart
            break;
#endif
        default:
            break;
    }
}

void bt_task_handler(cy_thread_arg_t arg)
{
    cy_rslt_t  result;
    uint8_t is_stack_inited = 0;
    uint16_t event_id;
    size_t num_waiting;
    uint32_t wait_for;
    void* event = NULL;

    cybt_platform_hci_open(arg);

    for( ; ; )
    {
        event = NULL;

        wait_for = BT_TASK_EVENT; // or with other events if we add anything

        /* Blocking call for bt_task event */
        cy_rtos_waitbits_event(&bt_task_event, &wait_for, CLEAR_CAUSE_EVENT, WAIT_FOR_ANY, CY_RTOS_NEVER_TIMEOUT);

        if(BT_TASK_EVENT == (BT_TASK_EVENT & wait_for))
        {
            do{
                cy_rtos_count_queue(&bt_task_queue, &num_waiting);
                if( 0 == num_waiting )
                    break;

                /* Non blocking call */
                result = cy_rtos_get_queue(&bt_task_queue, &event, NON_BLOCKING, NOT_IN_ISR);

                if(CY_RSLT_SUCCESS != result || (NULL == event))
                {
                    BTTASK_TRACE_WARNING("bt_task(): event error (0x%x), event = 0x%p", result, event);
                    continue;
                }

                event_id = cybt_platform_get_event_id(event);

                if(BT_EVT_TASK_SHUTDOWN == event_id)
                {
                    BTTASK_TRACE_DEBUG("bt_task(): event:BT_EVT_TASK_SHUTDOWN");
                    cybt_platform_hci_close();
                    continue;
                }
                if (BT_EVT_TASK_RESET_COMPLETE == event_id)
                {
                    BTTASK_TRACE_DEBUG("bt_task(): event:BT_EVT_TASK_RESET_COMPLETE");
                    /* Non blocking call */
                    while(CY_RSLT_SUCCESS == cy_rtos_get_queue(&bt_task_queue, &event, NON_BLOCKING, NOT_IN_ISR ))
                    {
                    }
                    cy_rtos_deinit_queue(&bt_task_queue);
                    cy_rtos_deinit_event(&bt_task_event);
                    wiced_bt_stack_shutdown();
                    is_stack_inited = 0;
                    host_stack_platform_interface_deinit();
                    cy_rtos_exit_thread();
                    return;
                }

                switch(event_id)
                {
                    case BT_EVT_TIMEOUT:
                        wiced_bt_process_timer();
                        break;
                    case BT_EVT_CORE_HCI:
                        handle_hci_rx_packet(event);
                        break;
                    case BT_EVT_CORE_HCI_WRITE_BUF_AVAIL:
                        wiced_bt_stack_indicate_lower_tx_complete();
                        break;
                    case BT_EVT_TASK_BOOT_COMPLETES:
                        BTTASK_TRACE_DEBUG("bt_task(): controller core has been booted");
                        if(0 == is_stack_inited)
                        {
                            cybt_core_stack_init();
                            cybt_platform_hci_post_stack_init();
                            is_stack_inited = 1;
                            BTTASK_TRACE_DEBUG("bt_task(): stack initialization has been completed");
                        }
                        else
                        {
                            /* Reset might be due to controller crash. Don't deinit and init the host stack here.
                             * Let app decides this */
                            cybt_platform_exception_handler(CYBT_CONTROLLER_RESTARTED, NULL, 0);
                        }
                        break;
                    default:
                        BTTASK_TRACE_ERROR("bt_task(): Unknown event (0x%x)", event_id);
                        break;
                } //switch(*event)
            }while(1);
        }// if(BT_TASK_EVENT == (BT_TASK_EVENT & wait_for))
    } // for(;;)
}

cybt_result_t cybt_bttask_init(void* p_arg)
{
    cy_rslt_t cy_result;
    cy_result = cy_rtos_init_event(&bt_task_event);
    if(CY_RSLT_SUCCESS != cy_result)
    {
        BTTASK_TRACE_ERROR("task_init(): Init bt task event failed (0x%x)", cy_result);
        return CYBT_ERR_INIT_EVENT_FAILED;
    }

    cy_result = cy_rtos_init_queue(&bt_task_queue,
                                   BTU_TASK_QUEUE_COUNT,
                                   BTU_TASK_QUEUE_ITEM_SIZE
                                   );
    if(CY_RSLT_SUCCESS != cy_result)
    {
        BTTASK_TRACE_ERROR("task_init(): Init bt task queue failed (0x%x)", cy_result);
        return CYBT_ERR_INIT_QUEUE_FAILED;
    }

    cy_result = cy_rtos_create_thread(&bt_task,
                                      bt_task_handler,
                                      BT_TASK_NAME_BTU,
                                      NULL,
                                      BTU_TASK_STACK_SIZE,
                                      BTU_TASK_PRIORITY,
                                      p_arg
                                      );
    if(CY_RSLT_SUCCESS != cy_result)
    {
        BTTASK_TRACE_ERROR("task_init(): Create bt task failed (0x%x)", cy_result);
        return CYBT_ERR_CREATE_TASK_FAILED;
    }

    return (CYBT_SUCCESS);
}

void cybt_bttask_deinit(void)
{
    /* To clean up resources of older bt task instance.
     * If we miss below call we will end up with CY_RTOS_NO_MEMORY for next thread create
     * Below call will wait on internal semaphore and it will set on cy_rtos_exit_thread() */
    cy_rtos_join_thread(&bt_task);
}

