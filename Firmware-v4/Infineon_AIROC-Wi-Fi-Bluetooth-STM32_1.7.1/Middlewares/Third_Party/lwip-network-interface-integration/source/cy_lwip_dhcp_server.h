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

/** @file
 *  Interface header for a simple DHCP server
 */

#pragma once

#if LWIP_IPV4


#include "cy_network_mw_core.h"
#include "cy_worker_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE_PRE
//#define ALWAYS_INLINE    __attribute__((always_inline))
/* Fixme: ALWAYS_INLINE should be set based on compiler GCC or IAR. but for
 * now it is forced inline is removed. Need to identify how to detect compiler flag here
 */
#define ALWAYS_INLINE
#endif

#ifndef htobe32   /* This is defined in POSIX platforms */
ALWAYS_INLINE_PRE static inline ALWAYS_INLINE uint32_t htobe32(uint32_t v)
{
    return (uint32_t)(((v&0x000000FF) << 24) | ((v&0x0000FF00) << 8) | ((v&0x00FF0000) >> 8) | ((v&0xFF000000) >> 24));
}
#endif /* ifndef htobe32 */

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/**
 * IP Version
 */
typedef enum
{
    CY_LWIP_IP_VER_V4 = 4,      /**< Denotes IPv4 version. */
    CY_LWIP_IP_VER_V6 = 6       /**< Denotes IPv6 version. */
} cy_lwip_ip_version_t;


/******************************************************
 *                    Structures
 ******************************************************/

/**
 * Structure used to store the IP address information
 */
typedef struct
{
    cy_lwip_ip_version_t version;  /**< IP version. */
    union
    {
        uint32_t v4;     /**< IPv4 address in network byte order. */
        uint32_t v6[4];  /**< IPv6 address in network byte order. */
    } ip;                /**< IP address bytes. */
} cy_lwip_ip_address_t;

/**
 * Structure for storing a MAC address (Wi-Fi Media Access Control address).
 */
typedef struct
{
    uint8_t octet[6]; /**< Unique 6-byte MAC address */
} cy_lwip_mac_addr_t;

typedef struct cy_lwip_udp_socket_struct cy_lwip_udp_socket_t;

struct cy_lwip_udp_socket_struct
{
    int                              socket;
    struct netconn                  *conn_handler;
    ip_addr_t                        local_ip_addr;
    bool                             is_bound;
    cy_network_hw_interface_type_t   type;
    uint8_t                          index;
};

typedef struct
{
    cy_thread_t                  thread;
    cy_lwip_udp_socket_t         socket;
    volatile bool                quit;
} cy_lwip_dhcp_server_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
/*****************************************************************************/
/**
 *
 *                   DHCP Server
 *
 * Communication functions for DHCP server
 *
 *
 */
/*****************************************************************************/

/**
 *  Start a DHCP server instance.
 *
 * @param[in] server         Structure that will be used for this DHCP server instance allocated by the caller, @ref cy_lwip_dhcp_server_t.
 * @param[in] iface_context  Network interface the DHCP server should listen on.
 *
 * @return CY_RSLT_SUCCESS if successful; failure code otherwise.
 */
cy_rslt_t cy_lwip_dhcp_server_start(cy_lwip_dhcp_server_t* server, cy_network_interface_context *iface_context);

/**
 *  Stop a DHCP server instance.
 *
 * @param[in] server     Structure workspace for the DHCP server instance - as used with @ref cy_lwip_dhcp_server_t.
 *
 * @return CY_RSLT_SUCCESS if successful; failure code otherwise.
 */
cy_rslt_t cy_lwip_dhcp_server_stop(cy_lwip_dhcp_server_t *server);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif //LWIP_IPV4
