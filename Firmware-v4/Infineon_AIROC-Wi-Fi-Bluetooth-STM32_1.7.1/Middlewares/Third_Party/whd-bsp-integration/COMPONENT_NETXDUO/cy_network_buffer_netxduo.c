/***********************************************************************************************//**
 * \file cy_network_buffer_netxduo.c
 *
 * \brief
 * Basic set of APIs for dealing with network packet buffers. This is used by WHD
 * for relaying data between the network stack and the connectivity chip.
 *
 ***************************************************************************************************
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
 **************************************************************************************************/

#include "nx_api.h"
#include "nx_packet.h"
#include "whd.h"
#include "whd_types_int.h"
#include "whd_network_types.h"
#include "cyabs_rtos.h"
#include "cy_network_buffer.h"
#include "cy_network_buffer_netxduo.h"
#include "cy_utils.h"

/******************************************************
*             Constants
******************************************************/

#ifdef COMPONENT_CAT5
/* WHD removes the entire added header before releasing the packet */
#define HOST_BUFFER_RELEASE_REMOVE_AT_FRONT_BUS_HEADER_SIZE    \
    (0)
#else
#define HOST_BUFFER_RELEASE_REMOVE_AT_FRONT_BUS_HEADER_SIZE    \
    (sizeof(whd_buffer_header_t) + BDC_HEADER_WITH_PAD + SDPCM_HEADER)
#endif
#define HOST_BUFFER_RELEASE_REMOVE_AT_FRONT_FULL_SIZE          \
    (HOST_BUFFER_RELEASE_REMOVE_AT_FRONT_BUS_HEADER_SIZE + WHD_ETHERNET_SIZE)

#define NX_TIMEOUT(timeout_ms)   \
    ((timeout_ms == CY_RTOS_NEVER_TIMEOUT) ? NX_WAIT_FOREVER : ((ULONG)(timeout_ms * TX_TIMER_TICKS_PER_SECOND / 1000)))

#define BLOCK_SIZE_ALIGNMENT                    (64)

static NX_PACKET_POOL* rx_pool;
static NX_PACKET_POOL* tx_pool;

static cy_dynamic_buffer_allocate_t cy_buffer_allocate;
static cy_dynamic_buffer_free_t cy_buffer_free;

#ifndef COMPONENT_CAT5
//--------------------------------------------------------------------------------------------------
// cy_buffer_free_dynamic_packet
//--------------------------------------------------------------------------------------------------
void cy_buffer_free_dynamic_packet(NX_PACKET* packet)
{
    if (packet != NULL)
    {
        free(packet);
    }
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_allocate_dynamic_packet
//--------------------------------------------------------------------------------------------------
static NX_PACKET* cy_buffer_allocate_dynamic_packet(uint16_t payload_size)
{
    NX_PACKET* packet;
    ULONG header_size;
    UINT total_size;

    /*
     * Allocate a dynamic packet to satisfy a request for a payload size that is larger than
     * the size in the packet pool.
     *
     * NOTE: This API is only used for WHD communications to support IOVARS with payloads
     * larger than WHD_LINK_MTU. The nx_packet_pool_owner pointer in the packet is left
     * as NULL. When the packet is released in cy_buffer_release, the NULL nx_packet_pool_owner
     * pointer will trigger a direct free rather than trying to release back to the
     * packet pool.
     *
     * Packets allocated via this API should never be passed to the IP stack as they
     * can not be released properly by the stack since they do not belong to a packet pool.
     */

    total_size = (payload_size + BLOCK_SIZE_ALIGNMENT + sizeof(NX_PACKET) + 1);

    packet = calloc(1, total_size);
    if (packet)
    {
        /*
         * Initialize the packet structure elements that are needed for use.
         */

        header_size =
            (ULONG)(((sizeof(NX_PACKET) + NX_PACKET_ALIGNMENT - 1) / NX_PACKET_ALIGNMENT) *
                    NX_PACKET_ALIGNMENT);
        packet->nx_packet_data_start  = (UCHAR*)((uint32_t)packet + header_size);
        packet->nx_packet_data_end    = (UCHAR*)((uint32_t)packet + header_size + payload_size);
        packet->nx_packet_prepend_ptr = packet->nx_packet_data_start;
        packet->nx_packet_append_ptr  =
            (UCHAR*)((uint32_t)packet->nx_packet_prepend_ptr + payload_size);
        packet->nx_packet_length      = payload_size;
    }

    return packet;
}


#endif // ifndef COMPONENT_CAT5

//--------------------------------------------------------------------------------------------------
// cy_buffer_pool_init
//--------------------------------------------------------------------------------------------------
whd_result_t cy_buffer_pool_init(void* tx_packet_pool, void* rx_packet_pool)
{
    if ((tx_packet_pool == NULL) || (rx_packet_pool == NULL))
    {
        return WHD_BADARG;
    }

    tx_pool = (NX_PACKET_POOL*)tx_packet_pool;
    rx_pool = (NX_PACKET_POOL*)rx_packet_pool;

    /*
     * Make sure the pools are valid.
     */

    if ((tx_pool->nx_packet_pool_id != NX_PACKET_POOL_ID) ||
        (rx_pool->nx_packet_pool_id != NX_PACKET_POOL_ID))
    {
        rx_pool = NULL;
        tx_pool = NULL;

        return WHD_BADARG;
    }

    #ifndef COMPONENT_CAT5
    /*
     * Set default dynamic buffer allocate/free routines for the non-CAT5 environment.
     * Void cast of functions are to avoid compiler warnings of unused functions.
     */

    (void)cy_buffer_allocate_dynamic_packet;
    (void)cy_buffer_free_dynamic_packet;
    cy_buffer_allocate = cy_buffer_allocate_dynamic_packet;
    cy_buffer_free     = cy_buffer_free_dynamic_packet;
    #endif

    return WHD_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// cy_host_buffer_get
//--------------------------------------------------------------------------------------------------
whd_result_t cy_host_buffer_get(whd_buffer_t* buffer, whd_buffer_dir_t direction,
                                uint16_t size, uint32_t timeout_ms)
{
    UINT status = NX_NO_PACKET;
    NX_PACKET** nx_buffer = (NX_PACKET**)buffer;
    NX_PACKET_POOL* pool;

    if (buffer == NULL)
    {
        return WHD_BADARG;
    }

    pool = (direction == WHD_NETWORK_TX) ? tx_pool : rx_pool;
    if (pool == NULL)
    {
        return WHD_BADARG;
    }

    if (size > pool->nx_packet_pool_payload_size)
    {
        /*
         * Request for a packet with a payload larger than MTU.
         * Try to create a dynamically allocated packet to satisfy the request.
         */
        if (cy_buffer_allocate != NULL)
        {
            *nx_buffer = cy_buffer_allocate(size);
        }
        else
        {
            *nx_buffer = NULL;
        }

        return (*nx_buffer != NULL) ? WHD_SUCCESS : WHD_BUFFER_UNAVAILABLE_PERMANENT;
    }

    /*
     * Packets allocated via this API are only for WHD communications so we don't
     * need to reserve header space when allocating the packet.
     */

    status = nx_packet_allocate(pool, nx_buffer, 0, NX_TIMEOUT(timeout_ms));

    if (status != NX_SUCCESS)
    {
        /* No packet available from the pool */
        return WHD_BUFFER_ALLOC_FAIL;
    }

    (*nx_buffer)->nx_packet_length = size;
    (*nx_buffer)->nx_packet_append_ptr = (*nx_buffer)->nx_packet_prepend_ptr + size;

    return WHD_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_release
//--------------------------------------------------------------------------------------------------
void cy_buffer_release(whd_buffer_t buffer, whd_buffer_dir_t direction)
{
    NX_PACKET* nx_buffer = (NX_PACKET*)buffer;

    CY_ASSERT(nx_buffer != NULL);

    if (nx_buffer->nx_packet_pool_owner == NULL)
    {
        /*
         * This was a dynamically allocated packet since it is not part of a packet pool.
         */
        if (cy_buffer_free != NULL)
        {
            cy_buffer_free(nx_buffer);
        }
        else
        {
            /* Dynamically allocated packet with no way to release it */
            CY_ASSERT(0);
        }
        return;
    }

    if (direction == WHD_NETWORK_TX)
    {
        /* TCP transmit packet isn't returned immediately to the pool. The stack holds the packet
         * temporarily until ACK is received. Otherwise, the same packet is used for
         * re-transmission. Return prepend pointer to the original location which the stack
         * expects (the start of IP header). For other packets, resetting prepend pointer isn't
         * required.
         */

        if (nx_buffer->nx_packet_length > HOST_BUFFER_RELEASE_REMOVE_AT_FRONT_FULL_SIZE)
        {
            if (cy_buffer_add_remove_at_front(&buffer,
                                              HOST_BUFFER_RELEASE_REMOVE_AT_FRONT_FULL_SIZE) !=
                WHD_SUCCESS)
            {
                /* Could not move packet pointer - this shouldn't happen normally */
                CY_ASSERT(0);
            }
        }

        if (NX_SUCCESS != nx_packet_transmit_release(nx_buffer))
        {
            /* Unable to release packet back to the packet pool. Packet will be leaked */
            CY_ASSERT(0);
        }
    }
    else
    {
        if (NX_SUCCESS != nx_packet_release(nx_buffer))
        {
            /* Unable to release packet back to the packet pool. Packet will be leaked */
            CY_ASSERT(0);
        }
    }
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_get_current_piece_data_pointer
//--------------------------------------------------------------------------------------------------
uint8_t* cy_buffer_get_current_piece_data_pointer(whd_buffer_t buffer)
{
    NX_PACKET* nx_buffer = (NX_PACKET*)buffer;
    CY_ASSERT(buffer != NULL);

    return nx_buffer->nx_packet_prepend_ptr;
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_get_current_piece_size
//--------------------------------------------------------------------------------------------------
uint16_t cy_buffer_get_current_piece_size(whd_buffer_t buffer)
{
    NX_PACKET* nx_buffer = (NX_PACKET*)buffer;
    CY_ASSERT(buffer != NULL);

    return (uint16_t)nx_buffer->nx_packet_length;
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_set_size
//--------------------------------------------------------------------------------------------------
whd_result_t cy_buffer_set_size(whd_buffer_t buffer, uint16_t size)
{
    NX_PACKET* nx_buffer = (NX_PACKET*)buffer;
    CY_ASSERT(buffer != NULL);

    if ((nx_buffer->nx_packet_prepend_ptr + size) > nx_buffer->nx_packet_data_end)
    {
        return WHD_BUFFER_SIZE_SET_ERROR;
    }

    nx_buffer->nx_packet_length = size;
    nx_buffer->nx_packet_append_ptr = nx_buffer->nx_packet_prepend_ptr + size;
    return WHD_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_add_remove_at_front
//--------------------------------------------------------------------------------------------------
whd_result_t cy_buffer_add_remove_at_front(whd_buffer_t* buffer, int32_t add_remove_amount)
{
    NX_PACKET** nx_buffer = (NX_PACKET**)buffer;
    UCHAR* new_start;

    if ((buffer == NULL) || (*buffer == NULL))
    {
        return WHD_BADARG;
    }

    new_start = (*nx_buffer)->nx_packet_prepend_ptr + add_remove_amount;

    if (new_start < (*nx_buffer)->nx_packet_data_start)
    {
        /* Trying to move to a location before start - not supported without buffer chaining*/
        return WHD_BUFFER_POINTER_MOVE_ERROR;
    }
    else if (new_start > (*nx_buffer)->nx_packet_data_end)
    {
        /* Trying to move to a location after end - not supported without buffer chaining */
        return WHD_BUFFER_POINTER_MOVE_ERROR;
    }
    else
    {
        (*nx_buffer)->nx_packet_prepend_ptr = new_start;
        if ((*nx_buffer)->nx_packet_append_ptr < (*nx_buffer)->nx_packet_prepend_ptr)
        {
            (*nx_buffer)->nx_packet_append_ptr = (*nx_buffer)->nx_packet_prepend_ptr;
        }
        (*nx_buffer)->nx_packet_length =
            (ULONG)((*nx_buffer)->nx_packet_length - (ULONG)add_remove_amount);
    }
    return WHD_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_enable_dynamic_buffers
//--------------------------------------------------------------------------------------------------
whd_result_t cy_buffer_enable_dynamic_buffers(cy_dynamic_buffer_allocate_t buffer_allocate,
                                              cy_dynamic_buffer_free_t buffer_free)
{
    cy_buffer_allocate = buffer_allocate;
    cy_buffer_free     = buffer_free;

    return WHD_SUCCESS;
}
