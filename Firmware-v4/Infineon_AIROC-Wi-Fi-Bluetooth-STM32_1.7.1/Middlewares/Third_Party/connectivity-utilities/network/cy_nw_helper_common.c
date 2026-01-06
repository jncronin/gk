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
 * Network helper common library functions
 */
 
#include "cy_nw_helper.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#ifdef __cplusplus
extern "C" {
#endif

#define NW_HTONL(x) ((((x) & (uint32_t)0x000000ffUL) << 24) | \
                     (((x) & (uint32_t)0x0000ff00UL) <<  8) | \
                     (((x) & (uint32_t)0x00ff0000UL) >>  8) | \
                     (((x) & (uint32_t)0xff000000UL) >> 24))

#define IPV4_MAX_STR_LEN 15
#define IPV6_MAX_STR_LEN 39

bool cy_nw_aton (const char *char_ptr , cy_nw_ip_address_t *addr)
{
    uint8_t byte = 0;
    uint8_t byte_cnt = 1;
    uint32_t ip_addr = 0;
    char ch;

    if(char_ptr == NULL || addr == NULL)
    {
        return 1;
    }

    while(*char_ptr != '\0' || byte_cnt <= 4)
    {
        ch = *char_ptr;
        if(ch == '.' || ch == '\0')
        {
            if(byte_cnt == 1 && ch == '.')
            {
                ip_addr = ip_addr | (uint32_t)byte;
            }
            else if(byte_cnt == 2 && ch == '.')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 8);
            }
            else if(byte_cnt == 3 && ch == '.')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 16);
            }
            else if(byte_cnt == 4 && ch == '\0')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 24);
                break;
            }
            else
            {
                break;
            }                
            byte = 0;
            byte_cnt++;
            char_ptr++;
            continue;
        }
        byte = (byte * 10) + (ch - '0');
        char_ptr++;
    }
    if(byte_cnt < 4)
    {
        return 1;
    }
    addr->version = NW_IP_IPV4;
    addr->ip.v4 = ip_addr;

    return 0;
}

static uint32_t str_to_decimal(char *hex)
{
    uint32_t decimal = 0, base = 1;
    int i = 0, length = 4;

    for(i = length-1; i >= 0; i--)
    {
        if(hex[i] >= '0' && hex[i] <= '9')
        {
            decimal += (hex[i] - 48) * base;
            base *= 16;
        }
        else if(hex[i] >= 'A' && hex[i] <= 'F')
        {
            decimal += (hex[i] - 55) * base;
            base *= 16;
        }
        else if(hex[i] >= 'a' && hex[i] <= 'f')
        {
            decimal += (hex[i] - 87) * base;
            base *= 16;
        }
    }
    return decimal;
}

bool cy_nw_aton_ipv6(const char *char_ptr , cy_nw_ip_address_t *addr)
{
    char hex[4] = {0},ch;
    uint8_t byte_cnt = 1, i=0;
    uint16_t byte = 0, ans[8];
    uint32_t temp = 0;

    if(char_ptr == NULL || addr == NULL)
    {
        return 1;
    }

    while(*char_ptr != '\0' || byte_cnt < 8)
    {
        if(byte_cnt > 8)
        {
            break;
        }
        ch = *char_ptr;
        if(ch == ':')
        {
            byte = str_to_decimal(hex);
            ans[byte_cnt-1] = byte;
            i = 0;
            byte_cnt++;
            char_ptr++;
            continue;
        }
        else if(ch == '\0' && byte_cnt == 8)
        {
            byte = str_to_decimal(hex);
            ans[byte_cnt-1] = byte;
            i = 0;
            byte_cnt++;
            char_ptr++;
            break;
        }
        else if(ch == '\0' && byte_cnt != 8)
        {
            break;
        }
        hex[i] = ch;
        i++;
        char_ptr++;
    }
    if(byte_cnt != 8)
    {
        return 1;
    }
    byte = str_to_decimal(hex);
    ans[byte_cnt-1] = byte;

    addr->version = NW_IP_IPV6;

    temp = (((uint32_t)ans[0])<<16) | (uint32_t)(ans[1]);
    addr->ip.v6[0] = NW_HTONL(temp);
    temp = ((uint32_t)ans[2]<<16) | (uint32_t)ans[3];
    addr->ip.v6[1] = NW_HTONL(temp);
    temp = ((uint32_t)ans[4]<<16) | (uint32_t)ans[5];
    addr->ip.v6[2] = NW_HTONL(temp);
    temp = ((uint32_t)ans[6]<<16) | (uint32_t)ans[7];
    addr->ip.v6[3] = NW_HTONL(temp);
    return 0;
}

bool cy_nw_ntoa (cy_nw_ip_address_t *addr, char *ip_str)
{
    uint8_t index = 0;
    uint8_t arr[4] = {0};
    uint32_t ip_addr;
    if(ip_str == NULL || addr == NULL)
    {
        return 1;
    }
    ip_addr = addr->ip.v4;
    while(ip_addr != 0)
    {
        arr[index] = ip_addr & 0xff;
        ip_addr = ip_addr >> 8;
        index++;
    }
    memset(ip_str,0, (IPV4_MAX_STR_LEN*sizeof(char)));
    sprintf(ip_str, "%d.%d.%d.%d", arr[0],arr[1],arr[2],arr[3]);
    return 0;
}

bool cy_nw_ntoa_ipv6 (cy_nw_ip_address_t *addr, char *ip_str)
{
    if(addr == NULL || ip_str ==NULL)
    {
        printf("Input IPV6 address is NULL\n");
        return 1;
    }
    memset(ip_str,0, (IPV6_MAX_STR_LEN*sizeof(char)));
    sprintf(ip_str, "%0x:%0x:%0x:%0x", (unsigned int)NW_HTONL(addr->ip.v6[0]), (unsigned int)NW_HTONL(addr->ip.v6[1]), (unsigned int)NW_HTONL(addr->ip.v6[2]), (unsigned int)NW_HTONL(addr->ip.v6[3]));
    return 0;
}

#ifdef __cplusplus
}
#endif