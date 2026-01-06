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
* @file cy_wcm.h
* @brief Wi-Fi Connection Manager (WCM) is a library that helps application developers manage their Wi-Fi connectivity.
* The library provides a set of APIs that can be used to establish and monitor Wi-Fi connections on Infineon platforms that support Wi-Fi connectivity.
* The library APIs are thread-safe. The library monitors the Wi-Fi connection and notifies the connection state change through an event notification mechanism.
* The library also provides APIs to connect to a Wi-Fi network using Wi-Fi Protected Setup (WPS) methods.
* See individual APIs for more details.
* WPS is disabled by default. WPS uses the Mbed TLS security stack. Enable the following components for WPS:
* COMPONENTS+=WPS MBEDTLS
*
* The library supports multi-core architecture by making a subset of APIs available as virtual APIs.
* The virtualization of the WCM library helps to access the WCM APIs defined in one core from the other core using Inter Process Communication (IPC).
* The WCM can now be run on two cores simultaneously, with one core containing the full connectivity stack (primary core) and the other core containing the subset of virtual-only APIs (secondary core).
* The virtual APIs pipe the API requests over IPC to the primary core where the API is actually executed and the result is passed back to the secondary core.
* This virtualization abstracts out the implementation details and complexity of IPC, thus making multi-core connectivity application development simple.
*
* The following APIs are available as virtual APIs on the secondary core:
* - cy_wcm_is_connected_to_ap
* - cy_wcm_register_event_callback
* - cy_wcm_deregister_event_callback
*
* In order to use the virtual APIs, the Virtual Connectivity Manager (VCM) library needs to be included and initialized first.
* Define the following compile-time macros in the secondary core's application:
* DEFINES+=ENABLE_MULTICORE_CONN_MW USE_VIRTUAL_API
* ENABLE_MULTICORE_CONN_MW must be defined on both applications. For more details, see the quick start guide.
*
* For more information on virtualization, see the [Virtual Connectivity Manager](https://github.com/Infineon/virtual-connectivity-manager) library.
*/


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef COMPONENT_CAT5
#define ENABLE_ENTPS_FEATURE 
#endif /* COMPONENT_CAT5 */

#include "cy_result.h"
#include "cy_wcm_error.h"
#include "whd_types.h"

#if defined(__ICCARM__)
#define CYPRESS_WEAK            __WEAK
#define CYPRESS_PACKED(struct)  __packed struct
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define CYPRESS_WEAK            __attribute__((weak))
#define CYPRESS_PACKED(struct)  struct __attribute__((packed))
#else
#define CYPRESS_WEAK           __attribute__((weak))
#define CYPRESS_PACKED(struct) struct __attribute__((packed))
#endif  /* defined(__ICCARM__) */

/**
 * \defgroup group_wcm_mscs Message sequence charts
 * \defgroup group_wcm_macros Macros
 * \defgroup group_wcm_enums Enumerated types
 * \defgroup group_wcm_typedefs Typedefs
 * \defgroup group_wcm_structures Structures
 * \defgroup group_wcm_functions Functions
 */

/**
*
* \addtogroup group_wcm_mscs
* \{
*
********************************************************************************
* \section section_wps_enrollee WPS Enrollee
********************************************************************************
*
* \image html uml_wps_enrollee.png
*
* \}
*
*/

/**
 * \addtogroup group_wcm_macros
 * \{
 */

/******************************************************
 *                    Constants
 ******************************************************/
#define CY_WCM_MAX_SSID_LEN                (32)        /**< Max SSID length.                            */
#define CY_WCM_MAX_PASSPHRASE_LEN          (63)        /**< Max passphrase length.                      */
#define CY_WCM_MIN_PASSPHRASE_LEN          (8)         /**< Min passphrase length.                      */
#define CY_WCM_MAC_ADDR_LEN                (6)         /**< MAC address length.                         */
#define CY_WCM_MAX_IE_LENGTH               (3)         /**< Maximum Length of Information Element       */
#define WEP_ENABLED                        0x0001      /**< Flag to enable WEP security.                */
#define TKIP_ENABLED                       0x0002      /**< Flag to enable TKIP encryption.             */
#define AES_ENABLED                        0x0004      /**< Flag to enable AES encryption.              */
#define SHARED_ENABLED                     0x00008000  /**< Flag to enable shared key security.         */
#define WPA_SECURITY                       0x00200000  /**< Flag to enable WPA security.                */
#define WPA2_SECURITY                      0x00400000  /**< Flag to enable WPA2 security.               */
#ifndef ENABLE_ENTPS_FEATURE
#define WPA2_SHA256_SECURITY               0x00800000  /**< Flag to enable WPA2 SHA256 Security         */
#endif
#define WPA3_SECURITY                      0x01000000  /**< Flag to enable WPA3 PSK security.           */
#define WPA3_OWE                           0x80000000  /**< Flag to enable WPA3 OWE Security            */
#define ENTERPRISE_ENABLED                 0x02000000  /**< Flag to enable enterprise security.         */
#ifdef ENABLE_ENTPS_FEATURE
#define SHA256_1X                          0x04000000  /**< Flag 1X with SHA256 key derivation          */
#define SUITE_B_SHA384                     0x08000000  /**< Flag to enable Suite B-192 SHA384 Security  */
#endif
#define WPS_ENABLED                        0x10000000  /**< Flag to enable WPS security.                */
#define IBSS_ENABLED                       0x20000000  /**< Flag to enable IBSS mode.                   */
#define FBT_ENABLED                        0x40000000  /**< Flag to enable FBT.                         */

/** WPS password length for PIN mode. */
#define CY_WCM_WPS_PIN_LENGTH              (9)

/** Maximum number of callbacks that can be registered with the WCM library. */
#define CY_WCM_MAXIMUM_CALLBACKS_COUNT     (5)

/** \} group_wcm_macros */

/**
 * \addtogroup group_wcm_enums
 * \{
 */

/******************************************************
 *            Enumerations
 ******************************************************/

/**
 * IP Version
 */
typedef enum
{
    CY_WCM_IP_VER_V4 = 4,      /**< Denotes IPv4 version. */
    CY_WCM_IP_VER_V6 = 6       /**< Denotes IPv6 version. */
} cy_wcm_ip_version_t;

/**
 * IPV6 types
 */
typedef enum
{
    CY_WCM_IPV6_LINK_LOCAL = 0,  /**< Denotes IPv6 link-local address type. */
    CY_WCM_IPV6_GLOBAL           /**< Denotes IPv6 global address type. */
} cy_wcm_ipv6_type_t;

/**
 * Enumeration of Wi-Fi Security Modes
 */
typedef enum
{
    CY_WCM_SECURITY_OPEN                = 0,                                                                   /**< Open security.                                         */
    CY_WCM_SECURITY_WEP_PSK             = WEP_ENABLED,                                                         /**< WEP PSK security with open authentication.             */
    CY_WCM_SECURITY_WEP_SHARED          = ( WEP_ENABLED   | SHARED_ENABLED ),                                  /**< WEP PSK security with shared authentication.           */
    CY_WCM_SECURITY_WPA_TKIP_PSK        = ( WPA_SECURITY  | TKIP_ENABLED ),                                    /**< WPA PSK security with TKIP.                            */
    CY_WCM_SECURITY_WPA_AES_PSK         = ( WPA_SECURITY  | AES_ENABLED ),                                     /**< WPA PSK security with AES.                             */
    CY_WCM_SECURITY_WPA_MIXED_PSK       = ( WPA_SECURITY  | AES_ENABLED | TKIP_ENABLED ),                      /**< WPA PSK security with AES and TKIP.                    */
    CY_WCM_SECURITY_WPA2_AES_PSK        = ( WPA2_SECURITY | AES_ENABLED ),                                     /**< WPA2 PSK security with AES.                            */
#ifndef ENABLE_ENTPS_FEATURE
    CY_WCM_SECURITY_WPA2_AES_PSK_SHA256 = ( WPA2_SECURITY | WPA2_SHA256_SECURITY | AES_ENABLED ),              /**< WPA2 PSK SHA256 Security with AES                      */
#else
    CY_WCM_SECURITY_WPA2_AES_PSK_SHA256 = ( WPA2_SECURITY | SHA256_1X | AES_ENABLED ),                         /**< WPA2 PSK SHA256 Security with AES                      */
#endif
    CY_WCM_SECURITY_WPA2_TKIP_PSK       = ( WPA2_SECURITY | TKIP_ENABLED ),                                    /**< WPA2 PSK security with TKIP.                           */
    CY_WCM_SECURITY_WPA2_MIXED_PSK      = ( WPA2_SECURITY | AES_ENABLED | TKIP_ENABLED ),                      /**< WPA2 PSK security with AES and TKIP.                   */
    CY_WCM_SECURITY_WPA2_FBT_PSK        = ( WPA2_SECURITY | AES_ENABLED | FBT_ENABLED),                        /**< WPA2 FBT PSK security with AES and TKIP.               */
    CY_WCM_SECURITY_WPA3_SAE            = ( WPA3_SECURITY | AES_ENABLED ),                                     /**< WPA3 security with AES.                                */
    CY_WCM_SECURITY_WPA2_WPA_AES_PSK    = (WPA2_SECURITY | WPA_SECURITY | AES_ENABLED),                        /**< WPA2 WPA PSK Security with AES                         */
    CY_WCM_SECURITY_WPA2_WPA_MIXED_PSK  = (WPA2_SECURITY | WPA_SECURITY | AES_ENABLED | TKIP_ENABLED),         /**< WPA2 WPA PSK Security with AES & TKIP.                 */
    CY_WCM_SECURITY_WPA3_WPA2_PSK       = ( WPA3_SECURITY | WPA2_SECURITY | AES_ENABLED ),                     /**< WPA3 WPA2 PSK security with AES.                       */
    CY_WCM_SECURITY_WPA_TKIP_ENT        = (ENTERPRISE_ENABLED | WPA_SECURITY | TKIP_ENABLED),                  /**< WPA Enterprise Security with TKIP.                     */
    CY_WCM_SECURITY_WPA_AES_ENT         = (ENTERPRISE_ENABLED | WPA_SECURITY | AES_ENABLED),                   /**< WPA Enterprise Security with AES                       */
    CY_WCM_SECURITY_WPA_MIXED_ENT       = (ENTERPRISE_ENABLED | WPA_SECURITY | AES_ENABLED | TKIP_ENABLED),    /**< WPA Enterprise Security with AES and TKIP.             */
    CY_WCM_SECURITY_WPA2_TKIP_ENT       = (ENTERPRISE_ENABLED | WPA2_SECURITY | TKIP_ENABLED),                 /**< WPA2 Enterprise Security with TKIP.                    */
    CY_WCM_SECURITY_WPA2_AES_ENT        = (ENTERPRISE_ENABLED | WPA2_SECURITY | AES_ENABLED),                  /**< WPA2 Enterprise Security with AES.                     */
    CY_WCM_SECURITY_WPA2_MIXED_ENT      = (ENTERPRISE_ENABLED | WPA2_SECURITY | AES_ENABLED | TKIP_ENABLED),   /**< WPA2 Enterprise Security with AES and TKIP.            */
    CY_WCM_SECURITY_WPA2_FBT_ENT        = (ENTERPRISE_ENABLED | WPA2_SECURITY | AES_ENABLED | FBT_ENABLED),    /**< WPA2 Enterprise Security with AES and FBT.             */
#ifdef ENABLE_ENTPS_FEATURE
    CY_WCM_SECURITY_WPA3_192BIT_ENT     = (ENTERPRISE_ENABLED | WPA3_SECURITY | SUITE_B_SHA384 | AES_ENABLED), /**< WPA3 192-BIT Enterprise Security with AES              */
    CY_WCM_SECURITY_WPA3_ENT            = (ENTERPRISE_ENABLED | WPA3_SECURITY | SHA256_1X | AES_ENABLED),      /**< WPA3 Enterprise only with AES GCM-256                  */
    CY_WCM_SECURITY_WPA3_ENT_AES_CCMP   = (ENTERPRISE_ENABLED | WPA3_SECURITY | WPA2_SECURITY | SHA256_1X | AES_ENABLED), /**< WPA3 Enterprise Transition with AES CCM-128 */
#endif
    CY_WCM_SECURITY_IBSS_OPEN           = ( IBSS_ENABLED ),                                                    /**< Open security on IBSS ad hoc network.                  */
    CY_WCM_SECURITY_WPS_SECURE          = ( WPS_ENABLED | AES_ENABLED),                                        /**< WPS with AES security.                                 */
#ifdef COMPONENT_WIFI6
    CY_WCM_SECURITY_OWE                 = ( WPA3_OWE | AES_ENABLED ),                                          /**< Enhanced open with AES security                        */
#endif

    CY_WCM_SECURITY_UNKNOWN             = -1,                                                                  /**< Returned by \ref cy_wcm_scan_result_callback_t if security is unknown. Do not pass this to the join function! */

    CY_WCM_SECURITY_FORCE_32_BIT        = 0x7fffffff                                                           /**< Exists only to force whd_security_t type to 32 bits.   */
} cy_wcm_security_t;

/**
 * Enumeration of 802.11 radio bands
 */
typedef enum
{
    CY_WCM_WIFI_BAND_ANY = 0,     /**< Platform will choose an available band.  */
    CY_WCM_WIFI_BAND_5GHZ,        /**< 5-GHz radio band.                            */
    CY_WCM_WIFI_BAND_2_4GHZ,      /**< 2.4-GHz radio band.                          */
    CY_WCM_WIFI_BAND_6GHZ,        /**< 6-GHz radio band.                            */
} cy_wcm_wifi_band_t;

/**
 * Enumeration of TWT profile type
 */
typedef enum
{
    CY_WCM_ITWT_PROFILE_NONE = 0,  /**< TWT is disabled.  */
    CY_WCM_ITWT_PROFILE_IDLE,      /**< Idle profile.     */
    CY_WCM_ITWT_PROFILE_ACTIVE,    /**< Active profile.   */
} cy_wcm_itwt_profile_t;

/**
 * Enumeration of RSSI range
 */
typedef enum
{
    CY_WCM_SCAN_RSSI_FAIR      = -90,      /**< Wi-Fi RSSI values greater than -90 dBm. */
    CY_WCM_SCAN_RSSI_GOOD      = -60,      /**< Wi-Fi RSSI values greater than -60 dBm. */
    CY_WCM_SCAN_RSSI_EXCELLENT = -40       /**< Wi-Fi RSSI values greater than -40 dBm. */
} cy_wcm_scan_rssi_range_t;


/**
 * Enumeration of WCM interfaces types
 */
typedef enum
{
    CY_WCM_INTERFACE_TYPE_STA = 0,    /**< STA or client interface. */
    CY_WCM_INTERFACE_TYPE_AP,         /**< SoftAP interface. */
    CY_WCM_INTERFACE_TYPE_AP_STA      /**< Concurrent AP + STA mode. */
} cy_wcm_interface_t;

/**
 * Enumeration of WCM Powersave Modes
 */
typedef enum
{
    CY_WCM_NO_POWERSAVE_MODE  = NO_POWERSAVE_MODE,   /**< No power savings */
    CY_WCM_PM1_POWERSAVE_MODE = PM1_POWERSAVE_MODE,  /**< Powersave mode on specified interface without regard for throughput reduction */
    CY_WCM_PM2_POWERSAVE_MODE = PM2_POWERSAVE_MODE,  /**< Powersave mode on specified interface with High throughput */
} cy_wcm_powersave_mode_t;

/**
 * Enumeration of scan status
 */
typedef enum
{
    CY_WCM_SCAN_INCOMPLETE,                /**< Scan is in progress; more scan results will be returned. */
    CY_WCM_SCAN_COMPLETE                   /**< Scan is completed. */
} cy_wcm_scan_status_t;

/**
 * Enumeration of WPS connection modes
 */
typedef enum
{
    CY_WCM_WPS_PBC_MODE,  /**< Push button mode. */
    CY_WCM_WPS_PIN_MODE   /**< PIN mode.         */
} cy_wcm_wps_mode_t;

/**
 * Enumeration of WPS configuration method
 */
typedef enum
{
    CY_WCM_WPS_CONFIG_USBA                  = 0x0001, /**< USB configuration. */
    CY_WCM_WPS_CONFIG_ETHERNET              = 0x0002, /**< Ethernet configuration. */
    CY_WCM_WPS_CONFIG_LABEL                 = 0x0004, /**< Label configuration. */
    CY_WCM_WPS_CONFIG_DISPLAY               = 0x0008, /**< Display configuration. */
    CY_WCM_WPS_CONFIG_EXTERNAL_NFC_TOKEN    = 0x0010, /**< External NFC configuration. */
    CY_WCM_WPS_CONFIG_INTEGRATED_NFC_TOKEN  = 0x0020, /**< Internal NFC configuration. */
    CY_WCM_WPS_CONFIG_NFC_INTERFACE         = 0x0040, /**< NFC interface. */
    CY_WCM_WPS_CONFIG_PUSH_BUTTON           = 0x0080, /**< Push button configuration. */
    CY_WCM_WPS_CONFIG_KEYPAD                = 0x0100, /**< Keypad configuration. */
    CY_WCM_WPS_CONFIG_VIRTUAL_PUSH_BUTTON   = 0x0280, /**< Virtual push button configuration. */
    CY_WCM_WPS_CONFIG_PHYSICAL_PUSH_BUTTON  = 0x0480, /**< Physical push button configuration. */
    CY_WCM_WPS_CONFIG_VIRTUAL_DISPLAY_PIN   = 0x2008, /**< Virtual display pin configuration. */
    CY_WCM_WPS_CONFIG_PHYSICAL_DISPLAY_PIN  = 0x4008  /**< Physical display pin configuration. */
} cy_wcm_wps_configuration_method_t;


/**
 * Enumeration of WPS authentication types
 */
typedef enum
{
    CY_WCM_WPS_OPEN_AUTHENTICATION               = 0x0001, /**< Authentication type OPEN. */
    CY_WCM_WPS_WPA_PSK_AUTHENTICATION            = 0x0002, /**< WPA-PSK authentication type - Deprecated in version 2.0. */
    CY_WCM_WPS_SHARED_AUTHENTICATION             = 0x0004, /**< WPS-SHARED authentication type - Deprecated in version 2.0. */
    CY_WCM_WPS_WPA_ENTERPRISE_AUTHENTICATION     = 0x0008, /**< WPA-ENTERPRISE authentication type - Deprecated in version 2.0. */
    CY_WCM_WPS_WPA2_ENTERPRISE_AUTHENTICATION    = 0x0010, /**< WPA2-ENTERPRISE authentication type. */
    CY_WCM_WPS_WPA2_PSK_AUTHENTICATION           = 0x0020, /**< WPA2-PSK authentication type. */
    CY_WCM_WPS_WPA2_WPA_PSK_MIXED_AUTHENTICATION = 0x0022, /**< WPA2-WPA-PSK authentication type. */
} cy_wcm_wps_authentication_type_t;

/**
 * Enumeration of WPS encryption type
 */
typedef enum
{
    CY_WCM_WPS_MIXED_ENCRYPTION = 0x000c, /**< MIXED encryption. */
    CY_WCM_WPS_AES_ENCRYPTION   = 0x0008, /**< AES encryption. */
    CY_WCM_WPS_TKIP_ENCRYPTION  = 0x0004, /**< TKIP encryption - Deprecated in WSC 2.0. */
    CY_WCM_WPS_WEP_ENCRYPTION   = 0x0002, /**< WEP encryption - Deprecated in WSC 2.0. */
    CY_WCM_WPS_NO_ENCRYPTION    = 0x0001, /**< OPEN - No encryption. */
    CY_WCM_WPS_NO_UNDEFINED     = 0x0000, /**< Undefined encryption type. */
} cy_wcm_wps_encryption_type_t;

/**
 * Enumeration of WPS device category from the WSC 2.0 spec
 */
typedef enum
{
    CY_WCM_WPS_DEVICE_COMPUTER               = 1,     /**< Computer devices.               */
    CY_WCM_WPS_DEVICE_INPUT                  = 2,     /**< Input devices.                  */
    CY_WCM_WPS_DEVICE_PRINT_SCAN_FAX_COPY    = 3,     /**< Devices such as printers, scanners, faxes and copiers.    */
    CY_WCM_WPS_DEVICE_CAMERA                 = 4,     /**< Camera devices.                 */
    CY_WCM_WPS_DEVICE_STORAGE                = 5,     /**< Storage devices.                */
    CY_WCM_WPS_DEVICE_NETWORK_INFRASTRUCTURE = 6,     /**< Network infrastructure devices. */
    CY_WCM_WPS_DEVICE_DISPLAY                = 7,     /**< Display devices.                */
    CY_WCM_WPS_DEVICE_MULTIMEDIA             = 8,     /**< Multimedia devices.             */
    CY_WCM_WPS_DEVICE_GAMING                 = 9,     /**< Gaming devices.                 */
    CY_WCM_WPS_DEVICE_TELEPHONE              = 10,    /**< Telephony devices.              */
    CY_WCM_WPS_DEVICE_AUDIO                  = 11,    /**< Audio devices.                  */
    CY_WCM_WPS_DEVICE_DOCK                   = 12,    /**< Docking devices.                */
    CY_WCM_WPS_DEVICE_OTHER                  = 0xFF,  /**< Other devices.                  */
} cy_wcm_wps_device_category_t;

/**
 * Enumeration of WCM events
 */
typedef enum
{
    CY_WCM_EVENT_CONNECTING  = 0,    /**< STA connecting to an AP.         */
    CY_WCM_EVENT_CONNECTED,          /**< STA connected to the AP.         */
    CY_WCM_EVENT_CONNECT_FAILED,     /**< STA connection to the AP failed. */
    CY_WCM_EVENT_RECONNECTED,        /**< STA reconnected to the AP.       */
    CY_WCM_EVENT_DISCONNECTED,       /**< STA disconnected from the AP.    */
    CY_WCM_EVENT_IP_CHANGED,         /**< IP address change event. This event is notified after connection, re-connection, and IP address change due to DHCP renewal. */
    CY_WCM_EVENT_INITIATED_RETRY,    /**< Indicates that WCM will initiate a retry logic to re-connect to the AP */
    CY_WCM_EVENT_STA_JOINED_SOFTAP,  /**< An STA device connected to SoftAP. */
    CY_WCM_EVENT_STA_LEFT_SOFTAP    /**< An STA device disconnected from SoftAP. */
} cy_wcm_event_t;

/**
 * Enumeration of scan filter types
 */
typedef enum
{
   CY_WCM_SCAN_FILTER_TYPE_SSID = 0,   /**< SSID-based scan filtering. */
   CY_WCM_SCAN_FILTER_TYPE_MAC,        /**< MAC-based scan filtering.  */
   CY_WCM_SCAN_FILTER_TYPE_BAND,       /**< Band-based scan filtering. */
   CY_WCM_SCAN_FILTER_TYPE_RSSI,       /**< RSSI-based scan filtering. */
}cy_wcm_scan_filter_type_t;

/**
 * Enumeration of network types
 */
typedef enum
{
    CY_WCM_BSS_TYPE_INFRASTRUCTURE =  0, /**< Infrastructure network.                  */
    CY_WCM_BSS_TYPE_ADHOC          =  1, /**< 802.11 ad hoc IBSS network.              */
    CY_WCM_BSS_TYPE_ANY            =  2, /**< Either infrastructure or ad hoc network. */
    CY_WCM_BSS_TYPE_MESH           =  3, /**< 802.11 mesh network.                     */
    CY_WCM_BSS_TYPE_UNKNOWN        = -1  /**< Returned by \ref cy_wcm_scan_result_callback_t if BSS type is unknown. Do not pass this to the Join function. */
} cy_wcm_bss_type_t;

/**
 * Enumeration of applicable packet mask bits for custom information elements (IEs)
 */
typedef enum
{
    CY_WCM_IE_MASK_BEACON         = 0x1,  /**< Denotes mask for beacon packet.                  */
    CY_WCM_IE_MASK_PROBE_RESPONSE = 0x2,  /**< Denotes mask for probe response packet.          */
    CY_WCM_IE_MASK_ASSOC_RESPONSE = 0x4,  /**< Denotes mask for association response packet.    */
    CY_WCM_IE_MASK_AUTH_RESPONSE  = 0x8,  /**< Denotes mask for authentication response packet. */
    CY_WCM_IE_MASK_PROBE_REQUEST  = 0x10, /**< Denotes mask for probe request packet.           */
    CY_WCM_IE_MASK_ASSOC_REQUEST  = 0x20, /**< Denotes mask for association request packet.     */
    CY_WCM_IE_MASK_CUSTOM         = 0x100 /**< Denotes mask for custom IE identifier.           */
} cy_wcm_ie_mask_t;

/** \} group_wcm_enums */

/**
 * \addtogroup group_wcm_typedefs
 * \{
 */
/******************************************************
 *                      Typedefs
 ******************************************************/

typedef uint8_t cy_wcm_ssid_t[CY_WCM_MAX_SSID_LEN + 1];              /**< SSID name (AP name in null-terminated string format). */

typedef uint8_t cy_wcm_mac_t[CY_WCM_MAC_ADDR_LEN];                   /**< Unique 6-byte MAC address represented in network byte order. */

typedef uint8_t cy_wcm_passphrase_t[CY_WCM_MAX_PASSPHRASE_LEN + 1];  /**< Passphrase in null-terminated string format. */

/** \} group_wcm_typedefs */

/**
 * \addtogroup group_wcm_structures
 * \{
 */

/******************************************************
 *             Structures
 ******************************************************/

/**
 * Structure used to pass WPS configuration parameters to \ref cy_wcm_wps_enrollee.
 * Password is mandatory only for CY_WCM_WPS_PIN mode and not used when mode is CY_WCM_WPS_PBC.
 */
typedef struct
{
    cy_wcm_wps_mode_t mode;        /**< WPS mode. */
    char*             password;    /**< Used only for CY_WCM_WPS_PIN mode. */
} cy_wcm_wps_config_t;

/**
 * Structure used to pass WCM configuration to \ref cy_wcm_init.
 */
typedef struct
{
    cy_wcm_interface_t interface;  /**< Interface type. */
} cy_wcm_config_t;


/**
 * Structure used to receive the IP address information from \ref cy_wcm_connect_ap.
 */
typedef struct
{
    cy_wcm_ip_version_t version;  /**< IP version. */
    union
    {
        uint32_t v4;     /**< IPv4 address in network byte order. */
        uint32_t v6[4];  /**< IPv6 address in network byte order. */
    } ip;                /**< IP address bytes. */
} cy_wcm_ip_address_t;

/**
 * Structure used to receive the IP address of the STA or MAC address of the connected STA to SoftAP through the callback registered using \ref cy_wcm_register_event_callback.
 */
typedef union
{
    cy_wcm_ip_address_t ip_addr;  /**< Contains the IP address for the CY_WCM_EVENT_IP_CHANGED event. */
    cy_wcm_mac_t        sta_mac;  /**< MAC address of the STA for the CY_WCM_EVENT_STA_JOINED or CY_WCM_EVENT_STA_LEFT */
    cy_wcm_reason_code  reason;   /**< Reason code which specifies the reason for disconnection. */
} cy_wcm_event_data_t;


/**
 * Structure used for providing the AP credential to connect to a AP using \ref cy_wcm_connect_ap.
 */
typedef struct
{
    cy_wcm_ssid_t        SSID;                /**< SSID of the Wi-Fi network to join; should be a null-terminated string. */
    cy_wcm_passphrase_t  password;            /**< Password needed to join the AP; should be a null-terminated string. */
    cy_wcm_security_t    security;            /**< Wi-Fi Security. @see cy_wcm_security_t. */
} cy_wcm_ap_credentials_t;

/**
 * Structure used to pass the static IP address information to \ref cy_wcm_connect_ap.
 *
 */
typedef struct
{
    cy_wcm_ip_address_t  ip_address;  /**< IP address.      */
    cy_wcm_ip_address_t  gateway;     /**< Gateway address. */
    cy_wcm_ip_address_t  netmask;     /**< Netmask.         */
} cy_wcm_ip_setting_t;

/**
 * Structure used to pass the Wi-Fi connection parameter information to \ref cy_wcm_connect_ap.
 *
 */
typedef struct
{
    cy_wcm_ap_credentials_t  ap_credentials;       /**< AP credentials. */
    cy_wcm_mac_t             BSSID;                /**< MAC address of the AP (optional). */
    cy_wcm_ip_setting_t      *static_ip_settings;  /**< Static IP settings of the device (optional). */
    cy_wcm_wifi_band_t       band;                 /**< Radio band to be connected (optional). */
    cy_wcm_itwt_profile_t    itwt_profile;         /**< Individual TWT profile */
} cy_wcm_connect_params_t;

/**
 * Structure used to pass scan filters to \ref cy_wcm_start_scan.
 */
typedef struct
{
    cy_wcm_scan_filter_type_t      mode;        /**< Scan filter mode. */
    union
    {
        cy_wcm_ssid_t              SSID;        /**< SSID. */
        cy_wcm_mac_t               BSSID;       /**< MAC address of AP. */
        cy_wcm_wifi_band_t         band;        /**< Radio band. */
        cy_wcm_scan_rssi_range_t   rssi_range;  /**< RSSI range. */
    } param;                                    /**< Parameter specific to scan filter mode.  */
} cy_wcm_scan_filter_t;


/**
 * Structure used for storing scan results.
 */
typedef struct
{
    cy_wcm_ssid_t                SSID;             /**< SSID (i.e., name of the AP). In case of a hidden AP, SSID.value will be empty and SSID.length will be 0. */
    cy_wcm_mac_t                 BSSID;            /**< Basic Service Set Identification (BSSID), i.e., MAC address of the AP.              */
    int16_t                      signal_strength;  /**< RSSI in dBm. (<-90=Very poor, >-30=Excellent).                                    */
    uint32_t                     max_data_rate;    /**< Maximum data rate in kbps.         */
    cy_wcm_bss_type_t            bss_type;         /**< Network type.  */
    cy_wcm_security_t            security;         /**< Security type.                                 */
    uint8_t                      channel;          /**< Radio channel that the AP beacon was received on.  */
    cy_wcm_wifi_band_t           band;             /**< Radio band.                                                            */
    uint8_t                      ccode[2];         /**< Two-letter ISO country code in network byte order.  */
    uint8_t                      flags;            /**< Indicates whether the scan results are from the same channel if flag is 1; otherwise from the beacon. */
    uint8_t                      *ie_ptr;          /**< Pointer to the received Beacon/Probe Response IE (Information Element).  */
    uint32_t                     ie_len;           /**< Length of the IE.  */
} cy_wcm_scan_result_t;


/**
 * Structure used to pass the device information to \ref cy_wcm_wps_enrollee.
 */
typedef struct
{
    const cy_wcm_wps_device_category_t device_category; /**< Device category.                */
    const uint16_t sub_category;                        /**< Device sub-category.            */
    const char    *device_name;                         /**< Device name.                    */
    const char    *manufacturer;                        /**< Manufacturer details.           */
    const char    *model_name;                          /**< Model name.                     */
    const char    *model_number;                        /**< Model number.                   */
    const char    *serial_number;                       /**< Serial number.                  */
    const uint32_t config_methods;                      /**< Configuration methods.          */
    const uint32_t os_version;                          /**< Operating system version.       */
    const uint16_t authentication_type_flags;           /**< Supported authentication types. */
    const uint16_t encryption_type_flags;               /**< Supported encryption types.     */
    const uint8_t  add_config_methods_to_probe_resp;    /**< Add configuration methods to probe response for Windows enrollees (Not compliant with WPS 2.0). */
} cy_wcm_wps_device_detail_t;

/**
 * Structure used to receive the AP credential after WPS is completed successfully from \ref cy_wcm_wps_enrollee.
 */
typedef struct
{
    cy_wcm_ssid_t         ssid;                                    /**< AP SSID (name) - must be null-terminated. */
    cy_wcm_security_t     security;                                /**< AP security type.                         */
    cy_wcm_passphrase_t   passphrase;                              /**< AP passphrase - must be null-terminated.  */
} cy_wcm_wps_credential_t;

/**
 * Structure used to receive the information of the associated AP from \ref cy_wcm_get_associated_ap_info().
 */
typedef struct
{
    cy_wcm_ssid_t         SSID;             /**< Associated AP name.                                     */
    cy_wcm_mac_t          BSSID;            /**< BSSID (MAC address) of the associated AP.               */
    cy_wcm_security_t     security;         /**< Security of the associated AP. @see cy_wcm_security_t.  */
    uint16_t              channel_width;    /**< Channel width (bandwidth in MHz) of the AP.             */
    int16_t               signal_strength;  /**< RSSI in dBm. (<-90=Very poor, >-30=Excellent).          */
    uint8_t               channel;          /**< Radio channel of the AP.                                */
} cy_wcm_associated_ap_info_t;

/**
 * Structure used to receive the WLAN statistics of the given interface from the time the WLAN driver is up.
 */
typedef struct
{
    uint32_t rx_bytes;   /**< Total received bytes.           */
    uint32_t tx_bytes;   /**< Total transmitted bytes.        */
    uint32_t rx_packets; /**< Total received WLAN packets.    */
    uint32_t tx_packets; /**< Total transmitted WLAN packets. */
    uint32_t tx_retries; /**< Total transmission retries.     */
    uint32_t tx_failed;  /**< Total failed packets.           */
    uint32_t tx_bitrate; /**< Current transmitted data rate in Kbps */
} cy_wcm_wlan_statistics_t;

/**
 * Structure used to fill the vendor information element (IE) as a part of starting SoftAP. \ref cy_wcm_start_ap.
 */
typedef struct
{
    uint8_t         oui[CY_WCM_MAX_IE_LENGTH];   /**< Unique identifier for the IE. */
    uint8_t         subtype;                     /**< Sub-type of the IE. */
    void            *data;                        /**< Pointer to IE data. */
    uint16_t        length;                      /**< IE data length. */
    uint16_t        ie_packet_mask;              /**< Mask of the IE details to be included \ref cy_wcm_ie_mask_t */
} cy_wcm_custom_ie_info_t;

/**
 * Structure used to configure the access point. \ref cy_wcm_start_ap.
 */
typedef struct
{
    cy_wcm_ap_credentials_t  ap_credentials;      /**< AP credentials. */
    cy_wcm_wifi_band_t       band;                /**< Band for the AP. */
    uint8_t                  channel;             /**< Radio channel of the AP. */
    cy_wcm_ip_setting_t      ip_settings;         /**< IP settings of the AP interface. */
    cy_wcm_custom_ie_info_t  *ie_info;            /**< Optional custom IE information to be added to the SoftAP. */
} cy_wcm_ap_config_t;

/** \} group_wcm_structures */

/**
 * \addtogroup group_wcm_typedefs
 * \{
 */
/**
 * Wi-Fi scan result callback function pointer type.
 *
 * @param[in] result_ptr       : A pointer to the scan result; the scan result will be freed once the callback function returns from the application.
 *                               There will not be any scan result when the scan status is CY_WCM_SCAN_COMPLETE.
 *                               For more details on content of result_ptr, see \ref cy_wcm_scan_result_t.
 * @param[in] user_data        : User-provided data.
 * @param[in] status           : Status of the scan process.
 *                               CY_WCM_SCAN_COMPLETE   : Indicates the scan is completed. In this case, the result_ptr will not contain any results.
 *                               CY_WCM_SCAN_INCOMPLETE : Indicates the scan is in progress. In this case, result_ptr contains one of the scan result.
 *
 *
 * Note: The callback function will be executed in the context of the WCM.
 */
typedef void (*cy_wcm_scan_result_callback_t)( cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status );


/**
 * WCM event callback function pointer type; events are invoked when the WHD posts events to WCM.
 * @param[in] event            : WCM events.
 * @param[in] event_data       : A pointer to the event data. The event data will be freed once the callback returns from the application.
 *
 * Note: The callback function will be executed in the context of the WCM.
 */
typedef void (*cy_wcm_event_callback_t)(cy_wcm_event_t event, cy_wcm_event_data_t *event_data);

/** \} group_wcm_typedefs */
/**
 * \addtogroup group_wcm_functions
 * \{
 * * The WCM library internally creates a thread; the created threads are executed with the "CY_RTOS_PRIORITY_ABOVENORMAL" priority. The definition of the CY_RTOS_PRIORITY_ABOVENORMAL macro is located at "libs/abstraction-rtos/include/COMPONENT_FREERTOS/cyabs_rtos_impl.h".
 * * The WCM APIs are thread-safe.
 * * All the WCM APIs except \ref cy_wcm_start_scan are blocking APIs.
 * * \ref cy_wcm_start_scan is a non-blocking API; scan results are delivered via \ref cy_wcm_scan_result_callback_t.
 * * All application callbacks invoked by the WCM will be running in the context of the WCM; the pointers passed as argument in the callback function will be freed once the function returns.
 * * For the APIs that expect \ref cy_wcm_interface_t as an argument, unless a specific interface type has been called out in the description of the API, any valid WCM interface type can be passed as an argument to the API.
 */

/**
 * Initializes the WCM.
 *
 * This function initializes the WCM resources, WHD, and Wi-Fi transport;
 * turns Wi-Fi on; and starts up the network stack. This function should be called before calling other WCM APIs.
 *
 * This function is supported in multi-core environment and it must be called in secondary core application before invoking any virtual WCM APIs.
 * \note The secondary core can invoke this API by passing the config parameter as NULL.
 *
 * @param[in]  config: The configuration to be initialized.
 *
 * @return CY_RSLT_SUCCESS if WCM initialization was successful; returns \ref cy_wcm_error otherwise.
 *
 */
cy_rslt_t cy_wcm_init(cy_wcm_config_t *config);

/**
 * Shuts down the WCM.
 *
 * This function cleans up all the resources of the WCM and brings down the Wi-Fi driver.
 *
 * This function is supported in multi-core environment and can be invoked in the secondary core application which then cleans up all the resources of WCM in secondary core.
 *
 * \note This API does not bring down the network stack because the default underlying stack does not have
 *       an implementation for deinit. Therefore, the expectation is that \ref cy_wcm_init and this API should
 *       be invoked only once.
 *
 * @return CY_RSLT_SUCCESS if the Wi-Fi module was successfully turned off; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_deinit(void);

/**
 * Performs a Wi-Fi network scan.
 * The scan progressively accumulates results over time and may take between 1 and 10 seconds to complete.
 * The results of the scan will be individually provided to the callback function.
 * This API can be invoked while being connected to an AP.
 *
 *  @param[in]  scan_callback  : Callback function which receives the scan results;
 *                               callback will be executed in the context of the WCM.
 *                               Scan results will be individually provided to this callback function.
 *                               For more details on the scan results, see \ref cy_wcm_scan_result_callback_t.
 *  @param[in]  user_data      : User data to be returned as an argument in the callback function
 *                               when the callback function is invoked.
 *  @param[in]  scan_filter    : Scan filter parameter passed for scanning (optional).
 *
 * @return CY_RSLT_SUCCESS if the Wi-Fi network scan was successful; returns \ref cy_wcm_error otherwise.
 * While a scan is in progress, if the user issues another scan, this API returns "CY_RSLT_WCM_SCAN_IN_PROGRESS".
 *
 */
cy_rslt_t cy_wcm_start_scan(cy_wcm_scan_result_callback_t scan_callback, void *user_data, cy_wcm_scan_filter_t *scan_filter);

/**
 * Stops an ongoing Wi-Fi network scan.
 *
 * @return CY_RSLT_SUCCESS if the Wi-Fi network scan was successful; returns \ref cy_wcm_error otherwise.
 *
 */
cy_rslt_t cy_wcm_stop_scan(void);

/**
 * Connects the STA interface to a AP using the Wi-Fi credentials and configuration parameters provided.
 * On a successful connection to the Wi-Fi network, the API returns the IP address.
 * If the user does not know the security type of the AP then, connect_param.ap_credentials.security must
 * be set to CY_WCM_SECURITY_UNKNOWN so that the library will internally find the security type before
 * connecting to AP.
 *
 * This API is a blocking call; this function additionally performs the following checks:
 * 1) Checks for and ignores duplicate connect requests to an already connected AP.
 * 2) Checks the current connection state; if already connected, disconnects from the current
 *    Wi-Fi network and connects to the new Wi-Fi network.
 * 3) If the user does not know the security type of the AP, the library internally finds the security type.
 *
 * @param[in]   connect_params      : Configuration to join the AP.
 * @param[out]  ip_addr             : Pointer to return the IP address (optional).
 *
 * \note WEP (Wired Equivalent Privacy) security is not supported by this API.
 *       WEP-based authentication types are considered to be weaker security types;
 *       therefore, this function doesn't connect to an AP that is configured with WEP-based authentication.
 *
 * @return CY_RSLT_SUCCESS if connection is successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_connect_ap(cy_wcm_connect_params_t *connect_params, cy_wcm_ip_address_t *ip_addr);

/**
 * Disconnects the STA interface from the currently connected AP.
 *
 * @return CY_RSLT_SUCCESS if disconnection was successful or if the device is already
 * disconnected; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_disconnect_ap(void);

/**
 * Retrieves the IPv4 address of the given interface. See \ref cy_wcm_get_ipv6_addr API to get IPv6 addresses.
 *
 * @param[in]   interface_type  : Type of the WCM interface.
 * @param[out]  ip_addr         : Pointer to an IP address structure (or) an IP address structure array.
 *                                If the given interface is CY_WCM_INTERFACE_TYPE_STA or CY_WCM_INTERFACE_TYPE_AP upon return, index-0 stores the IPv4 address of the interface.
 *                                If the given interface type is CY_WCM_INTERFACE_TYPE_AP_STA, index-0 stores the IPv4 address of the STA interface and index-1 stores the IPv4 address of the AP interface. ip_addr should have enough valid memory to hold two IP address structures.
 *
 * @return CY_RSLT_SUCCESS if IP-address get is successful; returns \ref cy_wcm_error otherwise.

 */
cy_rslt_t cy_wcm_get_ip_addr(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *ip_addr);

/**
 * Retrieves the IPv6 address of the given interface.
 *
 * Note: Currently, this API supports only \ref CY_WCM_IPV6_LINK_LOCAL type.
 *
 * @param[in]   interface_type  : Type of the WCM interface.
 * @param[in]   ipv6_addr_type  : IPv6 address type.
 * @param[out]  ip_addr         : Pointer to an IP address structure (or) an IP address structure array.
 *                                If the given interface is CY_WCM_INTERFACE_TYPE_STA or CY_WCM_INTERFACE_TYPE_AP upon return, index-0 stores the IPv6 link-local address of the interface.
 *                                If the given interface type is CY_WCM_INTERFACE_TYPE_AP_STA, index-0 stores the IPv6 link-local address of the STA interface and index-1 stores the IPv6 link-local address of the AP interface. ip_addr should have enough valid memory to hold two IP address structures.
 *
 * @return CY_RSLT_SUCCESS if IPv6 interface is up and IPv6 address is ready; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_ipv6_addr(cy_wcm_interface_t interface_type, cy_wcm_ipv6_type_t ipv6_addr_type, cy_wcm_ip_address_t *ip_addr);

/**
 * Retrieves the gateway IP address of the given interface.
 *
 * @param[in]   interface_type  : Type of the WCM interface.
 * @param[out]  gateway_addr    : Pointer to a single structure or an array of structures to be filled with the gateway IP address or addresses.
 *                                If the given interface is CY_WCM_INTERFACE_TYPE_STA or CY_WCM_INTERFACE_TYPE_AP upon return, index-0 stores the IPv4 gateway address of the interface.
 *                                If the given interface type is CY_WCM_INTERFACE_TYPE_AP_STA, index-0 stores the IPv4 gateway address of the STA interface and index-1 stores the IPV4 gateway address of the AP interface. gateway_addr should have enough valid memory to hold two IP address structures.
 *                                In future, IPv6 addresses will be supported.
 *
 *
 * @return CY_RSLT_SUCCESS if retrieval of the gateway IP address was successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_gateway_ip_address(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *gateway_addr);


/**
 * Retrieves the subnet mask address of the given interface.
  *
 * @param[in]   interface_type  : Type of the WCM interface.
 * @param[out]  net_mask_addr   : Pointer to a single structure or an array of structures to be filled with the subnet mask address or masks.
 *                                If the given interface is CY_WCM_INTERFACE_TYPE_STA or CY_WCM_INTERFACE_TYPE_AP upon return, index-0 stores the subnet mask address of the interface.
 *                                If the given interface type is CY_WCM_INTERFACE_TYPE_AP_STA, index-0 stores the subnet mask address of the STA interface and index-1 stores the subnet mask address of the AP interface. net_mask_addr should have enough valid memory to hold two IP address structures.
 *
 *
 * @return CY_RSLT_SUCCESS if retrieval of the subnet mask address was successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_ip_netmask(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *net_mask_addr);

/**
 * Retrieves the MAC address of the given interface.
 *
 * @param[in]   interface_type  : Type of the WCM interface.
 * @param[out]  mac_addr        : Pointer to a MAC address structure (or) a MAC address structure array.
 *                                If the given interface is CY_WCM_INTERFACE_TYPE_STA or CY_WCM_INTERFACE_TYPE_AP upon return, index-0 stores the MAC address of the interface.
 *                                If the given interface type is CY_WCM_INTERFACE_TYPE_AP_STA, index-0 stores the MAC address of the STA interface and index-1 stores the MAC address of the AP interface. mac_addr should have enough valid memory to hold two MAC address structures.
 *
 *
 * @return CY_RSLT_SUCCESS if the MAC address get is successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_mac_addr(cy_wcm_interface_t interface_type, cy_wcm_mac_t *mac_addr);

/**
 * Negotiates securely with a Wi-Fi Protected Setup (WPS) Registrar (usually an
 *  AP) and obtains the Wi-Fi network credentials.
 *
 * @param[in]  config               : Pointer to the WPS configuration information.
 * @param[in]  details              : Pointer to a structure containing manufacturing details
 *                                    of this device.
 * @param[out] credentials          : Pointer to an array of credentials structure \ref cy_wcm_wps_credential_t to receive the AP credentials.
 * @param[in, out] credential_count : Upon invocation, this parameter stores the size of the credentials parameter. Upon return, denotes the actual
 *                                    number of credentials returned.
 *
 * @return CY_RSLT_SUCCESS if credentials are retrieved successfully; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_wps_enrollee(cy_wcm_wps_config_t* config, const cy_wcm_wps_device_detail_t *details, cy_wcm_wps_credential_t *credentials, uint16_t *credential_count);

/**
 * Generates a random WPS PIN for PIN mode connection.
 *
 * @param[out]  wps_pin_string  : Pointer to store the WPS PIN as a null-terminated string.
 *
 * @return CY_RSLT_SUCCESS if WPS PIN generated; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_wps_generate_pin(char wps_pin_string[CY_WCM_WPS_PIN_LENGTH]);

/**
 * Registers an event callback to monitor the connection and IP address change events.
 * This is an optional registration; use it if the application needs to monitor events across disconnection and reconnection of the STA interface and notifies the clients which are connected or disconnected from the SoftAP.
 *
 * This API is supported in multi-core environment and can be invoked as a virtual API from the secondary core application.
 *
 * Note: This API is expected to be called typically while being connected to an AP or once the SoftAP is up.
 *
 * @param[in]  event_callback  : Callback function to be invoked for event notification.
 *                               The callback will be executed in the context of the WCM.
 *
 * @return CY_RSLT_SUCCESS if application callback registration was successful; returns \ref cy_wcm_error otherwise.
*/
cy_rslt_t cy_wcm_register_event_callback(cy_wcm_event_callback_t event_callback);

/**
 * De-registers an event callback.
 *
 * This API is supported in multi-core environment and can be invoked as a virtual API from the secondary core application.
 *
 * @param[in]  event_callback  : Callback function to de-register from getting notifications.
 *
 * @return CY_RSLT_SUCCESS if application callback de-registration was successful; returns \ref cy_wcm_error otherwise.
*/
cy_rslt_t cy_wcm_deregister_event_callback(cy_wcm_event_callback_t event_callback);

/**
 * Checks if the STA interface is connected to an AP.
 *
 * This API is supported in multi-core environment and can be invoked as a virtual API from the secondary core application.
 *
 * @return 1 if connected, 0 otherwise.
 */
uint8_t cy_wcm_is_connected_to_ap(void);

/**
 * Retrieves the information such as SSID, BSSID, and other details of the AP to which the STA interface is connected.
 *
 * @param[out] ap_info : Pointer to store the information of the associated AP \ref cy_wcm_associated_ap_info_t.
 *
 * @return CY_RSLT_SUCCESS if retrieving the information of the associated AP was successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_associated_ap_info(cy_wcm_associated_ap_info_t *ap_info);

/**
 * Gets the WLAN statistics of the given interface from the time WLAN driver is up and running.
 *
 * The application would typically use this API to get information such as "total transmitted packets" and "total received packets";
 * for more details, see members of \ref cy_wcm_wlan_statistics_t.
 *
 * @param[in] interface : Type of the WCM interface.
 * @param[in] stat      : Pointer to store the statistics \ref cy_wcm_wlan_statistics_t.
 *
 * @return CY_RSLT_SUCCESS if retrieval of statistics was successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_wlan_statistics(cy_wcm_interface_t interface, cy_wcm_wlan_statistics_t *stat);

/**
 * Retrieves the MAC address of the gateway for STA interface. Uses Address Resolution Protocol (ARP) to retrieve the gateway MAC address.
 *
 * This function is a blocking call and uses an internal timeout while running ARP.
 *
 * @param[out]  mac_addr : Pointer to a MAC address structure which is filled with the gateway's MAC address on successful return.
 *
 * @return CY_RSLT_SUCCESS if retrieval of the gateway MAC address was successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_gateway_mac_address(cy_wcm_mac_t *mac_addr);

/**
 * Sends a ping request to the given IP address. This function is a blocking call; it returns after the specified timeout.
 *
 * @param[in]  interface   : Type of the WCM interface.
 * @param[in]  ip_addr     : Pointer to the destination IP address structure to which the ping request will be sent.
 * @param[in]  timeout_ms  : Ping request timeout in milliseconds.
 * @param[out] elapsed_ms  : Pointer to store the round-trip time (in milliseconds),
 *                           i.e., the time taken to receive the ping response from the destination.
 *
 * @return CY_RSLT_SUCCESS if pinging to the IP address was successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_ping(cy_wcm_interface_t interface, cy_wcm_ip_address_t *ip_addr, uint32_t timeout_ms, uint32_t* elapsed_ms);


/**
 * Starts an infrastructure Wi-Fi network (SoftAP).
 * This API is a blocking call; this function adds the information element to the SoftAP and starts an internal DHCP server.
 *
 * @param[in]   ap_config   : Configuration parameters for the SoftAP.
 *
 * @return CY_RSLT_SUCCESS if SoftAp is started ; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_start_ap(const cy_wcm_ap_config_t *ap_config);

/**
 * Stops the infrastructure Wi-Fi network (SoftAP), removes the information element and stops the internal DHCP server.
 *
 * @return CY_RSLT_SUCCESS if connection is successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_stop_ap(void);

/**
 * Gets the MAC address of the clients associated with the SoftAP.
 *
 * @param[out] sta_list    : Pointer to the MAC address (or) array of MAC addresses. The client's (STA) MAC address is stored on this array before the function returns.
 *
 * @param[in]  num_clients : Length of the array passed in sta_list.
 *
 * \note If the number of connected clients is less than the num_clients, then elements of sta_list beyond number of connected clients will be set to zero.
 *       The maximum number of supported client list varies for different Wi-Fi chips.
 *
 * @return CY_RSLT_SUCCESS If getting the client list is successful; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_get_associated_client_list(cy_wcm_mac_t *sta_list, uint8_t num_clients);

/**
 * Stores the AP settings provided by the user.
 * NOTE: Dotted-decimal format example: 192.168.0.1
 *
 * @param[in] ip_addr       : Pointer to an array containing the IP address of the AP in dotted-decimal format.
 * @param[in] netmask       : Pointer to an array containing the network mask in dotted-decimal format.
 * @param[in] gateway_addr  : Pointer to an array containing the gateway address in dotted-decimal format.
 * @param[in] ver           : IP version. Possible values \ref CY_WCM_IP_VER_V6 or \ref CY_WCM_IP_VER_V4.
 * @param[in] ap_ip         : Pointer to the variable which stores AP settings.
 *
 * @result CY_RSLT_SUCCESS if the AP settings are successfully stored in the user-provided variable; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_set_ap_ip_setting(cy_wcm_ip_setting_t *ap_ip, const char *ip_addr, const char *netmask, const char *gateway_addr, cy_wcm_ip_version_t ver);

/**
 * Sets low power mode capability for WLAN CPU
 *
 * @param[in] mode - can be one of:
 *                  CY_WCM_NO_POWERSAVE_MODE  WLAN CPU will not go into powersave mode
 *                  CY_WCM_PM1_POWERSAVE_MODE Powersave mode on specified interface without regard for throughput reduction
 *                  CY_WCM_PM2_POWERSAVE_MODE Powersave mode on specified interface with High throughput
 *
 * @return CY_RSLT_SUCCESS if the Wi-Fi module mode was successfully changed; returns \ref cy_wcm_error otherwise.
 */
cy_rslt_t cy_wcm_allow_low_power_mode(cy_wcm_powersave_mode_t mode);

/** \} group_wcm_functions */

#ifdef __cplusplus
} /* extern C */
#endif
