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

#ifndef LIBS_WCM_INCLUDE_CY_WCM_ERROR_H_
#define LIBS_WCM_INCLUDE_CY_WCM_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cy_result.h"
#include "cy_result_mw.h"

/**
 * @defgroup cy_wcm_error WCM-specific error codes
 * @ingroup group_wcm_macros
 * Infineon middleware APIs return results of type cy_rslt_t.
 *
 * It consists of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result format
 *
   \verbatim
              Module base                   Type    Library-specific error code
      +-----------------------------------+------+------------------------------+
      |CY_RSLT_MODULE_WCM_BASE            | 0x2  |           Error code         |
      +-----------------------------------+------+------------------------------+
                14-bits                    2-bits            16-bits

   See the macro section of this document for library-specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h located in <core_lib/include>.
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of the CY_RSLT_MODULE_MIDDLEWARE_BASE.
 *              The details of the offset and the middleware base are defined in cy_result_mw.h, that is part of [GitHub connectivity-utilities] (https://github.com/infineon/connectivity-utilities)
 *              For example, Wi-Fi Connection Manager (WCM) uses CY_RSLT_MODULE_WCM_BASE as the module base.
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING, or CY_RSLT_TYPE_INFO. AWS library error codes are of type CY_RSLT_TYPE_ERROR.
 *
 * Library-specific error codes: These error codes are library-specific and defined in the macro section.
 *
 * Helper macros used for creating the library-specific result are provided as part of cy_result.h.
 * \{
 */
/** Generic WCM base error code */
#define CY_RSLT_WCM_ERR_BASE                   CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_WCM_BASE, 0)

/** WCM error codes */
#define CY_RSLT_WCM_WAIT_TIMEOUT                           (CY_RSLT_WCM_ERR_BASE + 1)  /**< Wait timeout.                                 */
#define CY_RSLT_WCM_BAD_NETWORK_PARAM                      (CY_RSLT_WCM_ERR_BASE + 2)  /**< Bad network parameters.                       */
#define CY_RSLT_WCM_BAD_SSID_LEN                           (CY_RSLT_WCM_ERR_BASE + 3)  /**< Bad SSID length.                              */
#define CY_RSLT_WCM_SECURITY_NOT_SUPPORTED                 (CY_RSLT_WCM_ERR_BASE + 4)  /**< Security not supported.                       */
#define CY_RSLT_WCM_BAD_PASSPHRASE_LEN                     (CY_RSLT_WCM_ERR_BASE + 5)  /**< Bad passphrase length.                        */
#define CY_RSLT_WCM_BAD_ARG                                (CY_RSLT_WCM_ERR_BASE + 6)  /**< Bad argument.                                 */
#define CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED                (CY_RSLT_WCM_ERR_BASE + 7)  /**< Interface type not supported.                 */
#define CY_RSLT_WCM_MUTEX_ERROR                            (CY_RSLT_WCM_ERR_BASE + 8)  /**< Mutex error.                                  */
#define CY_RSLT_WCM_STA_DISCONNECT_ERROR                   (CY_RSLT_WCM_ERR_BASE + 9)  /**< STA disconnect error.                         */
#define CY_RSLT_WCM_STA_NETWORK_DOWN                       (CY_RSLT_WCM_ERR_BASE + 10) /**< STA network is down.                          */
#define CY_RSLT_WCM_BSP_INIT_ERROR                         (CY_RSLT_WCM_ERR_BASE + 11) /**< CY BSP initialization error.                  */
#define CY_RSLT_WCM_BSP_DEINIT_ERROR                       (CY_RSLT_WCM_ERR_BASE + 12) /**< CY BSP error while de-initialization.         */
#define CY_RSLT_WCM_NO_ACTIVE_SCAN                         (CY_RSLT_WCM_ERR_BASE + 13) /**< No active scan running currently.             */
#define CY_RSLT_WCM_SCAN_IN_PROGRESS                       (CY_RSLT_WCM_ERR_BASE + 14) /**< Scan in progress.                             */
#define CY_RSLT_WCM_SCAN_ERROR                             (CY_RSLT_WCM_ERR_BASE + 15) /**< Scan error.                                   */
#define CY_RSLT_WCM_STOP_SCAN_ERROR                        (CY_RSLT_WCM_ERR_BASE + 16) /**< Stop scan error.                              */
#define CY_RSLT_WCM_BAND_NOT_SUPPORTED                     (CY_RSLT_WCM_ERR_BASE + 17) /**< BAND not supported.                           */
#define CY_RSLT_WCM_OUT_OF_MEMORY                          (CY_RSLT_WCM_ERR_BASE + 18) /**< WCM out of memory error.                      */
#define CY_RSLT_WCM_CHANNEL_ERROR                          (CY_RSLT_WCM_ERR_BASE + 19) /**< Error in retrieving the Wi-Fi channel.        */
#define CY_RSLT_WCM_NETIF_DOES_NOT_EXIST                   (CY_RSLT_WCM_ERR_BASE + 20) /**< Network interface does not exist.             */
#define CY_RSLT_WCM_ARP_REQUEST_FAILURE                    (CY_RSLT_WCM_ERR_BASE + 21) /**< Error returned for ARP request failure.       */
#define CY_RSLT_WCM_IPV6_GLOBAL_ADDRESS_NOT_SUPPORTED      (CY_RSLT_WCM_ERR_BASE + 22) /**< IPv6 global IP not supported.                 */
#define CY_RSLT_WCM_IPV6_INTERFACE_NOT_READY               (CY_RSLT_WCM_ERR_BASE + 23) /**< IPv6 interface not ready.                     */
#define CY_RSLT_WCM_PING_FAILURE                           (CY_RSLT_WCM_ERR_BASE + 24) /**< Failure in executing ping.                    */
#define CY_RSLT_WCM_PING_REQUEST_TIMEOUT                   (CY_RSLT_WCM_ERR_BASE + 25) /**< Ping request timed out.                       */
#define CY_RSLT_WCM_STATIC_IP_NOT_SUPPORTED                (CY_RSLT_WCM_ERR_BASE + 26) /**< Static IP address not supported for IPv6.     */
#define CY_RSLT_WCM_BAD_STATIC_IP                          (CY_RSLT_WCM_ERR_BASE + 27) /**< Bad Static IP address.                        */
#define CY_RSLT_WCM_SECONDARY_INTERFACE_ERROR              (CY_RSLT_WCM_ERR_BASE + 28) /**< Error in bringing up the secondary interface. */
#define CY_RSLT_WCM_AP_NETWORK_BRINGUP_ERROR               (CY_RSLT_WCM_ERR_BASE + 29) /**< AP network bring up error.                    */
#define CY_RSLT_WCM_AP_BAD_CHANNEL                         (CY_RSLT_WCM_ERR_BASE + 30) /**< Bad AP channel number.                        */
#define CY_RSLT_WCM_AP_IE_REMOVAL_ERROR                    (CY_RSLT_WCM_ERR_BASE + 31) /**< AP IE removal error.                          */
#define CY_RSLT_WCM_INVALID_IE                             (CY_RSLT_WCM_ERR_BASE + 32) /**< Invalid IE.                                   */
#define CY_RSLT_WCM_AP_NOT_UP                              (CY_RSLT_WCM_ERR_BASE + 33) /**< SoftAP is not started.                        */
#define CY_RSLT_WCM_AP_ALREADY_UP                          (CY_RSLT_WCM_ERR_BASE + 34) /**< SoftAP is already started                     */
#define CY_RSLT_WCM_INTERFACE_NOT_UP                       (CY_RSLT_WCM_ERR_BASE + 35) /**< Interface is not initialized.                 */
#define CY_RSLT_WCM_NOT_INITIALIZED                        (CY_RSLT_WCM_ERR_BASE + 36) /**< WCM not initialized.                          */
#define CY_RSLT_WCM_SEMAPHORE_ERROR                        (CY_RSLT_WCM_ERR_BASE + 37) /**< Semaphore error.                              */
#define CY_RSLT_WCM_SECURITY_NOT_FOUND                     (CY_RSLT_WCM_ERR_BASE + 38) /**< Security type could not be determined.        */
#define CY_RSLT_WCM_CONNECT_IN_PROGRESS                    (CY_RSLT_WCM_ERR_BASE + 39) /**< Connect to AP is in progress.                 */
#define CY_RSLT_WCM_GATEWAY_ADDR_ERROR                     (CY_RSLT_WCM_ERR_BASE + 40) /**< Failed to get the Gateway address.            */
#define CY_RSLT_WCM_NETMASK_ADDR_ERROR                     (CY_RSLT_WCM_ERR_BASE + 41) /**< Failed to get the netmask address.            */
#define CY_RSLT_WCM_IP_ADDR_ERROR                          (CY_RSLT_WCM_ERR_BASE + 42) /**< Failed to get the IP address.                 */
#define CY_RSLT_WCM_GATEWAY_MAC_ADDR_ERROR                 (CY_RSLT_WCM_ERR_BASE + 43) /**< Failed to get the Gateway MAC address.        */
#define CY_RSLT_WCM_NW_INIT_ERROR                          (CY_RSLT_WCM_ERR_BASE + 44) /**< Failed to initialize the network stack.       */

/** WPS error codes */
#define CY_RSLT_WCM_WPS_PBC_OVERLAP                        (CY_RSLT_WCM_ERR_BASE + 45) /**< WPS PBC overlap.                              */
#define CY_RSLT_WCM_WPS_ERROR_RECEIVED_WEP_CREDENTIALS     (CY_RSLT_WCM_ERR_BASE + 46) /**< WPS received incorrect credentials.           */
#define CY_RSLT_WCM_WPS_FAILED                             (CY_RSLT_WCM_ERR_BASE + 47) /**< WPS PBC/PIN mode failed.                      */

/** DHCP error code */
#define CY_RSLT_WCM_DHCP_TIMEOUT                           (CY_RSLT_WCM_ERR_BASE + 48)  /**< DHCP timeout.                                */

/** WPA3 supplicant error code */
#define CY_RSLT_WCM_WPA3_SUPPLICANT_ERROR                  (CY_RSLT_WCM_ERR_BASE + 49) /**< WPA3 supplicant error.                        */

/** Unsupported WCM API error code */
#define CY_RSLT_WCM_UNSUPPORTED_API                        (CY_RSLT_WCM_ERR_BASE + 50) /**< Unsupported WCM API.                          */

/** VCM error code */
#define CY_RSLT_WCM_VCM_ERROR                              (CY_RSLT_WCM_ERR_BASE + 51) /**< Failed to execute virtual API using VCM.      */

/** Powersave Mode error code **/
#define CY_RSLT_WCM_POWERSAVE_MODE_NOT_SUPPORTED           (CY_RSLT_WCM_ERR_BASE + 52) /**< Powersave mode not supported on this device.  */

/** Reason codes for disconnection based on WHD enums */
typedef enum
{
    /** roam reason codes */
    CY_WCM_REASON_INITIAL_ASSOC                  = 0,     /**< Initial assoc.                     */
    CY_WCM_REASON_LOW_RSSI                       = 1,     /**< Roamed due to low RSSI.            */
    CY_WCM_REASON_DEAUTH                         = 2,     /**< Roamed due to DEAUTH indication.   */
    CY_WCM_REASON_DISASSOC                       = 3,     /**< Roamed due to DISASSOC indication. */
    CY_WCM_REASON_BCNS_LOST                      = 4,     /**< Roamed due to lost beacons.        */
    CY_WCM_REASON_FAST_ROAM_FAILED               = 5,     /**< Roamed due to fast roam failure.   */
    CY_WCM_REASON_DIRECTED_ROAM                  = 6,     /**< Roamed due to request by AP.       */
    CY_WCM_REASON_TSPEC_REJECTED                 = 7,     /**< Roamed due to TSPEC rejection.     */
    CY_WCM_REASON_BETTER_AP                      = 8,     /**< Roamed due to finding better AP.   */

    /** NAN sub-events comes as a reason code with event as CY_WCM_REASON_NAN */
    CY_WCM_REASON_NAN_EVENT_STATUS_CHG           = 9,     /**< Generated on any change in nan_mac status. */
    CY_WCM_REASON_NAN_EVENT_MERGE                = 10,    /**< Merged to a NAN cluster.                   */
    CY_WCM_REASON_NAN_EVENT_STOP                 = 11,    /**< NAN stopped.                               */
    CY_WCM_REASON_NAN_EVENT_P2P                  = 12,    /**< NAN P2P EVENT.                             */

    /** XXX: Don't use the following  four events: They will be cleaned up; use WL_NAN_EVENT_POST_DISC. */
    CY_WCM_REASON_NAN_EVENT_WINDOW_BEGIN_P2P     = 13,    /**< Event for beginning of P2P further availability window. */
    CY_WCM_REASON_NAN_EVENT_WINDOW_BEGIN_MESH    = 14,
    CY_WCM_REASON_NAN_EVENT_WINDOW_BEGIN_IBSS    = 15,
    CY_WCM_REASON_NAN_EVENT_WINDOW_BEGIN_RANGING = 16,
    CY_WCM_REASON_NAN_EVENT_POST_DISC            = 17,    /**< Event for post discovery data.                      */
    CY_WCM_REASON_NAN_EVENT_DATA_IF_ADD          = 18,    /**< Event for Data IF add.                              */
    CY_WCM_REASON_NAN_EVENT_DATA_PEER_ADD        = 19,    /**< Event for peer add.                                 */

    /** nan 2.0 */
    CY_WCM_REASON_NAN_EVENT_DATA_IND             = 20,    /**< Data indication to host.        */
    CY_WCM_REASON_NAN_EVENT_DATA_CONF            = 21,    /**< Data response to host.          */
    CY_WCM_REASON_NAN_EVENT_SDF_RX               = 22,    /**< Entire service discovery frame. */
    CY_WCM_REASON_NAN_EVENT_DATA_END             = 23,
    CY_WCM_REASON_NAN_EVENT_BCN_RX               = 24,    /**< Received beacon payload.        */

    /** Prune reason codes */
    CY_WCM_REASON_PRUNE_ENCR_MISMATCH            = 257,   /**< Encryption mismatch.                          */
    CY_WCM_REASON_PRUNE_BCAST_BSSID              = 258,   /**< AP uses a broadcast BSSID.                    */
    CY_WCM_REASON_PRUNE_MAC_DENY                 = 259,   /**< STA's MAC address is in AP's MAC deny list.      */
    CY_WCM_REASON_PRUNE_MAC_NA                   = 260,   /**< STA's MAC addrress is not in AP's MAC allow list. */
    CY_WCM_REASON_PRUNE_REG_PASSV                = 261,   /**< AP not allowed due to regulatory restriction. */
    CY_WCM_REASON_PRUNE_SPCT_MGMT                = 262,   /**< AP does not support STA locale spectrum management. */
    CY_WCM_REASON_PRUNE_RADAR                    = 263,   /**< AP is on a radar channel of STA locale.       */
    CY_WCM_REASON_RSN_MISMATCH                   = 264,   /**< STA does not support AP's RSN.                */
    CY_WCM_REASON_PRUNE_NO_COMMON_RATES          = 265,   /**< No rates in common with AP.                   */
    CY_WCM_REASON_PRUNE_BASIC_RATES              = 266,   /**< STA does not support all basic rates of BSS.  */
    CY_WCM_REASON_PRUNE_CCXFAST_PREVAP           = 267,   /**< CCX FAST ROAM: prune previous AP.             */
    CY_WCM_REASON_PRUNE_CIPHER_NA                = 268,   /**< BSS's cipher not supported.                   */
    CY_WCM_REASON_PRUNE_KNOWN_STA                = 269,   /**< AP is already known to us as a STA.           */
    CY_WCM_REASON_PRUNE_CCXFAST_DROAM            = 270,   /**< CCX FAST ROAM: prune unqualified AP.          */
    CY_WCM_REASON_PRUNE_WDS_PEER                 = 271,   /**< AP is already known to us as a WDS peer.      */
    CY_WCM_REASON_PRUNE_QBSS_LOAD                = 272,   /**< QBSS LOAD - AAC is too low.                   */
    CY_WCM_REASON_PRUNE_HOME_AP                  = 273,   /**< Prune home AP.                                */
    CY_WCM_REASON_PRUNE_AP_BLOCKED               = 274,   /**< Prune blocked AP.                             */
    CY_WCM_REASON_PRUNE_NO_DIAG_SUPPORT          = 275,   /**< Prune due to diagnostic mode not supported.   */

    CY_WCM_REASON_SUP_OTHER                      = 512,   /**< Other reason.                              */
    CY_WCM_REASON_SUP_DECRYPT_KEY_DATA           = 513,   /**< Decryption of key data failed.             */
    CY_WCM_REASON_SUP_BAD_UCAST_WEP128           = 514,   /**< Illegal use of ucast WEP-128.               */
    CY_WCM_REASON_SUP_BAD_UCAST_WEP40            = 515,   /**< Illegal use of ucast WEP-40.                */
    CY_WCM_REASON_SUP_UNSUP_KEY_LEN              = 516,   /**< Unsupported key length.                    */
    CY_WCM_REASON_SUP_PW_KEY_CIPHER              = 517,   /**< Unicast cipher mismatch in pairwise key.   */
    CY_WCM_REASON_SUP_MSG3_TOO_MANY_IE           = 518,   /**< WPA IE contains > 1 RSN IE in key message 3.   */
    CY_WCM_REASON_SUP_MSG3_IE_MISMATCH           = 519,   /**< WPA IE mismatch in key message 3.          */
    CY_WCM_REASON_SUP_NO_INSTALL_FLAG            = 520,   /**< INSTALL flag unset in 4-way message.           */
    CY_WCM_REASON_SUP_MSG3_NO_GTK                = 521,   /**< Encapsulated GTK missing from message 3.       */
    CY_WCM_REASON_SUP_GRP_KEY_CIPHER             = 522,   /**< Multicast cipher mismatch in group key.    */
    CY_WCM_REASON_SUP_GRP_MSG1_NO_GTK            = 523,   /**< Encapsulated GTK missing from group message 1. */
    CY_WCM_REASON_SUP_GTK_DECRYPT_FAIL           = 524,   /**< GTK decrypt failure.                       */
    CY_WCM_REASON_SUP_SEND_FAIL                  = 525,   /**< Message send failure.                      */
    CY_WCM_REASON_SUP_DEAUTH                     = 526,   /**< Received FC_DEAUTH.                        */
    CY_WCM_REASON_SUP_WPA_PSK_TMO                = 527,   /**< WPA PSK 4-way handshake timeout.           */

    CY_WCM_DOT11_RC_RESERVED                     = 768,   /**< d11 RC reserved.                                                           */
    CY_WCM_DOT11_RC_UNSPECIFIED                  = 769,   /**< Unspecified reason.                                                       */
    CY_WCM_DOT11_RC_AUTH_INVAL                   = 770,   /**< Previous authentication no longer valid.                                   */
    CY_WCM_DOT11_RC_DEAUTH_LEAVING               = 771,   /**< De-authenticated because sending station is leaving (or has left) IBSS or ESS.  */
    CY_WCM_DOT11_RC_INACTIVITY                   = 772,   /**< Disassociated due to inactivity.                                          */
    CY_WCM_DOT11_RC_BUSY                         = 773,   /**< Disassociated because AP is unable to handle all currently associated stations.    */
    CY_WCM_DOT11_RC_INVAL_CLASS_2                = 774,   /**< Class 2 frame received from non-authenticated station.                    */
    CY_WCM_DOT11_RC_INVAL_CLASS_3                = 775,   /**< Class 3 frame received from non-associated station.                       */
    CY_WCM_DOT11_RC_DISASSOC_LEAVING             = 776,   /**< Disassociated because sending station is leaving (or has left) BSS.        */
    CY_WCM_DOT11_RC_NOT_AUTH                     = 777,   /**< Station requesting (re)association is not * authenticated with responding station. */
    CY_WCM_DOT11_RC_BAD_PC                       = 778,   /**< Unacceptable power capability element.                                    */
    CY_WCM_DOT11_RC_BAD_CHANNELS                 = 779,   /**< Unacceptable supported channels element.                                   */

    CY_WCM_DOT11_RC_UNSPECIFIED_QOS              = 800,   /**< Unspecified QoS-related reason.             */
    CY_WCM_DOT11_RC_INSUFFCIENT_BW               = 801,   /**< QAP lacks sufficient bandwidth.             */
    CY_WCM_DOT11_RC_EXCESSIVE_FRAMES             = 802,   /**< Excessive number of frames need ack.        */
    CY_WCM_DOT11_RC_TX_OUTSIDE_TXOP              = 803,   /**< Transmitting outside the limits of txop.    */
    CY_WCM_DOT11_RC_LEAVING_QBSS                 = 804,   /**< QSTA is leaving the QBSS (or restting).     */
    CY_WCM_DOT11_RC_BAD_MECHANISM                = 805,   /**< Does not want to use the mechanism.         */
    CY_WCM_DOT11_RC_SETUP_NEEDED                 = 806,   /**< Mechanism needs a setup.                    */
    CY_WCM_DOT11_RC_TIMEOUT                      = 807,   /**< Timeout.                                    */
    CY_WCM_DOT11_RC_MAX                          = 808,   /**< Reason codes > 23 are reserved.             */

    CY_WCM_REASON_FORCE_32_BIT                   = 0x7FFFFFFE /**< Force enum to be stored in a 32-bit variable. */
} cy_wcm_reason_code;

/** \} error codes */

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* LIBS_WCM_INCLUDE_CY_WCM_ERROR_H_ */
