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
 * Network helper library for NetXDuo
 */

#include <ctype.h>

#include "nx_api.h"
#include "nx_arp.h"

#include "cyabs_rtos.h"
#include "cy_nw_helper.h"
#include "cy_string_utils.h"
#include "cy_network_mw_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void cy_nw_ip_initialize_status_change_callback(cy_nw_ip_status_change_callback_t *cb, cy_nw_ip_status_change_callback_func_t *cb_func, void *arg)
{
    /* This function is not implemented for COMPONENT_NETXDUO. Expectation is to use the Wi-Fi Connection Manager library (WCM) for IP status change callback */
    printf ("Not implemented for COMPONENT_NETXDUO. Use Wi-Fi Connection Manager WCM APIs instead\n");
    return;
}

#ifndef NX_DISABLE_IPV4

bool cy_nw_ip_get_ipv4_address(cy_nw_ip_interface_t nw_interface, cy_nw_ip_address_t *ip_addr)
{
    NX_IP *ip = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);
    ULONG address;
    ULONG netmask;
    UINT status;

    if (ip != NULL && ip_addr != NULL)
    {
        memset(ip_addr, 0, sizeof(*ip_addr));

        status = nx_ip_address_get(ip, &address, &netmask);
        if (status == NX_SUCCESS)
        {
            /*
             * NetXDuo uses host byte order for IP addresses in the API.
             * Convert to network byte order before returning.
             */

            ip_addr->ip.v4   = htonl(address);
            ip_addr->version = NW_IP_IPV4;
            return true;
        }
    }
    return false;
}

int cy_nw_str_to_ipv4(const char *ip_str, cy_nw_ip_address_t *address)
{
    uint32_t addr = 0;
    uint8_t num = 0;

    if (ip_str == NULL || address == NULL)
    {
        return -1;
    }

    ip_str--;

    do
    {
        uint32_t tmp_val = 0;

        addr = addr << 8;
        cy_string_to_unsigned(++ip_str, 3, &tmp_val, 0);
        addr += (uint32_t)tmp_val;
        while ((*ip_str != '\x00') && (*ip_str != '.'))
        {
            if (!isdigit((int)*ip_str))
            {
                return -1;
            }
            ip_str++;
        }
        num++;
    } while ((num < 4) && (*ip_str != '\x00'));

    if (num == 4)
    {
        address->version = NW_IP_IPV4;
        address->ip.v4   = htonl(addr);
        return 0;
    }
    return -1;
}

#endif //NX_DISABLE_IPV4

void cy_nw_ip_register_status_change_callback(cy_nw_ip_interface_t nw_interface, cy_nw_ip_status_change_callback_t *cb)
{
    /* This function is not implemented for COMPONENT_NETXDUO. Expectation is to use the Wi-Fi Connection Manager library (WCM) for IP status change callback */
    printf ("Not implemented for COMPONENT_NETXDUO. Use Wi-Fi Connection Manager WCM APIs instead\n");
    return;
}

void cy_nw_ip_unregister_status_change_callback(cy_nw_ip_interface_t nw_interface, cy_nw_ip_status_change_callback_t *cb)
{
    /* This function is not implemented for COMPONENT_NETXDUO. Expectation is to use the Wi-Fi Connection Manager library (WCM) for IP status change callback */
    printf ("Not implemented for COMPONENT_NETXDUO. Use Wi-Fi Connection Manager WCM APIs instead\n");
    return;
}

#ifndef NX_DISABLE_IPV4

int cy_nw_host_arp_cache_clear( cy_nw_ip_interface_t iface )
{
    NX_IP *ip = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);

    if (ip != NULL)
    {
        if (_nx_arp_interface_entries_delete(ip, 0) != NX_SUCCESS)
        {
            return -1;
        }
    }

    return 0;
}

int cy_nw_host_arp_cache_get_list( cy_nw_ip_interface_t iface, cy_nw_arp_cache_entry_t *list, uint32_t count, uint32_t *filled)
{
    NX_IP *ip_ptr;
    NX_ARP *arp_entry;
    ULONG entry_num;

    if (list == NULL || filled == NULL)
    {
        return -1;
    }

    *filled = 0;

    ip_ptr = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);
    if (ip_ptr != NULL)
    {
        /* Obtain protection on this IP instance for access into the ARP lists */
        tx_mutex_get(&(ip_ptr->nx_ip_protection), TX_WAIT_FOREVER);

        /* Loop on the static list. */
        arp_entry = ip_ptr->nx_ip_arp_static_list;
        while (arp_entry && *filled < count)
        {
            list[*filled].ip.version = NW_IP_IPV4;
            list[*filled].ip.ip.v4   = htonl(arp_entry->nx_arp_ip_address);

            list[*filled].mac.mac[0] = (uint8_t)((arp_entry->nx_arp_physical_address_msw >>  8) & 0xFF);
            list[*filled].mac.mac[1] = (uint8_t)((arp_entry->nx_arp_physical_address_msw >>  0) & 0xFF);
            list[*filled].mac.mac[2] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >> 24) & 0xFF);
            list[*filled].mac.mac[3] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >> 16) & 0xFF);
            list[*filled].mac.mac[4] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >>  8) & 0xFF);
            list[*filled].mac.mac[5] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >>  0) & 0xFF);

            *filled += 1;

            arp_entry = arp_entry->nx_arp_pool_next;
        }

        /* Now loop on the dynamic list. */
        arp_entry = ip_ptr->nx_ip_arp_dynamic_list;
        entry_num = 0;
        while (arp_entry && *filled < count && entry_num < ip_ptr->nx_ip_arp_dynamic_active_count)
        {
            list[*filled].ip.version = NW_IP_IPV4;
            list[*filled].ip.ip.v4   = htonl(arp_entry->nx_arp_ip_address);

            list[*filled].mac.mac[0] = (uint8_t)((arp_entry->nx_arp_physical_address_msw >>  8) & 0xFF);
            list[*filled].mac.mac[1] = (uint8_t)((arp_entry->nx_arp_physical_address_msw >>  0) & 0xFF);
            list[*filled].mac.mac[2] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >> 24) & 0xFF);
            list[*filled].mac.mac[3] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >> 16) & 0xFF);
            list[*filled].mac.mac[4] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >>  8) & 0xFF);
            list[*filled].mac.mac[5] = (uint8_t)((arp_entry->nx_arp_physical_address_lsw >>  0) & 0xFF);

            *filled += 1;

            arp_entry = arp_entry->nx_arp_pool_next;
            entry_num++;
        }

        /* Release the protection on the ARP lists. */
        tx_mutex_put(&(ip_ptr->nx_ip_protection));

        /* The query was successful */
        return 0;
    }

    return 1;
}

int cy_nw_host_send_arp_request(cy_nw_ip_interface_t ipiface, const char *ip_string)
{
    cy_nw_ip_address_t ip_address;
    ULONG probe_address;
    NX_IP *ip_ptr;
    UINT err;

    if (cy_nw_str_to_ipv4(ip_string, &ip_address))
    {
        return -1;
    }

    ip_ptr = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);
    if (ip_ptr != NULL)
    {
        /*
         * NetXDuo addresses are in host byte order.
         */

        probe_address = ntohl(ip_address.ip.v4);

        err = _nx_arp_probe_send(ip_ptr, 0, probe_address);
        return err;
    }
    return 1;
}
#endif /* NX_DISABLE_IPV4 */

uint32_t cy_nw_get_time (void)
{
    cy_time_t systime;

    cy_rtos_get_time(&systime);
    return systime;
}


#ifdef __cplusplus
}
#endif
