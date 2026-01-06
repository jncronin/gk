/*******************************************************************************
* \file cybt_host_stack_platform_interface.c
*
* \brief
* This file implements the WICED BT stack porting interface.
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

#include <string.h>

#include "cyabs_rtos.h"
#include "wiced_bt_stack_platform.h"
#include "cybt_platform_hci.h"
#include "cybt_platform_task.h"
#include "cybt_platform_interface.h"
#include "cybt_platform_config.h"
#include "cybt_platform_trace.h"
#include "cybt_platform_util.h"
#include "cyhal_lptimer.h"
#include "cycfg_system.h"
#include "cybt_platform_internal.h"

/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
cy_mutex_t bt_stack_mutex;
extern wiced_bool_t bSupressProtocolTraces;
char bt_trace_buf[CYBT_TRACE_BUFFER_SIZE];

/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
extern uint8_t *cybt_platform_hci_get_buffer(hci_packet_type_t pti, uint32_t size);

/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
void host_stack_exception_handler(uint16_t code, char* msg, void* ptr)
{
    SPIF_TRACE_ERROR("[Exception] code = 0x%x, msg = %s", code, msg);
}

BTSTACK_PORTING_SECTION_BEGIN
void host_stack_mutex_lock(void * p_lock_context)
{
    cy_rtos_get_mutex(&bt_stack_mutex, CY_RTOS_NEVER_TIMEOUT);
}
BTSTACK_PORTING_SECTION_END

BTSTACK_PORTING_SECTION_BEGIN
void host_stack_mutex_unlock(void * p_lock_context)
{
    cy_rtos_set_mutex(&bt_stack_mutex);
}
BTSTACK_PORTING_SECTION_END

BTSTACK_PORTING_SECTION_BEGIN
uint8_t *host_stack_get_acl_to_lower_buffer(wiced_bt_transport_t transport, uint32_t size)
{
    uint8_t *p_bt_msg;

    p_bt_msg = cybt_platform_hci_get_buffer(HCI_PACKET_TYPE_ACL, size);

    if(NULL == p_bt_msg)
    {
        SPIF_TRACE_ERROR("get_acl_to_lower_buffer() failure ");
    }

    return (p_bt_msg);
}
BTSTACK_PORTING_SECTION_END

BTSTACK_PORTING_SECTION_BEGIN
wiced_result_t host_stack_send_acl_to_lower(wiced_bt_transport_t transport,
                                            uint8_t *p_data,
                                            uint16_t len
                                            )
{
    cybt_result_t result;
    wiced_result_t ret_val = WICED_SUCCESS;

    SPIF_TRACE_DEBUG("send_acl_to_lower(): p_data = %p, len = %d",
                     p_data,
                     len
                     );

    if(NULL == p_data || 0 == len)
    {
        SPIF_TRACE_ERROR("send_acl_to_lower(): Invalid data(%p) or length(%d)",
                         p_data,
                         len
                         );
        return WICED_ERROR;
    }

    result = cybt_platform_hci_write(HCI_PACKET_TYPE_ACL, p_data, len);
    if(CYBT_SUCCESS != result)
    {
        SPIF_TRACE_ERROR("send_acl_to_lower(): hci write failed (ret = 0x%x)",
                         result
                         );
        ret_val = WICED_ERROR;
    }

    return (ret_val);
}
BTSTACK_PORTING_SECTION_END

wiced_result_t host_stack_send_iso_to_lower(uint8_t *p_data,
                                            uint16_t len
                                            )
{
    cybt_result_t result;
    wiced_result_t ret_val = WICED_SUCCESS;
    uint8_t *p_bt_msg;

    SPIF_TRACE_DEBUG("send_iso_to_lower(): p_data = %p, len = %d",
                     p_data,
                     len
                     );

    if(NULL == p_data || 0 == len)
    {
        SPIF_TRACE_ERROR("send_iso_to_lower(): Invalid data(%p) or length(%d)",
                         p_data,
                         len
                         );
        return WICED_ERROR;
    }

    p_bt_msg = cybt_platform_hci_get_buffer(HCI_PACKET_TYPE_ISO, len);

    if(NULL == p_bt_msg)
    {
        SPIF_TRACE_ERROR("send_iso_to_lower(): get buffer failure ");
        return WICED_ERROR;
    }

    memcpy(p_bt_msg, p_data, len);

    result = cybt_platform_hci_write(HCI_PACKET_TYPE_ISO, p_bt_msg, len);

    if(CYBT_SUCCESS != result)
    {
        SPIF_TRACE_ERROR("send_iso_to_lower(): hci write failed (ret = 0x%x)",
                         result
                         );
        ret_val = WICED_ERROR;
    }

    return (ret_val);
}

wiced_result_t host_stack_send_cmd_to_lower(uint8_t *p_cmd, uint16_t cmd_len)
{
    cybt_result_t result;
    uint8_t *p_bt_msg;

    SPIF_TRACE_DEBUG("send_cmd_to_lower(): cmd = %p, cmd_len = %d",
                     p_cmd,
                     cmd_len
                     );

    if(NULL == p_cmd || 0 == cmd_len)
    {
        SPIF_TRACE_ERROR("send_cmd_to_lower(): Invalid command (%p) or length(%d)",
                         p_cmd,
                         cmd_len
                         );
        return WICED_ERROR;
    }

    p_bt_msg = cybt_platform_hci_get_buffer(HCI_PACKET_TYPE_COMMAND, cmd_len);

    if(NULL == p_bt_msg)
        return WICED_ERROR;

    memcpy(p_bt_msg, p_cmd, cmd_len);

    result = cybt_platform_hci_write(HCI_PACKET_TYPE_COMMAND, p_bt_msg, cmd_len);

    if(CYBT_SUCCESS != result)
    {
        SPIF_TRACE_ERROR("send_cmd_cmd_lower(): hci write failed (ret = 0x%x)",
                         result
                         );
        return WICED_ERROR;
    }

    return WICED_SUCCESS;
}

uint8_t *host_stack_get_sco_to_lower_buffer(uint32_t size)
{
    //NA
    return (NULL);
}

wiced_result_t host_stack_send_sco_to_lower(uint8_t* p_data, uint8_t len)
{
    //NA
    return WICED_SUCCESS;
}

void host_stack_print_trace_log(char *p_trace_buf,
                                int trace_buf_len,
                                wiced_bt_trace_type_t trace_type
                                )
{
    switch(trace_type)
    {
        case WICED_BT_TRACE_ERROR:
            STACK_TRACE_ERROR("%s", p_trace_buf);
            break;
        case WICED_BT_TRACE_WARN:
            STACK_TRACE_WARNING("%s", p_trace_buf);
            break;
        case WICED_BT_TRACE_API:
            STACK_TRACE_API("%s", p_trace_buf);
            break;
        case WICED_BT_TRACE_EVENT:
            STACK_TRACE_EVENT("%s", p_trace_buf);
            break;
        case WICED_BT_TRACE_DEBUG:
            STACK_TRACE_DEBUG("%s", p_trace_buf);
            break;
        default:
            break;
    }
}

void host_stack_platform_interface_init(void)
{
    wiced_bt_stack_platform_t platform_interface = {0};
    wiced_result_t result;
    cy_rslt_t cy_result;

    extern void bt_post_reset_cback(void);

    platform_interface.pf_exception               = host_stack_exception_handler;
    platform_interface.pf_os_malloc               = cybt_platform_malloc;
    platform_interface.pf_os_free                 = cybt_platform_free;
    platform_interface.pf_get_tick_count_64       = cybt_platform_get_tick_count_us;
    platform_interface.pf_set_next_timeout        = cybt_platform_set_next_timeout;
    platform_interface.stack_lock.p_lock_context  = NULL;
    platform_interface.stack_lock.pf_lock_func    = host_stack_mutex_lock;
    platform_interface.stack_lock.pf_unlock_func  = host_stack_mutex_unlock;
    platform_interface.pf_get_acl_to_lower_buffer = host_stack_get_acl_to_lower_buffer;
    platform_interface.pf_write_acl_to_lower      = host_stack_send_acl_to_lower;
    platform_interface.pf_write_cmd_to_lower      = host_stack_send_cmd_to_lower;
    platform_interface.pf_get_sco_to_lower_buffer = host_stack_get_sco_to_lower_buffer;
    platform_interface.pf_write_sco_to_lower      = host_stack_send_sco_to_lower;
    platform_interface.pf_write_iso_to_lower      = host_stack_send_iso_to_lower;
    platform_interface.pf_hci_trace_cback_t       = NULL;
    platform_interface.pf_debug_trace             = host_stack_print_trace_log;
    platform_interface.trace_buffer               = bt_trace_buf;
    platform_interface.trace_buffer_len           = CYBT_TRACE_BUFFER_SIZE;
    platform_interface.pf_patch_download          = PATCH_DOWNLOAD_FN;
    platform_interface.is_legacy_bless_controller = BLESS_CONTROLLER;
    memset(bt_trace_buf, 0, CYBT_TRACE_BUFFER_SIZE);

    cy_result = cy_rtos_init_mutex(&bt_stack_mutex);
    if(CY_RSLT_SUCCESS != cy_result)
    {
        SPIF_TRACE_ERROR("platform_interface_init(): lock init failed, result = 0x%x", cy_result);
    }

    result = wiced_bt_stack_platform_initialize(&platform_interface);

    if(WICED_SUCCESS != result)
    {
        SPIF_TRACE_ERROR("platform_interface_init(): failed, result = 0x%x", result);
    }
}

void host_stack_platform_interface_deinit(void)
{
    cy_rtos_deinit_mutex(&bt_stack_mutex);
}

