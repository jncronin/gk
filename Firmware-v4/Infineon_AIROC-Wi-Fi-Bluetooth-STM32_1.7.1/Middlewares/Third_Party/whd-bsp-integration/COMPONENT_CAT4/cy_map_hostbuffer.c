/***************************************************************************//**
* \file cy_map_hostbuffer.c
*
* \brief
* Implements a set of mapping functions for M2M DMA host buffer routines.
*
********************************************************************************
* \copyright
* Copyright 2021-2022 Cypress Semiconductor Corporation (an Infineon company) or
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
*******************************************************************************/

#include "wiced_osl.h"
#include "cy_network_buffer.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define HOST_BUFFER_WAIT_MS     (1000UL)

/*******************************************************************************
*       Host buffer functions
*******************************************************************************/

cy_rslt_t host_buffer_get(wiced_buffer_t* buffer, wwd_buffer_dir_t direction,
                          uint16_t size, bool wait)
{
    uint32_t timeout_ms = wait ? HOST_BUFFER_WAIT_MS : 0UL;

    return (cy_rslt_t)cy_host_buffer_get((whd_buffer_t*)buffer, (whd_buffer_dir_t)direction,
                                         size, timeout_ms);
}


//--------------------------------------------------------------------------------------------------
// host_buffer_release
//--------------------------------------------------------------------------------------------------
void host_buffer_release(wiced_buffer_t buffer, wwd_buffer_dir_t direction)
{
    cy_buffer_release((whd_buffer_t)buffer, (whd_buffer_dir_t)direction);
}


//--------------------------------------------------------------------------------------------------
// host_buffer_get_current_piece_data_pointer
//--------------------------------------------------------------------------------------------------
uint8_t* host_buffer_get_current_piece_data_pointer(wiced_buffer_t buffer)
{
    return cy_buffer_get_current_piece_data_pointer((whd_buffer_t)buffer);
}


//--------------------------------------------------------------------------------------------------
// host_buffer_get_current_piece_size
//--------------------------------------------------------------------------------------------------
uint16_t host_buffer_get_current_piece_size(wiced_buffer_t buffer)
{
    return cy_buffer_get_current_piece_size((whd_buffer_t)buffer);
}


//--------------------------------------------------------------------------------------------------
// host_buffer_set_size
//--------------------------------------------------------------------------------------------------
cy_rslt_t host_buffer_set_size(wiced_buffer_t buffer, uint16_t size)
{
    return (cy_rslt_t)cy_buffer_set_size((whd_buffer_t)buffer, size);
}


//--------------------------------------------------------------------------------------------------
// host_buffer_add_remove_at_front
//--------------------------------------------------------------------------------------------------
cy_rslt_t host_buffer_add_remove_at_front(wiced_buffer_t* buffer, int32_t add_remove_amount)
{
    return (cy_rslt_t)cy_buffer_add_remove_at_front((whd_buffer_t*)buffer, add_remove_amount);
}


#if defined(__cplusplus)
}
#endif /* __cplusplus */
