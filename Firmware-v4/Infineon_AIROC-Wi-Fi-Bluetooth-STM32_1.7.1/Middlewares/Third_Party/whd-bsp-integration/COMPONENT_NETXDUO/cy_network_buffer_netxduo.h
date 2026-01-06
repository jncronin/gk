/***********************************************************************************************//**
 * \file cy_network_buffer_netxduo.h
 *
 * \brief
 * NetX Duo specific APIs for dealing with network packet buffers. This is used by WHD
 * for relaying data between the network stack and the connectivity chip.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2023 Cypress Semiconductor Corporation (an Infineon company) or
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

/**
 * \addtogroup group_bsp_network_buffer Buffer management
 * \{
 * NetX Duo specific APIs for dealing with network packet buffers
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cy_result.h"
#include "whd.h"
#include "whd_network_types.h"

#include "nx_api.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** Function for allocating a dynamic packet */
typedef NX_PACKET* (* cy_dynamic_buffer_allocate_t)(uint16_t payload_size);

/** Function for releasing a dynamic packet */
typedef void (* cy_dynamic_buffer_free_t)(NX_PACKET* packet);

/** Enable dynamic buffers for IOCTLs requiring large buffers
 *
 *  Set the functions used to enable the allocation of dynamic buffers.
 *
 *  NetX Duo uses fixed sized buffers in the packet pool implementation. Some
 *  WHD IOCTL calls require buffers with payloads larger than the MAX_MTU size
 *  used for the packet pools. This API allows functions to be set which will
 *  allocate and free dynamic buffers as needed to support the large buffer IOCTLs.
 *
 *  NOTE: This API is specific to the NetX Duo implementation.
 *
 *  @param buffer_allocate  : Pointer to the function for allocating dynamic packets
 *  @param buffer_free      : Pointer to the function for releasing dynamic packets
 *
 *  @return          : CY_RSLT_SUCCESS or WHD_BADARG
 */
whd_result_t cy_buffer_enable_dynamic_buffers(cy_dynamic_buffer_allocate_t buffer_allocate,
                                              cy_dynamic_buffer_free_t buffer_free);


#ifdef __cplusplus
}
#endif // __cplusplus

/** \} group_bsp_network_buffer */
