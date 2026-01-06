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
 *
 * Network helper library for FreeRTOS
 */

#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"      // NOTE: LwIP specific - ip4_addr
#include "lwip/netif.h"         // NOTE: LwIP specific - for etharp_cleanup_netif()
#include "lwip/etharp.h"        // NOTE: LwIP specific - for netif_list for use in etharp_cleanup_netif() call
#include "cy_nw_helper.h"
#include "cyabs_rtos.h"
#include "cy_network_mw_core.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


static struct netif *nw_get_our_netif(void)
{
#if defined(CYBSP_ETHERNET_CAPABLE)
    struct netif *net = (struct netif *)cy_network_get_nw_interface( CY_NETWORK_ETH_INTERFACE, 1 );
#else
    struct netif *net = (struct netif *)cy_network_get_nw_interface( CY_NETWORK_WIFI_STA_INTERFACE, 0 );
#endif
    return net;
}

void cy_nw_ip_initialize_status_change_callback(cy_nw_ip_status_change_callback_t *cb, cy_nw_ip_status_change_callback_func_t *cb_func, void *arg)
{
    /* This function is not implemented for COMPONENT_LWIP. Expectation is to use the Wi-Fi Connection Manager library (WCM) for IP status change callback */
    printf ("Not implemented for COMPONENT_LWIP. Use Wi-Fi Connection Manager WCM APIs instead\n");
    return;
}

#if LWIP_IPV4

bool cy_nw_ip_get_ipv4_address(cy_nw_ip_interface_t nw_interface, cy_nw_ip_address_t *ip_addr)
{
#if defined(CYBSP_ETHERNET_CAPABLE)
    struct netif *net = (struct netif *)cy_network_get_nw_interface( CY_NETWORK_ETH_INTERFACE, 1 );
#else
    struct netif *net = (struct netif *)cy_network_get_nw_interface( CY_NETWORK_WIFI_STA_INTERFACE, 0 );
#endif

    if (net != NULL && ip_addr != NULL)
    {
#if LWIP_IPV6 && LWIP_IPV4
        uint32_t ipv4 = net->ip_addr.u_addr.ip4.addr;
#else
        uint32_t ipv4 = net->ip_addr.addr;
#endif
        if (ipv4 != 0UL)
        {
            ip_addr->ip.v4 = ipv4;
            ip_addr->version = NW_IP_IPV4;
            return true;
        }
    }
    return false;
}

int cy_nw_str_to_ipv4(const char *ip_str, cy_nw_ip_address_t *address)
{
    ip4_addr_t addr;
    if (ip_str == NULL || address == NULL)
    {
        return -1;
    }
    
    if (ip4addr_aton(ip_str, &addr))
    {
        address->ip.v4 = addr.addr;
        address->version = NW_IP_IPV4;
        return 0;
    }
    return -1;
}
#endif //LWIP_IPV4

void cy_nw_ip_register_status_change_callback(cy_nw_ip_interface_t nw_interface, cy_nw_ip_status_change_callback_t *cb)
{
    /* This function is not implemented for COMPONENT_LWIP. Expectation is to use the Wi-Fi Connection Manager library (WCM) for IP status change callback */
    printf ("Not implemented for COMPONENT_LWIP. Use Wi-Fi Connection Manager WCM APIs instead\n");
    return;
}

void cy_nw_ip_unregister_status_change_callback(cy_nw_ip_interface_t nw_interface, cy_nw_ip_status_change_callback_t *cb)
{
    /* This function is not implemented for COMPONENT_LWIP. Expectation is to use the Wi-Fi Connection Manager library (WCM) for IP status change callback */
    printf ("Not implemented for COMPONENT_LWIP. Use Wi-Fi Connection Manager WCM APIs instead\n");
    return;
}

#if LWIP_IPV4 && LWIP_ARP
int cy_nw_host_arp_cache_clear( cy_nw_ip_interface_t iface )
{
    struct netif *netif;

    netif = nw_get_our_netif();
    if( netif != NULL)
    {
        etharp_cleanup_netif(netif);
    }
    return 0;
}

int cy_nw_host_arp_cache_get_list( cy_nw_ip_interface_t iface, cy_nw_arp_cache_entry_t *list, uint32_t count, uint32_t *filled)
{
    uint8_t index;
    struct ip4_addr *ipaddr_ptr;
    struct netif *netif_ptr;
    struct eth_addr *eth_ret_ptr;
    struct netif *netif;

    *filled = 0;

    netif = nw_get_our_netif();
    if( netif != NULL)
    {
        for (index =0 ; index < ARP_TABLE_SIZE; index++)
        {
            if (etharp_get_entry(index, &ipaddr_ptr, &netif_ptr, &eth_ret_ptr) == 1)
            {
                list[*filled].ip.version = NW_IP_IPV4;
                list[*filled].ip.ip.v4 = ipaddr_ptr->addr;
                memcpy(list[*filled].mac.mac, eth_ret_ptr->addr, sizeof(list->mac.mac));

                *filled = *filled + 1;
                if (*filled >= count)
                {
                    break;
                }
            }
            else
            {

            }
        }
        /* The query was successful */
        return 0;
    }

    return 1;
}

int cy_nw_host_send_arp_request( cy_nw_ip_interface_t ipiface, const char *ip_string)
{
    ip4_addr_t ipv4addr;
    struct netif *netif;

    ipv4addr.addr = ipaddr_addr(ip_string);
    netif = nw_get_our_netif();
    
    if( netif != NULL)
    {
        err_t err = etharp_request(netif, &ipv4addr);
        return err;
    }
    return 1;
}
#endif /* LWIP_IPV4 && LWIP_ARP */

uint32_t cy_nw_get_time (void)
{
    cy_time_t systime;
    cy_rtos_get_time(&systime);
    return systime;
}

#ifdef __cplusplus
}
#endif
