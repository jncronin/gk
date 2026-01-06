/*
 * Copyright 2019-2024, Cypress Semiconductor Corporation (an Infineon company) or
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
 * 
 * This is a collection of network helper functions which would be used by various Cypress Middleware libraries.
 * 
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>


#if defined(__cplusplus)
extern "C" {
#endif

/** \addtogroup nwhelper_utils 
 * This is a collection of network helper functions to fetch IPv4 address of the local device, notify IPv4 address 
 * change via callback and conversion utilities.
 */

#define cy_assert( error_string, assertion )         do { (void)(assertion); } while(0)

typedef uintptr_t cy_nw_ip_interface_t;


/******************************************************
 *                   Enumerations
 ******************************************************/
/******************************************************************************/
/** \addtogroup group_nwhelper_enums 
 * This provides the documentation of all the enums provided by this utility.
 *//** \{ */
/******************************************************************************/
/** IP version */
typedef enum nw_ip_version
{
    NW_IP_IPV4 = 4,          /**< IPv4 version */
    NW_IP_IPV6 = 6,          /**< IPv6 version */
    NW_IP_INVALID_IP = (-1), /**< Invalid IP version */
} cy_nw_ip_version_t;

/** Network interface type */
typedef enum
{
    CY_NW_INF_TYPE_WIFI = 0, /**< Wi-Fi network interface */
    CY_NW_INF_TYPE_ETH       /**< Ethernet network interface */
} cy_network_interface_type_t;

/** \} */

/******************************************************************************/
/** \addtogroup group_nwhelper_structures 
 * Lists all the data structures and typedefs provided with the network helper utility along with the documentation.
 *//** \{ */
/******************************************************************************/
/** Network IP status change callback function
 *
 * @param[in] iface : Pointer to the network interface for which the callback is invoked.
 * @param[in] arg   : User data object provided during the status change callback registration.

 * @return none
 */
typedef void (cy_nw_ip_status_change_callback_func_t)(cy_nw_ip_interface_t iface, void *arg);

/** Network IP status change callback info */
typedef struct cy_nw_ip_status_change_callback
{
    cy_nw_ip_status_change_callback_func_t *cb_func; /**< IP address status change callback function */
    void *arg;                                    /**< User data */
    void *priv;                                   /**< NW interface */
} cy_nw_ip_status_change_callback_t;

/**
 * IP addr info
 */
typedef struct cy_nw_ip_address
{
    cy_nw_ip_version_t version; /**< IP version */

    union
    {
        uint32_t v4;         /**< IPv4 address info */
        uint32_t v6[4];      /**< IPv6 address info */
    } ip;                    /**< Union of IPv4 and IPv6 address info */
} cy_nw_ip_address_t;

/** Network interface object */
typedef void* cy_network_interface_object_t;

/** MAC Address info */
typedef struct cy_nw_ip_mac
{
    uint8_t    mac[6];              /**< MAC address                */
} cy_nw_ip_mac_t;

/** ARP Cache Entry info */
typedef struct cy_nw_arp_cache_entry
{
    cy_nw_ip_address_t    ip;         /**< IP address                 */
    cy_nw_ip_mac_t        mac;        /**< MAC address                */
} cy_nw_arp_cache_entry_t;

/**
 * Network interface info structure
 */
typedef struct
{
    cy_network_interface_type_t     type;     /**< Network interface type */
    cy_network_interface_object_t   object;   /**< Pointer to the network interface object */
} cy_network_interface_t;

/** \} */

/*****************************************************************************/
/**
 *
 *  @addtogroup group_nwhelper_func
 *
 * This is a collection of network helper functions which would be used by various Cypress Middleware libraries.
 *
 *  @{
 */
/*****************************************************************************/

/** Initialize status change callback
 *
 * Initialize @ref cy_nw_ip_status_change_callback_t instead of
 * directly manipulating the callback struct.
 *
 * @param[in, out] info : Pointer to network IP status change callback information structure which would be filled upon return
 * @param[in] cbf       : Pointer to callback function to be invoked during network status change
 * @param[in] arg       : User data object to be sent sent in the callback function
 *
 * @return none
 */
void cy_nw_ip_initialize_status_change_callback(cy_nw_ip_status_change_callback_t *info, cy_nw_ip_status_change_callback_func_t *cbf, void *arg);

/** Retrieves the IPv4 address for an interface
 *
 * Retrieves the IPv4 address for an interface (AP or STA) if it
 * exists.
 *
 * @param[in]  iface         : Pointer to network interface object
 * @param[out] addr          : Pointer to the IP information sturcture in which the results to be stored
 *
 * @return true  : if IP address is present
 * @return false : otherwise
 */
bool cy_nw_ip_get_ipv4_address(cy_nw_ip_interface_t iface, cy_nw_ip_address_t *addr);

/** Convert IPv4 string to an IP address structure.
 *
 * @param[in]  ip_str  :  IPv4 address string.
 * @param[out] address :  Pointer to the IP info structure in which the IPv4 address to be stored
 *
 * @return    0 : if successful
 * @return   -1 : if failed
 */
int cy_nw_str_to_ipv4(const char *ip_str, cy_nw_ip_address_t *address);

/** Registers for callback function to be invoked during IP status change
 *
 * @param[in] iface : Pointer to network interface object
 * @param[in] info  : Pointer to the status change information structure
 *
 * @return none
 */
void cy_nw_ip_register_status_change_callback(cy_nw_ip_interface_t iface, cy_nw_ip_status_change_callback_t *info);

/** Un-registers IP status change callback
 *
 * @param[in] iface : Pointer to network interface object
 * @param[in] info  : Pointer to the status change information structure
 *
 * @return none
 */
void cy_nw_ip_unregister_status_change_callback(cy_nw_ip_interface_t iface, cy_nw_ip_status_change_callback_t *info);

/** Clears the ARP cache for the interface
 * NOTE: in LwIP, we need the netif (NetworkInterface) to do things, we can find using wifi interface.
 *
 * @param[in] iface         : Pointer to network interface object
 *
 *  @return 0 : success
 *          1 : fail
 */
int cy_nw_host_arp_cache_clear( cy_nw_ip_interface_t iface );

/** Gets the ARP cache list for the interface
 *
 * @param[in]      iface         : Pointer to network interface object
 * @param[in, out] list          : Pointer to @ref cy_nw_arp_cache_entry_t array
 * @param[in]      count         : Number of entries in the array passed in `list`
 * @param[in, out] filled        : Pointer to get the number of entries filled in the array pointed by 'list'
 *
 * @return  0 : success
 *          1 : fail
 */
int cy_nw_host_arp_cache_get_list( cy_nw_ip_interface_t iface, cy_nw_arp_cache_entry_t *list, uint32_t count, uint32_t *filled );

/** Send ARP request
 * NOTE: in LwIP, we need the netif (NetworkInterface) to do things, we can find using wifi interface.
 *
 * @param[in]  iface         : Pointer to network interface object
 * @param[in]  ip_string     : Pointer to the IPv4 address string (Ex: "192.168.1.1") to which the ARP request to be sent
 *
 * @return 0 : success
 *         1 : failed to send ARP request
 */
int cy_nw_host_send_arp_request( cy_nw_ip_interface_t iface, const char *ip_string );

/** GET time in milliseconds
 *
 * @return time in milliseconds
 * */
uint32_t cy_nw_get_time (void);

/** GET IPv4 address in decimal notation.
 *
 * NOTE: dotted-decimal notation example (192.168.0.1)
 *
 * @param[in]  char_ptr : Pointer to the string containing the IPv4 address in dotted-decimal format.
 * @param[out] addr     : Pointer to the structure containing IPv4 address in decimal format.
 *
 * @return 0 : success
 *         1 : failed
 * */
bool cy_nw_aton (const char *char_ptr , cy_nw_ip_address_t *addr);

/** GET IPv6 address in decimal notation.
 * NOTE: This API does not support shorthand representation of IPv6 address. Input string should be of the format X:X:X:X:X:X:X:X.
 *
 * @param[in]  char_ptr : Pointer to the string containing the IPv6 address.
 * @param[out] addr     : Pointer to the structure containing IPv6 address in decimal format.
 *
 * @return 0 : success
 *         1 : failed
 * */
bool cy_nw_aton_ipv6(const char *char_ptr , cy_nw_ip_address_t *addr);

/** GET IPv4 address in string format.
 * 
 * @param[in]  addr     : Pointer to IPv4 address structure containing the IPv4 address.
 * @param[out] ip_str   : Pointer to the string containing IPv4 address in dotted-decimal notation.
 *                        ip_str must be 16 bytes long.
 * @return 0 : success
 *         1 : failed
 * */
bool cy_nw_ntoa (cy_nw_ip_address_t *addr, char *ip_str);

/** GET IPv6 address in string format.
 *
 * @param[in]  addr     : Pointer to IPv6 address structure containing the IPv6 address.
 * @param[out] ip_str   : Pointer to the string containing IPv6 address.
 *                        ip_str must 39 bytes long.
 * @return 0 : success
 *         1 : failed
 * */
bool cy_nw_ntoa_ipv6 (cy_nw_ip_address_t *addr, char *ip_str);

/** @} */

#if defined(__cplusplus)
}
#endif
