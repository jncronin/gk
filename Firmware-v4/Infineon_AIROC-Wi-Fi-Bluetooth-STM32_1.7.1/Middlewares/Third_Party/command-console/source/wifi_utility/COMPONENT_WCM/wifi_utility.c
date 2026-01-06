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

/** @file wifi_utility.c
 *  @brief This file contains the definition of Wi-Fi commands and implementation of the
 *  command handlers.
 */
#ifndef DISABLE_COMMAND_CONSOLE_WIFI
#include "command_console.h"
#include "wifi_utility.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cy_wcm.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef COMPONENT_CAT5
#define ENABLE_ENTPS_FEATURE
#endif /* COMPONENT_CAT5 */

/******************************************************
 *                      Macros
 ******************************************************/
#define INITIALISER_IPV4_ADDRESS1(addr_var, addr_val) addr_var = {CY_WCM_IP_VER_V4, {.v4 = (uint32_t)(addr_val)}}
#define MAKE_IPV4_ADDRESS1(a, b, c, d)      ((((uint32_t) d) << 24) | (((uint32_t) c) << 16) | (((uint32_t) b) << 8) |((uint32_t) a))

#define CMD_CONSOLE_SSID_MAX_LENGTH         33 /* 32 is what 802.11 defines as longest possible name; +1 for the \0 */
#define CMD_CONSOLE_PASS_MAX_LENGTH         64

#ifndef CMD_CONSOLE_MAX_WIFI_RETRY_COUNT
#define CMD_CONSOLE_MAX_WIFI_RETRY_COUNT    5
#endif

#define CMD_CONSOLE_PING_TIMEOUT_DEFAULT    10000
#define CMD_CONSOLE_IPV4_ADDR_SIZE          4

#if defined(__ICCARM__)
#define WIFI_WEAK_FUNC            __WEAK
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define WIFI_WEAK_FUNC            __attribute__((weak))
#else
#define WIFI_WEAK_FUNC           __attribute__((weak))
#endif

/******************************************************
 *               External Function Declarations
 ******************************************************/
extern char *strtok_r( char *, const char *, char ** );

/******************************************************
 *               Function Declarations
 ******************************************************/

/* Wi-Fi commands */
int join               (int argc, char* argv[], tlv_buffer_t** data);
int leave              (int argc, char* argv[], tlv_buffer_t** data);
int scan               (int argc, char* argv[], tlv_buffer_t** data);
int ping               (int argc, char* argv[], tlv_buffer_t** data);
int get_rssi           (int argc, char* argv[], tlv_buffer_t** data);
int start_ap           (int argc, char* argv[], tlv_buffer_t** data);
int stop_ap            (int argc, char* argv[], tlv_buffer_t** data);
int get_sta_ifconfig   (int argc, char* argv[], tlv_buffer_t** data);

#define WIFI_COMMANDS_LIMITED_SET \
    { (char*) "join",               join,             2, NULL, NULL, (char*) "<ssid> <open|wpa_aes|wpa_tkip|wpa2|wpa2_aes|wpa2_aes_sha256|wpa2_tkip|wpa2_fbt|wpa3|wpa3_wpa2> [password] [channel] [band<0=auto,1=5G,2=2.4G,3=6G>]"ESCAPE_SPACE_PROMPT, (char*) "Join an AP.(This command is deprecated and it will be removed in the future. Please use wifi_join command)"}, \
    { (char*) "leave",              leave,            0, NULL, NULL, (char*) "", (char*) "Leave the connected AP.(This command is deprecated and it will be removed in the future. Please use wifi_leave command)"}, \
    { (char*) "scan",               scan,             0, NULL, NULL, (char*) "", (char*) "Scan all the Wi-Fi AP in the vicinity.(This command is deprecated and it will be removed in the future. Please use wifi_scan command)"}, \
    { (char*) "ping",               ping,             0, NULL, NULL, (char*) "<IP address> [timeout(ms)]", (char*) "ping to an IP address.(This command is deprecated and it will be removed in the future. Please use wifi_ping command)"}, \
    { (char*) "get_rssi",           get_rssi,         0, NULL, NULL, (char*) "", (char*) "Get the received signal strength of the AP (client mode only).(This command is deprecated and it will be removed in the future. Please use wifi_get_rssi command)"}, \
    { (char*) "wifi_join",          join,             2, NULL, NULL, (char*) "<ssid> <open|wpa_aes|wpa_tkip|wpa2|wpa2_aes|wpa2_aes_sha256|wpa2_tkip|wpa2_fbt|wpa3|wpa3_wpa2> [password] [channel] [band<0=auto,1=5G,2=2.4G,3=6G>]"ESCAPE_SPACE_PROMPT, (char*) "Join an AP."}, \
    { (char*) "wifi_leave",         leave,            0, NULL, NULL, (char*) "", (char*) "Leave the connected AP."}, \
    { (char*) "wifi_scan",          scan,             0, NULL, NULL, (char*) "", (char*) "Scan all the Wi-FI AP in the vicinity."}, \
    { (char*) "wifi_ping",          ping,             0, NULL, NULL, (char*) "<IP address> [timeout(ms)]", (char*) "ping to an IP address"}, \
    { (char*) "wifi_get_rssi",      get_rssi,         0, NULL, NULL, (char*) "", (char*) "Get the received signal strength of the AP (client mode only)."}, \
    { (char*) "start_ap",           start_ap,         4, NULL, NULL, \
      (char*) "<ssid> <open|wpa2|wpa2_aes|wpa3|wpa3_wpa2|wep|wep_shared> <key> <channel> [band<0=auto,1=5G,2=2.4G,3=6G>] <bandwidth> [ip] [netmask]\n-->When any parameter has spaces, use quotes.\n\tE.g. start_ap \"my ssid\" wpa2 \"my wpa2 key \" 11 20 192.168.2.1 255.255.255.0.  Default settings for ip and subnet mask are 192.168.0.1 and 255.255.255.0, or the last ip and subnet specified through this command if applicable.", \
      (char*) "Start AP mode."}, \
    { (char*) "stop_ap",            stop_ap,          0, NULL, NULL, (char*) "", (char*) "Stop AP mode."}, \
    { (char*) "get_sta_ifconfig",   get_sta_ifconfig, 0, NULL, NULL, (char*) "", (char*) "Get IP & MAC address of STA."}, \

/******************************************************
 *                    Constants
 ******************************************************/

static const cy_command_console_cmd_t wifi_command_table[] =
{
    WIFI_COMMANDS_LIMITED_SET
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
static void scan_result_cb(cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status);

static int convert_to_wcm_connect_params(int argc, char* argv[], cy_wcm_connect_params_t* connect_params);
static int wifi_utils_str_to_ip(char* ip_str, cy_wcm_ip_address_t* ip_addr);
static int wifi_utils_str_to_band(char* channel_str, char* band_str, cy_wcm_wifi_band_t* band);
static cy_wcm_security_t wifi_utils_str_to_authtype(char* auth_str);
static const char* wifi_utils_authtype_to_str(cy_wcm_security_t sec);

static void print_ip4(uint32_t ip);
static cy_rslt_t start_ap_common(const char *ssid, const char *key, uint8_t channel, cy_wcm_security_t security_type, cy_wcm_custom_ie_info_t *custom_ie, cy_wcm_wifi_band_t band);

/******************************************************
 *               Variables Definitions
 ******************************************************/
static const cy_wcm_ip_setting_t ap_ip_settings =
{
    INITIALISER_IPV4_ADDRESS1(.ip_address, MAKE_IPV4_ADDRESS1(192, 168, 0, 2)),
    INITIALISER_IPV4_ADDRESS1(.netmask, MAKE_IPV4_ADDRESS1(255, 255, 255, 0)),
    INITIALISER_IPV4_ADDRESS1(.gateway, MAKE_IPV4_ADDRESS1(192, 168, 0, 2)),
};

/******************************************************
 *               Function Definitions
 ******************************************************/

WIFI_WEAK_FUNC cy_rslt_t wifi_utility_init(void)
{
    cy_rslt_t res;
    cy_wcm_config_t wcm_config;

    cy_command_console_add_table(wifi_command_table);

    wcm_config.interface = CY_WCM_INTERFACE_TYPE_AP_STA;
    res = cy_wcm_init(&wcm_config);
    if(res != CY_RSLT_SUCCESS)
    {
        WIFI_ERROR(("Failed to initialize Wi-Fi module. Res:0x%X\n", (unsigned int)res));
        return res;
    }
    WIFI_INFO(("Wi-Fi module initialized...\n"));

    return res;
}

int join(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    int result;
    cy_wcm_connect_params_t connect_params;
    cy_wcm_ip_address_t ip_addr;
    int retry_count = 0;
    bool wifi_connected = false;

    if(cy_wcm_is_connected_to_ap())
    {
        WIFI_INFO(("Already connected. Leave and join again.\n"));
        return 0;
    }

    result = convert_to_wcm_connect_params(argc, argv, &connect_params);
    if(result != 0)
    {
        return result;
    }

    WIFI_INFO(("Connecting to AP please wait...\n"));
    for(retry_count = 0; retry_count < CMD_CONSOLE_MAX_WIFI_RETRY_COUNT; retry_count++)
    {
        memset(&ip_addr, 0x00, sizeof(cy_wcm_ip_address_t));
        res = cy_wcm_connect_ap(&connect_params, &ip_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            WIFI_INFO(("Failed to join AP [0x%X]. Retrying...\n", (unsigned int)res));
            continue;
        }
        wifi_connected = true;
        WIFI_INFO(("Successfully connected to %s\n", argv[1]));
        print_ip4(ip_addr.ip.v4);
        break;
    }

    if(!wifi_connected)
    {
        WIFI_ERROR(("Exceeded maximum Wi-Fi connection attempts.\n"));
        return -1;
    }

    return 0;
}

int leave(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;

    if(!cy_wcm_is_connected_to_ap())
    {
        WIFI_INFO(("Failed to leave. Not connected to any AP.\n"));
        return 0;
    }

    res = cy_wcm_disconnect_ap();
    if(res != CY_RSLT_SUCCESS)
    {
        WIFI_ERROR(("Failed to disconnect from AP. Res:0x%X", (unsigned int)res));
        return -1;
    }

    WIFI_INFO(("Disconnected from AP.\n"));

    return 0;
}

int scan(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;

    WIFI_INFO(("#### Scan Results ####\n\n"));
    WIFI_INFO(("SSID                 Security Type  RSSI(dBm)  Channel BSSID\n"));

    res = cy_wcm_start_scan(scan_result_cb, NULL, NULL);
    if(res != CY_RSLT_SUCCESS && res != CY_RSLT_WCM_SCAN_IN_PROGRESS)
    {
        WIFI_ERROR(("Error while scanning. Res: 0x%X", (unsigned int)res));
        return -1;
    }

    return 0;
}

int ping(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    cy_wcm_ip_address_t ip_addr;
    uint32_t timeout_ms = CMD_CONSOLE_PING_TIMEOUT_DEFAULT;
    uint32_t elapsed_ms;

    if(!cy_wcm_is_connected_to_ap())
    {
        WIFI_INFO(("Not connected to AP.\n"));
        return 0;
    }

    if(argc < 2)
    {
        WIFI_ERROR(("Wrong number of arguments. Command format: ping <ipaddr> [timeout(ms)]\n"));
        return -1;
    }

    if(wifi_utils_str_to_ip(argv[1], &ip_addr) == -1)
    {
        return -1;
    }

    if(argc == 3)
    {
        timeout_ms = atoi(argv[2]);
    }

    res = cy_wcm_ping(CY_WCM_INTERFACE_TYPE_STA, &ip_addr, timeout_ms, &elapsed_ms);
    if(res != CY_RSLT_SUCCESS)
    {
        WIFI_ERROR(("Ping failed. Error: 0x%X\n", (unsigned int)res));
        return -1;
    }

    WIFI_INFO(("Ping successful. Elapsed time: %u (ms)\n", (unsigned int)elapsed_ms));

    return 0;
}

int get_rssi(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    cy_wcm_associated_ap_info_t ap_info;

    res = cy_wcm_get_associated_ap_info(&ap_info);
    if(res != CY_RSLT_SUCCESS)
    {
        WIFI_ERROR(("Failed to get RSSI. Res:0x%X\n", (unsigned int)res));
        return -1;
    }
    WIFI_INFO(("RSSI: %d dBm\n", ap_info.signal_strength));

    return 0;
}

int start_ap(int argc, char* argv[], tlv_buffer_t** data)
{
    char *ssid = argv[1];
    cy_wcm_security_t auth_type = wifi_utils_str_to_authtype(argv[2]);
    char *security_key = argv[3];
    uint8_t channel = atoi(argv[4]);
    cy_wcm_wifi_band_t band = (cy_wcm_wifi_band_t)(atoi(argv[5]));
    cy_rslt_t res;

    res = start_ap_common(ssid, security_key, channel, auth_type, NULL, band);
    if(res != CY_RSLT_SUCCESS)
    {
        WIFI_ERROR(("Failed to start_ap_common. Res:0x%x\n", (unsigned int)res));
        return -1;
    }
    WIFI_INFO(("start_ap successful!\n"));
    return 0;
}

int stop_ap(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res;
    WIFI_INFO(("stop_ap start\n"));
    res = cy_wcm_stop_ap();
    if(res != CY_RSLT_SUCCESS)
    {
        WIFI_ERROR(("Failed to stop_ap. Res:0x%x", (unsigned int)res));
        return -1;
    }

    WIFI_INFO(("stop_ap successful!\n"));
    return 0;
}

int get_sta_ifconfig(int argc, char* argv[], tlv_buffer_t** data)
{
    cy_rslt_t res, result;
    cy_wcm_ip_address_t ip_addr;
    uint8_t mac[6] = {0};

    res = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
    if (res != CY_RSLT_SUCCESS)
    {
         WIFI_INFO(("cy_wcm_get_ip_addr failed...! with error: [0x%X]\n", (unsigned int)res));
    }
    else
    {
         print_ip4(ip_addr.ip.v4);
    }
    result = cy_wcm_get_mac_addr(CY_WCM_INTERFACE_TYPE_STA, &mac);
    if (result != CY_RSLT_SUCCESS)
    {
         WIFI_INFO(("STA MAC Address failed result:0x%X\n", (unsigned int)result));
    }
    else
    {
         WIFI_INFO(("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]));
    }
    return 0;
}

static cy_rslt_t start_ap_common(const char *ssid, const char *key, uint8_t channel, cy_wcm_security_t security_type, cy_wcm_custom_ie_info_t *custom_ie, cy_wcm_wifi_band_t band)
{
    cy_rslt_t result;
    cy_wcm_ap_config_t ap_params;
    cy_wcm_ip_address_t ipv4_addr;

    memset(&ap_params, 0, sizeof(cy_wcm_ap_config_t));
    memcpy(ap_params.ap_credentials.SSID, ssid, strlen(ssid) + 1);
    memcpy(ap_params.ap_credentials.password, key, strlen(key) + 1);
    ap_params.ap_credentials.security = security_type;
    ap_params.channel = channel;
#ifdef WIFI_6G_CAPABLE 
    ap_params.band = band;
#endif
    ap_params.ip_settings.ip_address = ap_ip_settings.ip_address;
    ap_params.ip_settings.gateway = ap_ip_settings.gateway;
    ap_params.ip_settings.netmask = ap_ip_settings.netmask;
    if (custom_ie)
    {
        WIFI_INFO(("Setting custom IE\n"));
        ap_params.ie_info = custom_ie;
        WIFI_INFO(("custom_ie.subtype = %d \n", ap_params.ie_info->subtype));
        WIFI_INFO(("custom_ie.length = %d \n", ap_params.ie_info->length));
        WIFI_INFO(("custom_ie.ie_packet_mask  = %d \n", ap_params.ie_info->ie_packet_mask));
        WIFI_INFO(("custom_ie->data  = %s \n", (char *)ap_params.ie_info->data));
    }
    result = cy_wcm_start_ap(&ap_params);
    if (result != CY_RSLT_SUCCESS)
    {
        WIFI_INFO(("WCM AP start failed with error: [0x%X]\n", (unsigned int)result));
        return result;
    }
    WIFI_INFO(("WCM AP started\n"));
    result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_AP, &ipv4_addr);
    if (result != CY_RSLT_SUCCESS)
    {
        WIFI_INFO(("cy_wcm_get_ip_addr failed...! with error: [0x%X]\n", (unsigned int)result));
        return result;
    }
    print_ip4(ap_params.ip_settings.ip_address.ip.v4);
    return CY_RSLT_SUCCESS;
}

void scan_result_cb(cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status)
{
    if(status == CY_WCM_SCAN_INCOMPLETE)
    {
        WIFI_INFO(("%-20s %-14s %-10d %-7d %02X:%02X:%02X:%02X:%02X:%02X",
        result_ptr->SSID,
        wifi_utils_authtype_to_str(result_ptr->security),
        result_ptr->signal_strength,
        result_ptr->channel,
        result_ptr->BSSID[0],
        result_ptr->BSSID[1],
        result_ptr->BSSID[2],
        result_ptr->BSSID[3],
        result_ptr->BSSID[4],
        result_ptr->BSSID[5]));

        if(user_data != NULL)
        {
            WIFI_DEBUG((" message = %s ", (char*)user_data));
        }

#if IE_DATA_PRINTS
        int i = 0;
        WIFI_DEBUG((" ie len = %lu data: ", (unsigned long)result_ptr->ie_len));
        for(i=0; i<result_ptr->ie_len; i++)
        {
            WIFI_DEBUG(("%02x ", result_ptr->ie_ptr[i]));
        }
#endif
        WIFI_INFO(("\n"));
    }
    else if(status == CY_WCM_SCAN_COMPLETE)
    {
        WIFI_INFO(("#### Scan Results END ####\n\n"));
    }
}

int convert_to_wcm_connect_params(int argc, char* argv[], cy_wcm_connect_params_t* connect_params)
{
    memset(connect_params, 0, sizeof(cy_wcm_connect_params_t));

    // command format: join <ssid> <sec type> [key] [channel/band]
    if(argc < 3)
    {
        WIFI_ERROR(("Insufficient number of arguments. Command format: join <ssid> <sec type> [key] [channel/band]\n"));
        return -1;
    }

    if(strlen(argv[1]) >= CMD_CONSOLE_SSID_MAX_LENGTH)
    {
        WIFI_ERROR(("SSID length should be lesser than [%d] bytes.\n", CMD_CONSOLE_SSID_MAX_LENGTH));
        return -1;
    }

    // ssid
    memcpy(connect_params->ap_credentials.SSID, argv[1], strlen(argv[1]) + 1);

    // security type
    connect_params->ap_credentials.security = wifi_utils_str_to_authtype(argv[2]);
    if(connect_params->ap_credentials.security == CY_WCM_SECURITY_UNKNOWN)
    {
        WIFI_ERROR(("Cannot connect due to unsupported auth type.\n"));
        return -1;
    }

    // passkey
    if(connect_params->ap_credentials.security != CY_WCM_SECURITY_OPEN)
    {
        if(argc < 4)
        {
            WIFI_ERROR(("Insufficient number of arguments.\n"));
            return -1;
        }
        if(strlen(argv[3]) >= CMD_CONSOLE_PASS_MAX_LENGTH)
        {
            WIFI_ERROR(("Password length should be lesser than [%d] bytes.\n", CMD_CONSOLE_PASS_MAX_LENGTH));
            return -1;
        }
        memcpy(connect_params->ap_credentials.password, argv[3], strlen(argv[3]) + 1);
    }

    // channel
    if(argc == 5 && wifi_utils_str_to_band(argv[4], NULL, &connect_params->band) == -1)
    {
        return -1;
    }

    // channel/band
    if(argc == 6 && wifi_utils_str_to_band(argv[4], argv[5], &connect_params->band) == -1)
    {
        return -1;
    }
    return 0;
}

int wifi_utils_str_to_ip(char* ip_str, cy_wcm_ip_address_t* ip_addr)
{
    int bytes[CMD_CONSOLE_IPV4_ADDR_SIZE];
    char* p = NULL;
    char* rest = NULL;
    int i = 0;
    char* delimiter = ".";

    if((ip_str == NULL)||(ip_addr == NULL))
    {
        WIFI_ERROR(("Invalid parameter.\n"));
        return -1;
    }
    memset(&bytes, 0x00, sizeof(bytes));
    rest = ip_str;
    p = strtok_r (rest, delimiter, &rest);
    if(p == NULL)
    {
        WIFI_ERROR(("Invalid IP Addr.\n"));
        return -1;
    }
    bytes[i] = atoi(p);
    WIFI_DEBUG(("bytes[%d]:%d\n", i, bytes[i]));
    while(p != NULL)
    {
        ++i;
        p = strtok_r (rest, delimiter, &rest);
        if(i < CMD_CONSOLE_IPV4_ADDR_SIZE)
        {
            if(p == NULL)
            {
                WIFI_ERROR(("Invalid IP Addr.\n"));
                return -1;
            }
            bytes[i] = atoi(p);
            WIFI_DEBUG(("bytes[%d]:%d\n", i, bytes[i]));
        }
    }

    if(i != (CMD_CONSOLE_IPV4_ADDR_SIZE))
    {
        WIFI_ERROR(("Invalid IP Addr.\n"));
        return -1;
    }

    ip_addr->version = CY_WCM_IP_VER_V4;
    ip_addr->ip.v4 = (uint32_t)MAKE_IPV4_ADDRESS1(bytes[0], bytes[1], bytes[2], bytes[3]);

    return 0;
}

int wifi_utils_str_to_band(char* channel_str, char* band_str, cy_wcm_wifi_band_t* band)
{
    int channel = atoi(channel_str);
    int band_arg = CY_WCM_WIFI_BAND_ANY;

    if (band_str != NULL)
    {
        band_arg = atoi(band_str);
    }

    if((band_arg == CY_WCM_WIFI_BAND_ANY || band_arg == CY_WCM_WIFI_BAND_2_4GHZ) &&
        (channel >= 1 && channel <= 14))
    {
        *band = CY_WCM_WIFI_BAND_2_4GHZ;
    }
    else if((band_arg == CY_WCM_WIFI_BAND_ANY || band_arg == CY_WCM_WIFI_BAND_5GHZ) &&
        (channel >= 32 && channel <= 165))
    {
        *band = CY_WCM_WIFI_BAND_5GHZ;
    }
#if defined(WIFI_6G_CAPABLE)
    else if((band_arg == CY_WCM_WIFI_BAND_ANY || band_arg == CY_WCM_WIFI_BAND_6GHZ) &&
        (channel >= 1 && channel <= 233))
    {
        *band = CY_WCM_WIFI_BAND_6GHZ;
    }
#endif // defined(WIFI_6G_CAPABLE)
    else
    {
        WIFI_ERROR(("Invalid channel(%d) band(%d). Valid values: 1 to 14 - 2.4GHz, 32 to 165 - 5GHz, 1 to 233 - 6GHz(Please define WIFI_6G_CAPABLE in Makefile)\n",
            channel, band_arg));
        return -1;
    }

    return 0;
}

cy_wcm_security_t wifi_utils_str_to_authtype(char* auth_str)
{
    if (strcmp(auth_str, "open") == 0)
    {
        return CY_WCM_SECURITY_OPEN;
    }
    else if(strcmp(auth_str, "wpa2_tkip") == 0)
    {
        return CY_WCM_SECURITY_WPA2_TKIP_PSK;
    }
    else if(strcmp(auth_str, "wpa2_aes") == 0)
    {
        return CY_WCM_SECURITY_WPA2_AES_PSK;
    }
    else if(strcmp(auth_str, "wpa2_aes_sha256") == 0)
    {
        return CY_WCM_SECURITY_WPA2_AES_PSK_SHA256;
    }
    else if(strcmp(auth_str, "wpa2") == 0)
    {
        return CY_WCM_SECURITY_WPA2_MIXED_PSK;
    }
    else if(strcmp(auth_str, "wpa_aes" ) == 0)
    {
        return CY_WCM_SECURITY_WPA_AES_PSK;
    }
    else if (strcmp(auth_str, "wpa_tkip") == 0)
    {
        return CY_WCM_SECURITY_WPA_TKIP_PSK;
    }
    else if(strcmp(auth_str, "wpa3") == 0)
    {
        return CY_WCM_SECURITY_WPA3_SAE;
    }
    else if(strcmp(auth_str, "wpa3_wpa2") == 0)
    {
        return CY_WCM_SECURITY_WPA3_WPA2_PSK;
    }
    else
    {
        WIFI_ERROR(("Unsupported auth type: '%s'\r\n", auth_str));
        return CY_WCM_SECURITY_UNKNOWN;
    }
}

const char* wifi_utils_authtype_to_str(cy_wcm_security_t sec)
{
    switch (sec)
    {
        case CY_WCM_SECURITY_OPEN:
            return "open";
        case CY_WCM_SECURITY_WEP_PSK:
        case CY_WCM_SECURITY_WEP_SHARED:
            return "wep";
        case CY_WCM_SECURITY_WPA2_MIXED_PSK:
            return "wpa2";
        case CY_WCM_SECURITY_WPA_AES_PSK:
            return "wpa_aes";
        case CY_WCM_SECURITY_WPA2_AES_PSK_SHA256:
            return "wpa2_aes_sha256";
        case CY_WCM_SECURITY_WPA2_AES_PSK:
            return "wpa2_aes";
        case CY_WCM_SECURITY_WPA_TKIP_PSK:
            return "wpa_tkip";
        case CY_WCM_SECURITY_WPA2_TKIP_PSK:
            return "wpa2_tkip";
        case CY_WCM_SECURITY_WPA3_SAE:
            return "wpa3";
        case CY_WCM_SECURITY_WPA3_WPA2_PSK:
            return "wpa3_wpa2";
        case CY_WCM_SECURITY_WPA_AES_ENT:
            return "wpa_aes_ent";
        case CY_WCM_SECURITY_WPA_MIXED_ENT:
            return "wpa_mixed_ent";
        case CY_WCM_SECURITY_WPA2_TKIP_ENT:
            return "wpa2_tkip_ent";
        case CY_WCM_SECURITY_WPA2_AES_ENT:
            return "wpa2_aes_ent";
        case CY_WCM_SECURITY_WPA2_MIXED_ENT:
            return "wpa2_mixed_ent";
        case CY_WCM_SECURITY_WPA2_FBT_ENT:
            return "wpa2_fbt_ent";
#ifdef ENABLE_ENTPS_FEATURE
        case CY_WCM_SECURITY_WPA3_192BIT_ENT:
            return "wpa3_192bit_ent";
        case CY_WCM_SECURITY_WPA3_ENT:
            return "wpa3_aes_gcm_256_ent";
        case CY_WCM_SECURITY_WPA3_ENT_AES_CCMP:
            return "wpa3_aes_ccm_128_ent";
#endif
        case CY_WCM_SECURITY_UNKNOWN:
            return "Unknown";
        default:
            return "Unsupported";
    }
}

static void print_ip4(uint32_t ip)
{
    unsigned char bytes[CMD_CONSOLE_IPV4_ADDR_SIZE];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;

    WIFI_INFO(("IP Address: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]));
}

#ifdef __cplusplus
}
#endif
#endif
