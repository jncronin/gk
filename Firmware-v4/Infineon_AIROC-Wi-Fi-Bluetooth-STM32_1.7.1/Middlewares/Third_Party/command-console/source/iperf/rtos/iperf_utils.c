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
 * Network utils functions for Netxduo
 */

#ifdef COMPONENT_NETXDUO
#include "cy_nw_helper.h"
#include "iperf_sockets_internal.h"
#include "iperf_netdb_internal.h"

struct hostent* gethostbyname(const char *name)
{
    static struct hostent s_hostent;
    /* Do nothing as it is not supported in netxduo */
    return &s_hostent;
}

int fcntl(int sockID, int flag_type, int f_options)
{
    /* TODO: To be implemented */
    return 0;
}

static int inet_ntoa_internal(const void *src, char *dst, unsigned long dst_size)
{
    unsigned long temp;
    int dst_idx = 0;
    int idx = dst_size;
    char ip_str[35];
    char *ip_str_ptr;
    cy_nw_ip_address_t addr;
    int number_counter = 0;

    /* Set a local pointer to move up the buffer. */
    temp = ntohl(*((unsigned long *)src));

    addr.ip.v4 = temp;
    ip_str_ptr = &ip_str[0];
    cy_nw_ntoa (&addr, ip_str_ptr);

    memset(dst, '\0', dst_size);
    while(ip_str[idx]=='\0')
    {
        idx--;
    }

    while(idx>=0)
    {
        do
        {
            idx--;
        }while(ip_str[idx] != '.' && idx>=0);

        for(int j = idx+1; ;j++)
        {
            if(ip_str[j] == '\0' || ip_str[j] == '.')
            {
                number_counter++;
                if(number_counter < 4)
                {
                    dst[dst_idx] = '.';
                    dst_idx++   ;
                }
                break;
            }
            dst[dst_idx] = ip_str[j];
            dst_idx++;
        }
    }
    dst[dst_idx] = '\0';

    /* Return the size of the dst string. */
    return ((int)(dst_idx));
}

int inet_aton(const char *address_buffer_ptr, struct in_addr *addr)
{
    cy_nw_ip_address_t address;
    cy_nw_aton (address_buffer_ptr , &address);

    /* Check if a return pointer for the address data is supplied. */
    if(addr)
    {
        /* Convert the IP address data to network byte order and return the data. */
        addr->s_addr = address.ip.v4;
    }

    return (1);
}

int  inet_pton(int af, const char *src, void *dst)
{
    struct  in_addr ipv4_addr;

    if(af == AF_INET)
    {
        /* Convert IPv4 address from presentation to numeric. */
        if(inet_aton(src, &ipv4_addr))
        {
            /* Copy the IPv4 address to the destination. */
            *((unsigned long *)dst) = ipv4_addr.s_addr;
            return 1;
        }
        return 0;
    }
    else if(af == AF_INET6)
    {
        cy_nw_ip_address_t ipv6_addr;
        cy_nw_aton_ipv6(src , &ipv6_addr);
        ((struct in6_addr *)dst)->un.u32_addr[0] = ipv6_addr.ip.v6[0];
        ((struct in6_addr *)dst)->un.u32_addr[1] = ipv6_addr.ip.v6[1];
        ((struct in6_addr *)dst)->un.u32_addr[2] = ipv6_addr.ip.v6[2];
        ((struct in6_addr *)dst)->un.u32_addr[3] = ipv6_addr.ip.v6[3];
        return 1;
    }
    else
    {
        /* Error. */
        return -1;
    }
}
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if(af == AF_INET)
    {
        /* Convert IPv4 address from numeric to presentation. */
        if(inet_ntoa_internal(src, dst, size))
            return dst;
        else
            return NX_NULL;
    }
    else if(af == AF_INET6)
    {
        cy_nw_ip_address_t addr_ipv6;
        char ipv6_str[39];
        char *ipv6_str_ptr = &ipv6_str[0];
        addr_ipv6.ip.v6[0] = (uint32_t)(*((uint32_t *)src));
        addr_ipv6.ip.v6[1] = (uint32_t)(*(((uint32_t *)src) + 1));
        addr_ipv6.ip.v6[2] = (uint32_t)(*(((uint32_t *)src) + 2));
        addr_ipv6.ip.v6[3] = (uint32_t)(*(((uint32_t *)src) + 3));
        cy_nw_ntoa_ipv6 (&addr_ipv6, ipv6_str_ptr);
        memcpy(dst, ipv6_str, sizeof(ipv6_str));

        return dst;
    }
    else
    {
        return NX_NULL;
    }
}
#endif /* COMPONENT_NETXDUO */
