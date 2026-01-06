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

/**
* \addtogroup group_netxduo_network_interface_integration netxduo-network-interface-integration
* \{
* Functions for dealing with linking the NetXDuo TCP/IP stack with Wi-Fi Host Driver and ethernet driver
*
* \defgroup group_netxduo_network_interface_integration_enums Enumerated Types
* \defgroup group_netxduo_network_interface_integration_structures Port Structures
* \defgroup group_netxduo_network_interface_integration_functions Port Functions
*/

#pragma once

#include "cy_result.h"
#include "cy_nw_helper.h"
#include "cy_nw_mw_core_error.h"
#include "nx_api.h"

#ifdef __cplusplus
extern "C" {
#endif
/** \cond INTERNAL */
#define CY_NETWORK_PRIMARY_INTERFACE    (0)
#define NX_TIMEOUT(timeout_ms)          ((timeout_ms != CY_RTOS_NEVER_TIMEOUT) ? ((ULONG)(timeout_ms * TX_TIMER_TICKS_PER_SECOND / 1000)) : NX_WAIT_FOREVER)
/** \endcond */

/**
 * \addtogroup group_netxduo_network_interface_integration_macros
 * \{
 */

/******************************************************
 *                    Constants
 ******************************************************/
/**
 * Suppress unused variable warning
 */
#ifndef UNUSED_VARIABLE
#define UNUSED_VARIABLE(x) ( (void)(x) )
#endif

#define CY_MAC_ADDR_LEN                        (6U)         /**< MAC address length */

/** \} group_netxduo_network_interface_integration_macros */

/**
 * \addtogroup group_netxduo_network_interface_integration_enums
 * \{
 */

/**
 * Enumeration of network interface role.
 */
typedef enum
{
    CY_NETWORK_WIFI_STA_INTERFACE    = 0,  /**< STA interface  */
    CY_NETWORK_WIFI_AP_INTERFACE     = 1,  /**< SoftAP Interface */
    CY_NETWORK_ETH_INTERFACE         = 2   /**< Ethernet Interface */
} cy_network_hw_interface_type_t;
/**
 * Enumeration of network activity type
 */
typedef enum
{
    CY_NETWORK_ACTIVITY_TX = 0,  /**< TX network activity  */
    CY_NETWORK_ACTIVITY_RX = 1   /**< RX network activity  */
} cy_network_activity_type_t;

/**
* IPV6 types
*/
typedef enum
{
    CY_NETWORK_IPV6_LINK_LOCAL = 0, /**< Denotes IPv6 link-local address type. */
    CY_NETWORK_IPV6_GLOBAL          /**< Denotes IPv6 global address type. */
} cy_network_ipv6_type_t;

/** \} group_netxduo_network_interface_integration_enums */

/**
* \addtogroup group_netxduo_network_interface_integration_structures
* \{
*/

/**
 * Interface context structure to store the hardware and
 * network interface structures.
 */
typedef struct
{
    cy_network_hw_interface_type_t  iface_type;     /**< Interface type */
    uint8_t                         iface_idx;      /**< Ethernet Interface index */
    void                           *hw_interface;  /**< HW interface handle. Caller should map it to the HW interface specific structure */
    void                           *nw_interface;  /**< Network interface. Caller should map it to the network stack specific interface structure */
    bool                            is_initialized; /**< Network interface is initialized or not*/
    uint8_t                         mac_address[CY_MAC_ADDR_LEN]; /**< Mac address of the device to be configured to NetXDuo stack */
} cy_network_interface_context;

/**
 * This structure represents a static IP address
 */
typedef struct cy_network_static_ip_addr
{
    cy_nw_ip_address_t addr;    /**< The IP address for the network interface */
    cy_nw_ip_address_t netmask; /**< The netmask for the network interface */
    cy_nw_ip_address_t gateway; /**< The default gateway for network traffic */
} cy_network_static_ip_addr_t;

/**
 * This structure contains information about the packet pool
 */
typedef struct cy_network_packet_pool_info
{
    ULONG total_packets;              /**< Number of total packets in this pool               */
    ULONG free_packets;               /**< Number of free packets in this pool                */
    ULONG empty_pool_requests;        /**< Number of empty pool requests made in this pool    */
    ULONG empty_pool_suspensions;     /**< Number of empty pool suspensions made in this pool */
    ULONG invalid_packet_releases;    /**< Number of invalid packet releases made in this pool*/
} cy_network_packet_pool_info_t;

/** \} group_netxduo_network_interface_integration_structures */

/**
 * Enumeration of network packet types
 */
typedef enum
{
    CY_NETWORK_IP_PACKET   = 0,     /**< IP network packet type   */
    CY_NETWORK_TCP_PACKET  = 1,     /**< TCP network packet type  */
    CY_NETWORK_UDP_PACKET  = 2,     /**< UDP network packet type  */
    CY_NETWORK_ICMP_PACKET = 4      /**< ICMP network packet type */
} cy_network_packet_type_t;

/**
 * Indicates transmit/receive direction for the packet buffer
 */
typedef enum
{
    CY_NETWORK_PACKET_TX,           /**< Transmit direction */
    CY_NETWORK_PACKET_RX            /**< Receive direction  */
} cy_network_packet_dir_t;

/**
* \addtogroup group_netxduo_network_interface_integration_functions
* \{
*/
/**
 * Initializes the network stack. Also allocates and initializes
 * the resources needed by this library.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_init();

/**
 * De-initializes the network stack. Also releases the resources
 * allocated in \ref cy_network_init function.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_deinit();

/**
 * Adds a network interface. Binds hardware interface to IP layer of the network stack.
 *
 * @param[in]  iface_type      Network interface type \ref cy_network_hw_interface_type_t.
 * @param[in]  iface_idx       Network interface index. If cy_network_add_nw_interface function is called multiple times for same interface type,
 *                             for the first call iface_idx should be zero, and incremented by one for sub-sequent calls. This is used to handle multiple ethernet interfaces.
 * @param[in]  hw_interface    Handle to the already initialized HW interface.
 *                             For example, whd_interface_t pointer that is initialized using cybsp_wifi_init_primary() function. Similarly for ethernet it can be ethernet interface pointer.
 * @param[in]  mac_address     MAC address to be configured to the network interface
 * @param[in]  ipaddr          Static IP address provided by the user.
 * @param[out] iface_context   Interface context. Network core library allocates memory for this structure and initializes it. Caller should not modify the contents of this structure. And should not free the memory. To free the memory allocated by this functions caller should invoke \ref cy_network_remove_nw_interface function.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_add_nw_interface(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx,
                                      void *hw_interface, uint8_t *mac_address,
                                      cy_network_static_ip_addr_t *ipaddr, cy_network_interface_context **iface_context);

/**
 * Removes a network interface. Unbinds network interface from IP layer of the network stack.
 *
 * @param[in] iface_context   Interface context that has been created with \ref cy_network_add_nw_interface function.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_remove_nw_interface(cy_network_interface_context *iface_context);

/**
 * Return the network interface for the given network interface type.
 *
 * @param[in] iface_type      Network interface type.
 * @param[in] iface_idx       Network interface index.
 *
 * @return Associated network stack's net interface structure for the HW interface.
 */
void* cy_network_get_nw_interface(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx);

/**
 * This function brings up the network link layer and
 * starts DHCP if required
 *
 * @param[in] iface_context   Interface context that has been created with \ref cy_network_add_nw_interface function.
 *
 *  @return CY_RSLT_SUCCESS on successful network bring up, failure code otherwise.
 */
cy_rslt_t cy_network_ip_up(cy_network_interface_context *iface_context);

/**
 * This function brings down the network interface, brings down the network link layer
 * and stops DHCP
 *
 * @param[in] iface_context   Interface context that has been created with \ref cy_network_add_nw_interface function.
 *
 * @return CY_RSLT_SUCCESS on successful tear down of the network, failure code otherwise.
 */
cy_rslt_t cy_network_ip_down(cy_network_interface_context *iface_context);

/** \cond INTERNAL*/
/**
 * This function returns the DHCP handle.
 *
 * @param[in] iface_type      Network interface type.
 * @param[in] iface_idx       Network interface index.
 *
 * @return Returns a valid DHCP handle or NULL if it is not initialized.
 */
void *cy_network_get_dhcp_handle(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx);
/** \endcond */

/**
 * This function invalidates all the ARP entries and renews the DHCP.
 *
 * @param[in] iface_context   Interface context that has been created with \ref cy_network_add_nw_interface function.
 *
 * @return CY_RSLT_SUCCESS on successful network bring up, failure code otherwise.
 */
cy_rslt_t cy_network_dhcp_renew(cy_network_interface_context *iface_context);

/**
 * IP change callback function prototype
 * Callback function which can be registered to receive IP changes
 * needs to be of this prototype.
 *
 * @param[in] iface_context   Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[in] user_data       User data provided at the time of callback registration.
 */
typedef void (*cy_network_ip_change_callback_t)(cy_network_interface_context *iface_context, void *user_data);

/**
 * This function helps to register/unregister for any IP changes from network stack.
 * Passing "NULL" as callback will deregister the IP changes callback
 *
 * @param[in] iface_context   Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[in] cb              IP change callback function.
 * @param[in] user_data       User data that is passed to the callback function.
 *
 */
void cy_network_register_ip_change_cb(cy_network_interface_context *iface_context, cy_network_ip_change_callback_t cb, void *user_data);

/**
 * Network activity callback function prototype
 * Callback function which can be registered/unregistered for any network activity
 * needs to be of this prototype
 */
typedef void (*cy_network_activity_event_callback_t)(bool event_type);

/**
 * This function helps to register/unregister callback function that will be invoked on any TX/RX activity on any of the network interfaces.
 * Passing "NULL" as cb will deregister the activity callback
 *
 * @param[in] cb Network activity callback function
 *
 */
void cy_network_activity_register_cb(cy_network_activity_event_callback_t cb);

/**
 * This function notifies network activity to Low Power Assistant(LPA) module.
 *
 * @param[in] activity_type Network activity type \ref cy_network_activity_type_t to be notified to Low Power Assistant(LPA) module.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_activity_notify(cy_network_activity_type_t activity_type);

/**
 * Retrieves the IPv4 address of the given interface. See \ref cy_network_get_ipv6_address API to get IPv6 addresses.
 *
 * @param[in]   iface_context  : Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[out]  ip_addr        : Pointer to an IP address structure of type cy_nw_ip_address_t.
 *
 * @return CY_RSLT_SUCCESS if IP-address get is successful; failure code otherwise.

 */
cy_rslt_t cy_network_get_ip_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *ip_addr);

/**
 * Retrieves the IPv6 address of the given interface.
 *
 * @param[in]   iface_context  : Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[in]   type           : IPv6 address type. Only IPv6 link-local address type is supported.
 * @param[out]  ip_addr        : Pointer to an IP address structure of type cy_nw_ip_address_t.
 *
 * @return CY_RSLT_SUCCESS if IP-address get is successful; failure code otherwise.

 */
cy_rslt_t cy_network_get_ipv6_address(cy_network_interface_context *iface_context, cy_network_ipv6_type_t type, cy_nw_ip_address_t *ip_addr);

/**
 * Retrieves the gateway IP address of the given interface.
 *
 * @param[in]   iface_context  : Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[out]  gateway_addr   : Pointer to an IP address structure of type cy_nw_ip_address_t to be filled with the gateway IP address.
 *
 * @return CY_RSLT_SUCCESS if retrieval of the gateway IP address was successful; failure code otherwise.

 */
cy_rslt_t cy_network_get_gateway_ip_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *gateway_addr);

/**
 * Retrieves the MAC address of the gateway for the given interface. Uses Address Resolution Protocol (ARP) to retrieve the gateway MAC address.
 *
 * This function is a blocking call and uses an internal timeout while running ARP.
 *
 * @param[in]   iface_context  : Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[out]  mac_addr       : Pointer to a MAC address structure which is filled with the gateway's MAC address on successful return.
 *
 * @return CY_RSLT_SUCCESS if retrieval of the gateway MAC address was successful; failure code otherwise.
 */
cy_rslt_t cy_network_get_gateway_mac_address(cy_network_interface_context *iface_context, cy_nw_ip_mac_t *mac_addr);

/**
 * Retrieves the subnet mask address of the given interface.
 *
 * @param[in]   iface_context  : Interface context that has been created with \ref cy_network_add_nw_interface function.
 * @param[out]  net_mask_addr  : Pointer to an IP address structure of type cy_nw_ip_address_t to be filled with the subnet mask address of the interface.
 *
 * @return CY_RSLT_SUCCESS if retrieval of the subnet mask address was successful; failure code otherwise.
 */
cy_rslt_t cy_network_get_netmask_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *net_mask_addr);

/** Sends a ping request to the given IP address.
 *
 * @param[in]  if_ctx           : Pointer to network interface context(typecasted to void *).
 * @param[in]  address          : Pointer to the destination IP address structure to which the ping request will be sent.
 * @param[in]  timeout_ms       : Ping request timeout in milliseconds.
 * @param[in]  elapsed_time_ms  : Pointer to store the round-trip time (in milliseconds),
 *                                i.e., the time taken to receive the ping response from the destination.
 * @return CY_RSLT_SUCCESS if successful, CY_RSLT_NETWORK_ERROR_PING if failure
 * */
cy_rslt_t cy_network_ping(cy_network_interface_context *if_ctx, cy_nw_ip_address_t *address, uint32_t timeout_ms, uint32_t* elapsed_time_ms);

/**
 * This function performs allocates a NetXDuo packet.
 *
 * @param[in]    packet_type  Type of packet to allocate (IP, TCP, UDP, or ICMP)
 * @param[in]    timeout      Allocation timeout in milliseconds
 * @param[inout] packet       Pointer to the pointer of the allocated packet pointer.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_get_packet(cy_network_packet_type_t packet_type, uint32_t timeout, void **packet);

/**
 * This function returns a packet pool for the given packet direction.
 *
 * @param[in]    direction    Indicates transmit/receive direction of the packet pool.
 * @param[inout] pool         Pointer to the pointer of the pool.
 *
 * @return CY_RSLT_SUCCESS if successful, failure code otherwise.
 */
cy_rslt_t cy_network_get_packet_pool(cy_network_packet_dir_t direction, void **pool);

/** \cond INTERNAL */
cy_rslt_t cy_network_get_hostbyname(cy_network_hw_interface_type_t iface_type, unsigned char *hostname, uint32_t lookup_type, uint32_t timeout, void *ipaddr);

/**
 * This function returns available packet pool size for the given packet direction.
 *
 * @param[in]     direction    Indicates transmit/receive direction of the packet pool.
 * @param[in/out] pool_info    Pointer to packet pool info structure of type \ref cy_network_packet_pool_info
 *
 */
void cy_network_get_packet_pool_info(cy_network_packet_dir_t direction, cy_network_packet_pool_info_t *pool_info);

/** \endcond */

#if defined(COMPONENT_CAT1)
/**
 * Random number generate using PDL trng APIs
 *
 * @param[out] output    : Data to fill
 * @param[in]  len       : Maximum size to provide
 * @param[out] olen      : The actual amount of bytes put into the buffer (Can be 0)
 *
 * @return  CY_RSLT_SUCCESS if successful; CY_RSLT_NETWORK_ERROR_TRNG if failure.
 */
cy_rslt_t cy_network_random_number_generate( unsigned char *output, size_t len, size_t *olen );
#endif
/** \} group_netxduo_network_interface_integration_functions */
#ifdef __cplusplus
}
#endif
/** \} group_netxduo_network_interface_integration */
