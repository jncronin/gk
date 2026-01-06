/*******************************************************************************
* \file cybt_hci_tx_task.c
*
* \brief
* Implement HCI task which handles HCI packet transmission.
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

#include <stdint.h>
#include <stdlib.h>

#include "hcidefs.h"

#include "cybt_platform_task.h"
#include "cybt_platform_hci.h"
#include "cybt_platform_interface.h"
#include "cybt_platform_trace.h"

#include "wiced_bt_stack_platform.h"


/******************************************************************************
 *                                Constants
 ******************************************************************************/


/******************************************************************************
 *                           Type Definitions
 ******************************************************************************/


/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
cybt_hci_tx_status_t  hci_tx_status = CYBT_HCI_TX_NORMAL;


/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
cybt_hci_tx_status_t cybt_get_hci_tx_status(void)
{
    return hci_tx_status;
}

void cybt_lock_hci_tx(cybt_hci_tx_status_t reason)
{
    switch(reason)
    {
        case CYBT_HCI_TX_BLOCKED_HEAP_RAN_OUT:
        case CYBT_HCI_TX_BLOCKED_QUEUE_FULL_CMD:
        case CYBT_HCI_TX_BLOCKED_QUEUE_FULL_ACL:
            hci_tx_status |= reason;
            break;
        default:
            HCITXTASK_TRACE_ERROR("cybt_lock_hci_tx(): unknown reason (%d)",
                                reason
                               );
            break;
    }
}

bool cybt_check_and_resume_hci_tx(void)
{
    if(CYBT_HCI_TX_NORMAL == hci_tx_status)
    {
        return false;
    }
    else
    {
        uint16_t largest_free_fragment_size = 0;
        uint8_t  cur_heap_usage = cybt_platform_task_get_tx_heap_utilization(&largest_free_fragment_size);
        uint8_t cur_q_usage = cybt_platform_task_get_queue_utilization(BT_TASK_ID_HCI_TX);

        BT_MSG_HDR  *p_msg_hdr = NULL;
        bool pending_hci_cmd = (hci_tx_status & CYBT_HCI_TX_BLOCKED_QUEUE_FULL_CMD)? true: false;

        if(CYBT_INVALID_HEAP_UTILIZATION == cur_heap_usage
           || CYBT_INVALID_QUEUE_UTILIZATION == cur_q_usage
          )
        {
            HCITXTASK_TRACE_ERROR("cybt_check_and_resume_hci_tx(): get heap/queue info failed, heap %d%%, queue %d%%",
                                  cur_heap_usage,
                                  cur_q_usage
                                 );
            return false;
        }

        if(HCI_TX_UNLOCK_THRESHOLD_TX_HEAP_IN_PERCENT >= cur_heap_usage
           && HCI_TX_UNLOCK_THRESHOLD_HCITX_Q_IN_PERCENT >= cur_q_usage
          )
        {
            HCITXTASK_TRACE_ERROR("cybt_check_and_resume_hci_tx(): Resume hci tx. TX heap (%d%% < %d%%), queue (%d%% < %d%%)",
                                  cur_heap_usage,
                                  HCI_TX_UNLOCK_THRESHOLD_TX_HEAP_IN_PERCENT,
                                  cur_q_usage,
                                  HCI_TX_UNLOCK_THRESHOLD_HCITX_Q_IN_PERCENT
                                 );

            hci_tx_status = CYBT_HCI_TX_NORMAL;

            if(true == pending_hci_cmd)
            {
                p_msg_hdr = (BT_MSG_HDR *) cybt_platform_task_get_tx_cmd_mem();
                if(p_msg_hdr
                   && (BT_EVT_TO_HCI_COMMAND == p_msg_hdr->event)
                   && (0 != p_msg_hdr->length)
                  )
                {
                    // There is pending hci command, send it now
                    HCITXTASK_TRACE_ERROR("cybt_check_and_resume_hci_tx(): send HCI command");
                    cy_rtos_put_queue(&HCI_TX_TASK_QUEUE, (void *) &p_msg_hdr, 0, false);
                }
            }

            wiced_bt_stack_indicate_lower_tx_complete();
            return true;
        }
        else
        {
            HCITXTASK_TRACE_ERROR("cybt_check_and_resume_hci_tx(): Blocked, TX heap (now: %d%%, cri: %d%%), queue (now: %d%%, cri: %d%%)",
                                  cur_heap_usage,
                                  HCI_TX_UNLOCK_THRESHOLD_TX_HEAP_IN_PERCENT,
                                  cur_q_usage,
                                  HCI_TX_UNLOCK_THRESHOLD_HCITX_Q_IN_PERCENT
                                 );
            return false;
        }
    }
}

cybt_result_t cybt_send_msg_to_hci_tx_task(BT_MSG_HDR *p_bt_msg,
                                                         bool is_from_isr
                                                        )
{
    cy_rslt_t result;

    if(NULL == p_bt_msg)
    {
        return CYBT_ERR_BADARG;
    }

    result = cy_rtos_put_queue(&HCI_TX_TASK_QUEUE, (void *) &p_bt_msg, 0, is_from_isr);

    if(CY_RSLT_SUCCESS != result)
    {
        if(false == is_from_isr)
        {
            HCITXTASK_TRACE_ERROR("send_msg_to_hci_tx_task(): send failure (0x%x)",
                                  result
                                 );
        }

        return CYBT_ERR_SEND_QUEUE_FAILED;
    }

    return CYBT_SUCCESS;
}

void handle_hci_tx_command(BT_MSG_HDR *p_bt_msg)
{
    uint8_t *p_hci_payload;
    cybt_result_t result;

    if(NULL == p_bt_msg)
    {
        HCITXTASK_TRACE_ERROR("handle_hci_tx_command(): Invalid task msg");
        return;
    }

    HCITXTASK_TRACE_DEBUG("handle_hci_tx_command(): msg = 0x%x, len = %d",
                          p_bt_msg,
                          p_bt_msg->length
                         );

    p_hci_payload = (uint8_t *)(p_bt_msg + 1);
    result = cybt_platform_hci_write(HCI_PACKET_TYPE_COMMAND,
                                     p_hci_payload,
                                     p_bt_msg->length
                                    );
    if(CYBT_SUCCESS != result)
    {
        HCITXTASK_TRACE_ERROR("handle_hci_tx_command(): hci write failed (0x%x)",
                              result
                             );
    }
}

void handle_hci_tx_acl(BT_MSG_HDR *p_bt_msg)
{
    uint8_t *p_hci_payload;
    cybt_result_t result;

    if(NULL == p_bt_msg)
    {
        HCITXTASK_TRACE_ERROR("handle_hci_tx_acl(): Invalid task msg");
        return;
    }

    HCITXTASK_TRACE_DEBUG("handle_hci_tx_acl(): msg = 0x%x, len = %d",
                          p_bt_msg,
                          p_bt_msg->length
                         );

    p_hci_payload = (uint8_t *)(p_bt_msg + 1);
    result = cybt_platform_hci_write(HCI_PACKET_TYPE_ACL,
                                     p_hci_payload,
                                     p_bt_msg->length
                                    );
    if(CYBT_SUCCESS != result)
    {
        HCITXTASK_TRACE_ERROR("handle_hci_tx_acl(): hci write failed (0x%x)",
                              result
                             );
    }

    cybt_platform_task_mempool_free((void *) p_bt_msg);
}

void handle_hci_tx_sco(BT_MSG_HDR *p_bt_msg)
{
    uint8_t *p_hci_payload;
    cybt_result_t result;

    if(NULL == p_bt_msg)
    {
        HCITXTASK_TRACE_ERROR("handle_hci_tx_sco(): Invalid task msg");
        return;
    }

    HCITXTASK_TRACE_DEBUG("handle_hci_tx_sco(): msg = 0x%x, len = %d",
                          p_bt_msg,
                          p_bt_msg->length
                         );

    p_hci_payload = (uint8_t *)(p_bt_msg + 1);
    result = cybt_platform_hci_write(HCI_PACKET_TYPE_SCO,
                                     p_hci_payload,
                                     p_bt_msg->length
                                    );
    if(CYBT_SUCCESS != result)
    {
        HCITXTASK_TRACE_ERROR("handle_hci_tx_sco(): hci write failed (0x%x)",
                              result
                             );
    }

    cybt_platform_task_mempool_free((void *) p_bt_msg);
}

void cybt_hci_tx_task(cy_thread_arg_t arg)
{
    HCITXTASK_TRACE_DEBUG("hci_tx_task(): start");

    hci_tx_status = CYBT_HCI_TX_NORMAL;

    cybt_platform_hci_open(NULL);

    while(1)
    {
        cy_rslt_t  result;
        BT_MSG_HDR *p_bt_msg = NULL;

        result = cy_rtos_get_queue(&HCI_TX_TASK_QUEUE,
                                   (void *)&p_bt_msg,
                                   CY_RTOS_NEVER_TIMEOUT,
                                   false
                                  );

        if(CY_RSLT_SUCCESS != result || NULL == p_bt_msg)
        {
            HCITXTASK_TRACE_WARNING("hci_tx_task(): queue error (0x%x)", result);
            continue;
        }

        if(BT_IND_TASK_SHUTDOWN == (uint32_t)p_bt_msg)
        {
            while(CY_RSLT_SUCCESS == cy_rtos_get_queue(&HCI_TX_TASK_QUEUE,
                                                       (void *)&p_bt_msg,
                                                       0,
                                                       false
                                                      )
                 )
            {
                if(BT_IND_TASK_SHUTDOWN != (uint32_t)p_bt_msg)
                {
                    cybt_platform_task_mempool_free((void *) p_bt_msg);
                }
            }
            cy_rtos_deinit_queue(&HCI_TX_TASK_QUEUE);

            break;
        }

        switch(p_bt_msg->event)
        {
            case BT_EVT_TO_HCI_COMMAND:
                handle_hci_tx_command(p_bt_msg);
                break;
            case BT_EVT_TO_HCI_ACL:
                handle_hci_tx_acl(p_bt_msg);
                break;
            case BT_EVT_TO_HCI_SCO:
                handle_hci_tx_sco(p_bt_msg);
                break;
            default:
                HCITXTASK_TRACE_ERROR("hci_tx_task(): Unknown event (0x%x)", p_bt_msg->event);
                cybt_platform_task_mempool_free((void *) p_bt_msg);
                break;
        }

        cybt_check_and_resume_hci_tx();
    }

    cybt_platform_hci_close();
    cy_rtos_exit_thread();
}

