/*******************************************************************************
* \file cybt_platform_task.c
*
* \brief
* Provides functions for OS task communication.
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
#include "cyabs_rtos.h"
#include "cybt_platform_config.h"
#include "cybt_platform_task.h"
#include "cybt_platform_interface.h"
#include "cybt_platform_util.h"
#include "cybt_platform_trace.h"

/******************************************************************************
 *                                Constants
 ******************************************************************************/
#define NOT_IN_ISR             (false)
#define IN_ISR                 (true)

/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
#ifdef ENABLE_DEBUG_UART
extern cybt_result_t cybt_init_debug_trans_task(void);
#endif // ENABLE_DEBUG_UART
extern cybt_result_t cybt_platform_msg_to_bt_task(const uint16_t msg, bool is_from_isr);
extern cybt_result_t cybt_bttask_init(void*);
extern void cybt_bttask_deinit(void);

/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
cybt_result_t cybt_platform_task_init(void *p_arg)
{
    cybt_result_t task_result;

    task_result = cybt_bttask_init(p_arg);
    if(CYBT_SUCCESS != task_result)
        return task_result;

#ifdef ENABLE_DEBUG_UART
    cybt_init_debug_trans_task();
#endif // ENABLE_DEBUG_UART
    return CYBT_SUCCESS;
}

cybt_result_t cybt_platform_task_deinit(void)
{
    cy_rslt_t result;

    MAIN_TRACE_DEBUG("cybt_platform_task_deinit()");

    result = cybt_platform_msg_to_bt_task(BT_EVT_TASK_SHUTDOWN, NOT_IN_ISR);

    if(CY_RSLT_SUCCESS != result)
    {
        MAIN_TRACE_ERROR("task_deinit(): send queue failure (0x%x)", result);
        return CYBT_ERR_SEND_QUEUE_FAILED;
    }

    cybt_bttask_deinit();

    return CYBT_SUCCESS;
}
