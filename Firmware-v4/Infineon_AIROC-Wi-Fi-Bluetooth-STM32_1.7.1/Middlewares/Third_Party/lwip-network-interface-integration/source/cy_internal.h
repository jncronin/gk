/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
*/

#ifndef LIBS_CONNECTIVITY_MANAGER_INTERNAL_HEADER_H_
#define LIBS_CONNECTIVITY_MANAGER_INTERNAL_HEADER_H_

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1
#include "cy_ethif.h"
#include "cy_ephy.h"

/******************************************************
 *                      Macros
 ******************************************************/
/**
 * Maximum number of interface instances supported.
 */
#define CY_IFACE_MAX_HANDLE    (4U)
#define NO_OF_BUFFERS          (40)
#define NO_OF_MEMPOOL_ELEMENTS (50)
#define MEM_BYTE_ALIGNMENT     (32)
/******************************************************
 *                      Type definitions
 ******************************************************/
/** Buffer created from the pool */
typedef void* cy_buffer_t;

CY_ALIGN(32)
typedef struct cy_rx_buffer_info
{
    uint8_t *rx_data_ptr;  /* Pointer to the actual buffer created */
    uint32_t eth_idx;
    uint32_t length;
    uint32_t reserved[5];
} cy_rx_buffer_info_t;

CY_ALIGN(32)
typedef struct list_node_t
{
    void *pool_handle; /* Stores the pool handle */
    struct list_node_t *next; /* Points to the next buffer in the list */
    uint8_t *buffer_ptr; /* Pointer to the actual buffer created */
    uint32_t reserved[5];
} list_node_t;

typedef struct
{
    uint16_t total_num_buf_created; /* Total number of buffers created */
    uint16_t sizeof_buffer; /* Size of each created buffer */
    uint8_t *data_buffer; /* Base address of the data buffer */
    list_node_t *head; /* Pointer to the head node in the buffer list */
} cy_internal_buffer_pool_t;

typedef struct rx_buf_info
{
    uint32_t rx_data_ptr; /* Pointer to the actual buffer created */
    uint32_t length;
} rx_buf_info_t;

typedef struct my_custom_pbuf
{
   struct pbuf_custom pbuf_custom;
   cy_rx_buffer_info_t *pbuffer;
} my_custom_pbuf_t;

#endif
#endif

#endif /* LIBS_CONNECTIVITY_MANAGER_INTERNAL_HEADER_H_ */
