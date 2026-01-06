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

/** @file eth_utility.c
 *  @brief This file contains the definition of ethernet commands and implementation of the
 *  command handlers.
 */
#ifndef DISABLE_COMMAND_CONSOLE_ETH
#include "command_console.h"
#include "eth_utility.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cy_ecm.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

#define CMD_CONSOLE_PING_TIMEOUT_DEFAULT    10000
#define CMD_CONSOLE_IPV4_ADDR_SIZE          4

#define MAKE_IPV4_ADDRESS1(a, b, c, d)      ((((uint32_t) d) << 24) | (((uint32_t) c) << 16) | (((uint32_t) b) << 8) |((uint32_t) a))

#if defined(__ICCARM__)
#define ETH_WEAK_FUNC            __WEAK
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define ETH_WEAK_FUNC            __attribute__((weak))
#else
#define ETH_WEAK_FUNC            __attribute__((weak))
#endif

/******************************************************
 *               Function Declarations
 ******************************************************/

/* Ethernet commands */
int eth_set_promiscuous_mode (int argc, char* argv[], tlv_buffer_t** data);
int eth_broadcast            (int argc, char* argv[], tlv_buffer_t** data);
int eth_set_filter_address   (int argc, char* argv[], tlv_buffer_t** data);
int eth_set_filter           (int argc, char* argv[], tlv_buffer_t** data);
int eth_up                   (int argc, char* argv[], tlv_buffer_t** data);
int eth_down                 (int argc, char* argv[], tlv_buffer_t** data);
int eth_ping                 (int argc, char* argv[], tlv_buffer_t** data);

#define ETH_COMMANDS_LIMITED_SET \
    { (char*) "eth_set_promiscuous_mode", eth_set_promiscuous_mode, 1, NULL, NULL, (char*) "<1/0(Enable=1,Disable=0)>", (char*) "Enable/Disable Promiscuous mode"}, \
    { (char*) "eth_broadcast",            eth_broadcast,            1, NULL, NULL, (char*) "<1/0(Enable=1,Disable=0)>", (char*) "Enable/Disable Receiving of broadcast packets"}, \
    { (char*) "eth_set_filter_address",   eth_set_filter_address,   1, NULL, NULL, (char*) "[1-4 supported Filter numbers] [src/dest(Source/Destination)] [X:X:X:X:X:X(MAC address)] [X(Bytes)]", (char*) "Enable MAC address filtering."}, \
    { (char*) "eth_set_filter",           eth_set_filter,           0, NULL, NULL, (char*) "", (char*) "Set the filters to the hardware."}, \
    { (char*) "eth_up",                   eth_up,                   0, NULL, NULL, (char*) "[MII/GMII/RGMII/RMII(Speed type)] [10M/100M/1000M/AUTO(Speed)] [HALF/FULL/AUTO(Duplex Mode)] [0/1(Interface index)]", (char*) "Ethernet interface up. Connects to network"}, \
    { (char*) "eth_down",                 eth_down,                 0, NULL, NULL, (char*) "", (char*) "Ethernet interface down. Disconnects from network"}, \
    { (char*) "eth_ping",                 eth_ping,                 1, NULL, NULL, (char*) "<IP address> [timeout(ms)]", (char*) "ping to an IP address."}, \

/******************************************************
 *                    Constants
 ******************************************************/

static const cy_command_console_cmd_t eth_command_table[] =
{
    ETH_COMMANDS_LIMITED_SET
    CMD_TABLE_END
};

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
static int eth_utils_str_to_ip(char* ip_str, cy_ecm_ip_address_t* ip_addr);
static int eth_utils_str_to_mac(char* mac_str, uint8_t* mac_addr);

/******************************************************
 *               Variables Definitions
 ******************************************************/
cy_ecm_phy_config_t ecm_phy_config;
cy_ecm_t ecm_handle;
cy_ecm_filter_address_t filter_address[CY_ECM_MAX_FILTER_ADDRESS];
static uint8_t filter_count=0;

/******************************************************
 *               Function Definitions
 ******************************************************/
static void print_ip4(uint32_t ip)
{
    unsigned char bytes[CMD_CONSOLE_IPV4_ADDR_SIZE];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;

    ETH_INFO(("IP Address: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]));
}

ETH_WEAK_FUNC cy_rslt_t eth_utility_init(void)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_command_console_add_table(eth_command_table);
    memset(filter_address, 0, CY_ECM_MAX_FILTER_ADDRESS * sizeof(cy_ecm_filter_address_t));
    return res;
}

int eth_set_promiscuous_mode(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    bool promiscuous_mode = false;

    if(atoi(argv[1]) == 1)
    {
        promiscuous_mode = true;
    }
    res = cy_ecm_set_promiscuous_mode(ecm_handle, promiscuous_mode);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_INFO(("cy_ecm_set_promiscuous_mode failed with %x\n", (unsigned int)res));
        return 1;
    }

    ETH_INFO(("Promiscuous mode set : %x\n", promiscuous_mode));
    return 0;
}

int eth_broadcast(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    bool broadcast = false;

    if(atoi(argv[1]) == 1)
    {
        broadcast = true;
    }
    res = cy_ecm_broadcast_disable(ecm_handle, broadcast);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_INFO(("cy_ecm_broadcast_disable failed with %x\n", (unsigned int)res));
        return 1;
    }

    ETH_INFO(("Broadcast mode set : %x\n", broadcast));
    return 0;
}


int eth_set_filter_address(int argc, char* argv[], tlv_buffer_t** data)
{
    uint8_t filter_index=0;

    if(argc == 5)
    {
        uint8_t filter_number = atoi(argv[1]);

        if(filter_number < 1 || filter_number > CY_ECM_MAX_FILTER_ADDRESS)
        {
            ETH_INFO(("Invalid filter number: Should be in the range 1-4\n"));
            return 1;
        }

        filter_index =  filter_number - 1;

        if(strcmp(argv[2],"dest") == 0)
        {
            filter_address[filter_index].filter_type = CY_ECM_FILTER_TYPE_DESTINATION;
        }
        else if(strcmp(argv[2],"src") == 0)
        {
            filter_address[filter_index].filter_type = CY_ECM_FILTER_TYPE_SOURCE;
        }
        else
        {
            ETH_INFO(("Invalid first parameter\n"));
            return 1;
        }

        eth_utils_str_to_mac(argv[3], filter_address[filter_index].filter_addr);
        filter_address[filter_index].ignoreBytes = atoi(argv[4]);
        filter_count++;
    }
    else
    {
        ETH_INFO(("Invalid arguments\n"));
        return 1;
    }

    ETH_INFO((" eth_set_filter_address successful \n"));
    return 0;
}

int eth_set_filter(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;

    res = cy_ecm_set_filter_address(ecm_handle, filter_address, filter_count);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_INFO(("cy_ecm_set_filter_address failed with %x\n", (unsigned int)res));
        filter_count = 0;
        return 1;
    }

    memset(filter_address, 0, CY_ECM_MAX_FILTER_ADDRESS * sizeof(cy_ecm_filter_address_t));
    filter_count = 0;

    ETH_INFO((" eth_set_filter successful. Configured filter address to hardware \n"));
    return 0;
}

int eth_up(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    cy_ecm_ip_address_t ip_addr;
    cy_ecm_interface_t eth_idx;

    ETH_INFO(("Initiating Ethernet connection...\n"));

    if(argc == 5)
    {
        if(strcmp(argv[1],"RGMII") != 0)
        {
            ETH_INFO(("Only supports RGMII\n"));
            return 1;
        }
        ecm_phy_config.interface_speed_type = CY_ECM_SPEED_TYPE_RGMII;

        if(strcmp(argv[2],"10M") == 0)
        {
            ecm_phy_config.phy_speed = CY_ECM_PHY_SPEED_10M;
        }
        else if(strcmp(argv[2],"100M") == 0)
        {
            ecm_phy_config.phy_speed = CY_ECM_PHY_SPEED_100M;
        }
        else if(strcmp(argv[2],"1000M") == 0)
        {
            ecm_phy_config.phy_speed = CY_ECM_PHY_SPEED_1000M;
        }
        else
        {
            ecm_phy_config.phy_speed = CY_ECM_PHY_SPEED_AUTO;
        }

        if(strcmp(argv[3],"HALF") == 0)
        {
            ecm_phy_config.mode = CY_ECM_DUPLEX_HALF;
        }
        else if(strcmp(argv[3],"FULL") == 0)
        {
            ecm_phy_config.mode = CY_ECM_DUPLEX_FULL;
        }
        else
        {
            ecm_phy_config.mode = CY_ECM_DUPLEX_AUTO;
        }

        if(*argv[4] == '0')
        {
            eth_idx = CY_ECM_INTERFACE_ETH0;
        }
        else if(*argv[4] == '1')
        {
            eth_idx = CY_ECM_INTERFACE_ETH1;
        }
        else
        {
            eth_idx = CY_ECM_INTERFACE_INVALID;
            ETH_INFO(("Invalid ethernet index\n"));
            return 1;
        }

        res = cy_ecm_ethif_init(eth_idx, NULL, &ecm_phy_config, &ecm_handle);
        if(res != CY_RSLT_SUCCESS)
        {
            ETH_INFO(("cy_ecm_ethif_init failed\n"));
            return 1;
        }
    }
    else if (argc == 1)
    {
        eth_idx = CY_ECM_INTERFACE_ETH1;
        res = cy_ecm_ethif_init(eth_idx, NULL, NULL, &ecm_handle);
        if(res != CY_RSLT_SUCCESS)
        {
            ETH_INFO(("cy_ecm_ethif_init failed\n"));
            return 1;
        }
    }
    else
    {
        ETH_INFO(("Invalid number of parameters\n"));
    }

    ETH_INFO(("Connecting...\n"));
    res = cy_ecm_connect(ecm_handle, NULL, &ip_addr);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_ERROR(("eth_up failed\n"));
        cy_ecm_ethif_deinit(&ecm_handle);
        return 1;
    }
    else
    {
        ETH_INFO(("Successfully connected\n"));
        print_ip4(ip_addr.ip.v4);
    }
    return 0;
}

int eth_down(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    bool status;

    res = cy_ecm_get_link_status(ecm_handle, &status);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_INFO(("Failed to get the link status\n"));
        return 1;
    }
    if(status)
    {
        res = cy_ecm_disconnect(ecm_handle);
        if(res != CY_RSLT_SUCCESS)
        {
            ETH_INFO(("eth_down failed\n"));
            return 1;
        }

        res = cy_ecm_ethif_deinit(&ecm_handle);
        if(res != CY_RSLT_SUCCESS)
        {
            ETH_INFO(("cy_ecm_ethif_deinit failed\n"));
            return 1;
        }

        ETH_INFO(("Ethernet down...\n"));
    }
    return 0;
}

int eth_ping(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    cy_ecm_ip_address_t ip_addr;
    uint32_t timeout_ms = CMD_CONSOLE_PING_TIMEOUT_DEFAULT;
    uint32_t elapsed_ms;
    bool status;
    if(argc < 2)
    {
        ETH_ERROR(("Wrong number of arguments. Command format: eth_ping <ipaddr> [timeout(ms)]\n"));
        return 1;
    }

    res = cy_ecm_get_link_status(ecm_handle, &status);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_INFO(("Failed to get the link status\n"));
        return 1;
    }
    if(status == 0)
    {
        ETH_INFO(("Ethernet link is not up\n"));
        return 1;
    }
    if(eth_utils_str_to_ip(argv[1], &ip_addr) == 1)
    {
        return 1;
    }
    if(argc == 3)
    {
        timeout_ms = atoi(argv[2]);
    }
    res = cy_ecm_ping(ecm_handle, &ip_addr, timeout_ms, &elapsed_ms);
    if(res != CY_RSLT_SUCCESS)
    {
        ETH_ERROR(("Ping failed. Error: 0x%X\n", (unsigned int)res));
        return 1;
    }

    ETH_INFO(("Ping successful. Elapsed time: %u (ms)\n", (unsigned int)elapsed_ms));
    return 0;
}

int eth_utils_str_to_ip(char* ip_str, cy_ecm_ip_address_t* ip_addr)
{
    int bytes[CMD_CONSOLE_IPV4_ADDR_SIZE];
    char* p = NULL;
    char* rest = NULL;
    int i = 0;
    char* delimiter = ".";

    if((ip_str == NULL)||(ip_addr == NULL))
    {
        ETH_ERROR(("Invalid parameter.\n"));
        return 1;
    }
    memset(&bytes, 0x00, sizeof(bytes));
    rest = ip_str;
    p = strtok_r (rest, delimiter, &rest);
    if(p == NULL)
    {
        ETH_ERROR(("Invalid IP Addr.\n"));
        return 1;
    }
    bytes[i] = atoi(p);
    ETH_DEBUG(("bytes[%d]:%d\n", i, bytes[i]));
    while(p != NULL)
    {
        ++i;
        p = strtok_r (rest, delimiter, &rest);
        if(i < CMD_CONSOLE_IPV4_ADDR_SIZE)
        {
            if(p == NULL)
            {
                ETH_ERROR(("Invalid IP Addr.\n"));
                return 1;
            }
            bytes[i] = atoi(p);
            ETH_DEBUG(("bytes[%d]:%d\n", i, bytes[i]));
        }
    }
    if(i != (CMD_CONSOLE_IPV4_ADDR_SIZE))
    {
        ETH_ERROR(("Invalid IP Addr.\n"));
        return 1;
    }

    ip_addr->version = CY_ECM_IP_VER_V4;
    ip_addr->ip.v4 = (uint32_t)MAKE_IPV4_ADDRESS1(bytes[0], bytes[1], bytes[2], bytes[3]);

    return 0;
}

static int eth_utils_str_to_mac(char* mac_str, uint8_t* mac_addr)
{
    if(strlen(mac_str) != 17)
    {
        ETH_ERROR(("Invalid Mac address\n"));
        return 1;
    }
    sscanf(mac_str,"%x:%x:%x:%x:%x:%x",
            (unsigned int *)&mac_addr[0],(unsigned int *)&mac_addr[1],(unsigned int *)&mac_addr[2],
            (unsigned int *)&mac_addr[3],(unsigned int *)&mac_addr[4],(unsigned int *)&mac_addr[5]);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
