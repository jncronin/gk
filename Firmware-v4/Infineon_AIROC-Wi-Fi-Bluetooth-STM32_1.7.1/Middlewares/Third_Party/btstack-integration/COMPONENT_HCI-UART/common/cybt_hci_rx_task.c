/*******************************************************************************
* \file cybt_hci_rx_task.c
*
* \brief
* Implement HCI task which handles HCI packet reception.
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
#include "cybt_platform_config.h"
#include "cybt_platform_util.h"

#include "wiced_bt_stack_platform.h"

/******************************************************************************
 *                                Constants
 ******************************************************************************/

#define HCI_MAX_READ_PACKET_NUM_PER_ROUND   (10)

#define CHECK_IF_HCI_RX_IND_IS_VALID(ind) \
            if(BT_IND_BASE > (ind) || BT_IND_END <= (ind)) \
            { \
                return CYBT_ERR_BADARG; \
            }

/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/


/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/

uint8_t                 hci_rx_pending_inds = 0;


/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
extern void cybt_core_stack_init(void);
extern void host_stack_platform_interface_deinit(void);


/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/

cybt_result_t cybt_send_msg_to_hci_rx_task(bt_task_ind_t bt_ind_msg,
                                                         bool is_from_isr
                                                        )
{
    cy_rslt_t result;

    CHECK_IF_HCI_RX_IND_IS_VALID(bt_ind_msg);

    result = cy_rtos_put_queue(&HCI_RX_TASK_QUEUE, (void *) &bt_ind_msg, 0, is_from_isr);

    if(CY_RSLT_SUCCESS != result)
    {
        cybt_platform_disable_irq();
        hci_rx_pending_inds |= (1 << (bt_ind_msg & BT_IND_ID_MASK));
        cybt_platform_enable_irq();

        return CYBT_ERR_SEND_QUEUE_FAILED;
    }

    return CYBT_SUCCESS;
}

void handle_hci_rx_event(void)
{
    cybt_result_t    result;
    uint32_t         read_len = 0;
    uint8_t          *p;
    uint8_t          *p_hci_evt_packet;
    hci_event_packet_header_t  *p_hci_evt_header;

    p_hci_evt_packet = (uint8_t *)cybt_platform_task_get_rx_mem();
    if(NULL == p_hci_evt_packet)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_event(): failed to get memory");
        return;
    }

    p = p_hci_evt_packet;
    read_len = sizeof(hci_event_packet_header_t);
    result = cybt_platform_hci_read(HCI_PACKET_TYPE_EVENT,
                                    p,
                                    &read_len,
                                    CY_RTOS_NEVER_TIMEOUT
                                   );
    if(CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_event(): read header failed (0x%x)", result);

        return;
    }

    p_hci_evt_header = (hci_event_packet_header_t *)p;
    p+= sizeof(hci_event_packet_header_t);

    read_len = p_hci_evt_header->content_length;
    result = cybt_platform_hci_read(HCI_PACKET_TYPE_EVENT,
                                    p,
                                    &read_len,
                                    CY_RTOS_NEVER_TIMEOUT
                                   );
    if(CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_event(): read payload failed (0x%x)", result);

        return;
    }

    wiced_bt_process_hci_events(p_hci_evt_packet,
                                HCIE_PREAMBLE_SIZE + p_hci_evt_header->content_length
                               );
}

void handle_hci_rx_acl(void)
{
    cybt_result_t    result;
    uint32_t         read_len = 0;
    uint8_t          *p;
    uint8_t          *p_hci_acl_packet;
    hci_acl_packet_header_t  *p_hci_acl_header;

    p_hci_acl_packet = (uint8_t *)cybt_platform_task_get_rx_mem();
    if(NULL == p_hci_acl_packet)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_acl(): failed to get memory");
        return;
    }

    p = p_hci_acl_packet;
    read_len = sizeof(hci_acl_packet_header_t);
    result = cybt_platform_hci_read(HCI_PACKET_TYPE_ACL,
                                    p,
                                    &read_len,
                                    CY_RTOS_NEVER_TIMEOUT
                                   );
    if(CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_acl(): read header failed (0x%x)", result);

        return;
    }

    p_hci_acl_header = (hci_acl_packet_header_t *)p;
    p+= sizeof(hci_acl_packet_header_t);

    read_len = p_hci_acl_header->content_length;
    result = cybt_platform_hci_read(HCI_PACKET_TYPE_ACL,
                                    p,
                                    &read_len,
                                    CY_RTOS_NEVER_TIMEOUT
                                   );
    if(CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_acl(): read payload failed (0x%x)", result);

        return;
    }

    wiced_bt_process_acl_data(p_hci_acl_packet,
                              HCI_DATA_PREAMBLE_SIZE + p_hci_acl_header->content_length
                             );
}

void handle_hci_rx_sco(void)
{
    cybt_result_t    result;
    uint32_t         read_len = 0;
    uint8_t          *p;
    uint8_t          *p_hci_sco_packet;
    hci_sco_packet_header_t  *p_hci_sco_header;

    p_hci_sco_packet = (uint8_t *)cybt_platform_task_get_rx_mem();
    if(NULL == p_hci_sco_packet)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_sco(): failed to get memory");
        return;
    }

    p = p_hci_sco_packet;
    read_len = sizeof(hci_sco_packet_header_t);
    result = cybt_platform_hci_read(HCI_PACKET_TYPE_SCO,
                                    p,
                                    &read_len,
                                    CY_RTOS_NEVER_TIMEOUT
                                   );
    if(CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_sco(): read header failed (0x%x)", result);

        return;
    }

    p_hci_sco_header = (hci_sco_packet_header_t *)p;
    p+= sizeof(hci_sco_packet_header_t);

    read_len = p_hci_sco_header->content_length;
    result = cybt_platform_hci_read(HCI_PACKET_TYPE_SCO,
                                    p,
                                    &read_len,
                                    CY_RTOS_NEVER_TIMEOUT
                                   );
    if(CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_rx_sco(): read payload failed (0x%x)", result);

        return;
    }

    wiced_bt_process_sco_data(p_hci_sco_packet,
                              HCI_DATA_PREAMBLE_SIZE + p_hci_sco_header->content_length
                             );
}

#ifdef ENABLE_DEBUG_UART
void handle_hci_diag(void)
{
#define EDR_LMP_RECV 1
#define EDR_LMP_XMIT 0
#define HCI_PACKET_TYPE_LMP_RECV 8
#define HCI_PACKET_TYPE_LMP_XMIT 9

    uint32_t read_len = 63; // fixed LMP len
    uint8_t output_buf[64];
    cybt_result_t result;

    result = cybt_platform_hci_read(HCI_PACKET_TYPE_DIAG,
        (uint8_t*)& output_buf,
        &read_len,
        CY_RTOS_NEVER_TIMEOUT
    );

    if (CYBT_SUCCESS != result)
    {
        HCIRXTASK_TRACE_ERROR("handle_hci_diag(): read failed (0x%x)", result);
        return;
    }

    if (output_buf[0] == EDR_LMP_RECV)
        cybt_debug_uart_send_hci_trace(HCI_PACKET_TYPE_LMP_RECV, read_len - 1, &output_buf[1]);
    else if (output_buf[0] == EDR_LMP_XMIT)
        cybt_debug_uart_send_hci_trace(HCI_PACKET_TYPE_LMP_XMIT, read_len - 1, &output_buf[1]);
    else
        return;
}
#endif // ENABLE_DEBUG_UART

void handle_hci_rx_data_ready(hci_packet_type_t hci_packet_type)
{
    uint32_t read_len = 0;
    cybt_result_t result;
    const cybt_platform_config_t *p_bt_platform_cfg = cybt_platform_get_config();

    if(CYBT_HCI_UART == p_bt_platform_cfg->hci_config.hci_transport)
    {
        uint8_t count = 0;

        for(; count < HCI_MAX_READ_PACKET_NUM_PER_ROUND; count++)
        {
            read_len = 1;
            result = cybt_platform_hci_read(HCI_PACKET_TYPE_IGNORE,
                                            (uint8_t *) &hci_packet_type,
                                            &read_len,
                                            0
                                           );
            if(CYBT_SUCCESS != result || 0 == read_len)
            {
                // No data is read from UART FIFO
                return;
            }

            switch(hci_packet_type)
            {
                case HCI_PACKET_TYPE_ACL:
                    handle_hci_rx_acl();
                    break;
                case HCI_PACKET_TYPE_EVENT:
                    handle_hci_rx_event();
                    break;
                case HCI_PACKET_TYPE_SCO:
                    handle_hci_rx_sco();
                    break;
#ifdef ENABLE_DEBUG_UART
                case HCI_PACKET_TYPE_DIAG:
                    handle_hci_diag();
                    break;
#endif // ENABLE_DEBUG_UART
                default:
                    HCIRXTASK_TRACE_ERROR("handle_hci_rx_data_ready(): unknown type (0x%02x)",
                                          hci_packet_type
                                         );
                break;
            }
        }
    }
}

void handle_task_shutdown(void)
{
    bt_task_ind_t  bt_ind_msg;

    while(CY_RSLT_SUCCESS == cy_rtos_get_queue(&HCI_RX_TASK_QUEUE,
                                               (void *)&bt_ind_msg,
                                               0,
                                               false
                                              )
         );

    cy_rtos_deinit_queue(&HCI_RX_TASK_QUEUE);
    wiced_bt_stack_shutdown();
    host_stack_platform_interface_deinit();
}

void handle_pending_inds(void)
{
    cy_rslt_t      result;
    uint8_t  offset = 0;
    bt_task_ind_t  bt_ind_msg;

    for(offset = 0; offset < BT_IND_TOTAL_NUM; offset++)
    {
        if(0 == (hci_rx_pending_inds & (1 << offset)))
        {
            continue;
        }

        bt_ind_msg = BT_IND_BASE + offset;
        result = cy_rtos_put_queue(&HCI_RX_TASK_QUEUE, (void *) &bt_ind_msg, 0, false);
        if(CY_RSLT_SUCCESS == result)
        {
            hci_rx_pending_inds &= ~(1 << offset);
        }
        else
        {
            HCIRXTASK_TRACE_ERROR("handle_pending_inds(): failed to put queue (0x%x)",
                                  result
                                 );
            break;
        }
    }
}

void cybt_hci_rx_task(cy_thread_arg_t arg)
{
    bt_task_ind_t  bt_ind_msg;
    cy_rslt_t      result;

    cybt_core_stack_init();

    while(1)
    {
        bt_ind_msg = BT_IND_INVALID;

        result = cy_rtos_get_queue(&HCI_RX_TASK_QUEUE,
                                   (void *)&bt_ind_msg,
                                   CY_RTOS_NEVER_TIMEOUT,
                                   false
                                  );
        if(CY_RSLT_SUCCESS != result || BT_IND_INVALID == bt_ind_msg)
        {
            HCIRXTASK_TRACE_WARNING("hci_rx_task(): queue error (0x%x), msg = 0x%x",
                                    result,
                                    bt_ind_msg
                                   );
            continue;
        }

        /** If there are any pending indications, which means put queue was failed
         *  at occuring time, handle this here.
         */
        if(0 != hci_rx_pending_inds)
        {
            cybt_platform_disable_irq();
            handle_pending_inds();
            cybt_platform_enable_irq();
        }

        if(BT_IND_TASK_SHUTDOWN == bt_ind_msg)
        {
            handle_task_shutdown();
            break;
        }

        switch(bt_ind_msg)
        {
            case BT_IND_TO_HCI_DATA_READY_UNKNOWN:
                handle_hci_rx_data_ready(HCI_PACKET_TYPE_IGNORE);
                cybt_platform_hci_irq_rx_data_ind(true);
                break;
            case BT_IND_TO_BTS_TIMER:
                wiced_bt_process_timer();
                break;
            case BT_IND_TO_HCI_DATA_READY_ACL:
                handle_hci_rx_acl();
                break;
            case BT_IND_TO_HCI_DATA_READY_SCO:
                handle_hci_rx_sco();
                break;
            case BT_IND_TO_HCI_DATA_READY_EVT:
                handle_hci_rx_event();
                break;
            default:
                HCIRXTASK_TRACE_ERROR("hci_rx_task(): Unknown message (0x%x)",
                                      bt_ind_msg
                                     );
                break;
        }
    }

    cy_rtos_exit_thread();
}

