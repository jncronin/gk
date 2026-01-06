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
* @file cy_wps_constants.h
* @brief WPS constants
*/
#pragma once

#include "cy_wps_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#define ETHERNET_ADDRESS_LENGTH    (6)

#define WPS_OUI         "\x00\x50\xF2"  /* WPS OUI */
#define WPS_OUI_TYPE        4

/* Data Element Definitions */
#define WPS_ID_AP_CHANNEL         0x1001
#define WPS_ID_ASSOC_STATE        0x1002
#define WPS_ID_AUTH_TYPE          0x1003
#define WPS_ID_AUTH_TYPE_FLAGS    0x1004
#define WPS_ID_AUTHENTICATOR      0x1005
#define WPS_ID_CONFIG_METHODS     0x1008
#define WPS_ID_CONFIG_ERROR       0x1009
#define WPS_ID_CONF_URL4          0x100A
#define WPS_ID_CONF_URL6          0x100B
#define WPS_ID_CONN_TYPE          0x100C
#define WPS_ID_CONN_TYPE_FLAGS    0x100D
#define WPS_ID_CREDENTIAL         0x100E
#define WPS_ID_DEVICE_NAME        0x1011
#define WPS_ID_DEVICE_PWD_ID      0x1012
#define WPS_ID_E_HASH1            0x1014
#define WPS_ID_E_HASH2            0x1015
#define WPS_ID_E_SNONCE1          0x1016
#define WPS_ID_E_SNONCE2          0x1017
#define WPS_ID_ENCR_SETTINGS      0x1018
#define WPS_ID_ENCR_TYPE          0x100F
#define WPS_ID_ENCR_TYPE_FLAGS    0x1010
#define WPS_ID_ENROLLEE_NONCE     0x101A
#define WPS_ID_FEATURE_ID         0x101B
#define WPS_ID_IDENTITY           0x101C
#define WPS_ID_IDENTITY_PROOF     0x101D
#define WPS_ID_KEY_WRAP_AUTH      0x101E
#define WPS_ID_KEY_IDENTIFIER     0x101F
#define WPS_ID_MAC_ADDR           0x1020
#define WPS_ID_MANUFACTURER       0x1021
#define WPS_ID_MSG_TYPE           0x1022
#define WPS_ID_MODEL_NAME         0x1023
#define WPS_ID_MODEL_NUMBER       0x1024
#define WPS_ID_NW_INDEX           0x1026
#define WPS_ID_NW_KEY             0x1027
#define WPS_ID_NW_KEY_INDEX       0x1028
#define WPS_ID_NEW_DEVICE_NAME    0x1029
#define WPS_ID_NEW_PWD            0x102A
#define WPS_ID_OOB_DEV_PWD        0x102C
#define WPS_ID_OS_VERSION         0x102D
#define WPS_ID_POWER_LEVEL        0x102F
#define WPS_ID_PSK_CURRENT        0x1030
#define WPS_ID_PSK_MAX            0x1031
#define WPS_ID_PUBLIC_KEY         0x1032
#define WPS_ID_RADIO_ENABLED      0x1033
#define WPS_ID_REBOOT             0x1034
#define WPS_ID_REGISTRAR_CURRENT  0x1035
#define WPS_ID_REGISTRAR_ESTBLSHD 0x1036
#define WPS_ID_REGISTRAR_LIST     0x1037
#define WPS_ID_REGISTRAR_MAX      0x1038
#define WPS_ID_REGISTRAR_NONCE    0x1039
#define WPS_ID_REQ_TYPE           0x103A
#define WPS_ID_RESP_TYPE          0x103B
#define WPS_ID_RF_BAND            0x103C
#define WPS_ID_R_HASH1            0x103D
#define WPS_ID_R_HASH2            0x103E
#define WPS_ID_R_SNONCE1          0x103F
#define WPS_ID_R_SNONCE2          0x1040
#define WPS_ID_SEL_REGISTRAR      0x1041
#define WPS_ID_SERIAL_NUM         0x1042
#define WPS_ID_SC_STATE           0x1044
#define WPS_ID_SSID               0x1045
#define WPS_ID_TOT_NETWORKS       0x1046
#define WPS_ID_UUID_E             0x1047
#define WPS_ID_UUID_R             0x1048
#define WPS_ID_VENDOR_EXT         0x1049
#define WPS_ID_VERSION            0x104A
#define WPS_ID_X509_CERT_REQ      0x104B
#define WPS_ID_X509_CERT          0x104C
#define WPS_ID_EAP_IDENTITY       0x104D
#define WPS_ID_MSG_COUNTER        0x104E
#define WPS_ID_PUBKEY_HASH        0x104F
#define WPS_ID_REKEY_KEY          0x1050
#define WPS_ID_KEY_LIFETIME       0x1051
#define WPS_ID_PERM_CFG_METHODS   0x1052
#define WPS_ID_SEL_REG_CFG_METHODS 0x1053
#define WPS_ID_PRIM_DEV_TYPE      0x1054
#define WPS_ID_SEC_DEV_TYPE_LIST  0x1055
#define WPS_ID_PORTABLE_DEVICE    0x1056
#define WPS_ID_AP_SETUP_LOCKED    0x1057
#define WPS_ID_APP_LIST           0x1058
#define WPS_ID_EAP_TYPE           0x1059
#define WPS_ID_INIT_VECTOR        0x1060
#define WPS_ID_KEY_PROVIDED_AUTO  0x1061
#define WPS_ID_8021X_ENABLED      0x1062
#define WPS_ID_WEP_TRANSMIT_KEY   0x1064
#define WPS_ID_REQ_DEV_TYPE       0x106A

/* Fixed size elements sizes */
#define WPS_ID_AP_CHANNEL_S             2
#define WPS_ID_ASSOC_STATE_S            2
#define WPS_ID_AUTH_TYPE_S              2
#define WPS_ID_AUTH_TYPE_FLAGS_S        2
#define WPS_ID_AUTHENTICATOR_S          8
#define WPS_ID_CONFIG_METHODS_S         2
#define WPS_ID_CONFIG_ERROR_S           2
#define WPS_ID_CONN_TYPE_S              1
#define WPS_ID_CONN_TYPE_FLAGS_S        1
#define WPS_ID_DEVICE_PWD_ID_S          2
#define WPS_ID_ENCR_TYPE_S              2
#define WPS_ID_ENCR_TYPE_FLAGS_S        2
#define WPS_ID_FEATURE_ID_S             4
#define WPS_ID_MAC_ADDR_S               6
#define WPS_ID_MSG_TYPE_S               1
#define WPS_ID_SC_STATE_S               1
#define WPS_ID_RF_BAND_S                1
#define WPS_ID_OS_VERSION_S             4
#define WPS_ID_VERSION_S                1
#define WPS_ID_SEL_REGISTRAR_S          1
#define WPS_ID_SEL_REG_CFG_METHODS_S    2
#define WPS_ID_REQ_TYPE_S               1
#define WPS_ID_RESP_TYPE_S              1
#define WPS_ID_AP_SETUP_LOCKED_S        1
#define WPS_ID_PRIM_DEV_TYPE_S          8
#define WPS_ID_UUID_S                   16
#define WPS_ID_PUBLIC_KEY_S             192

/* WPS Message Types */
#define WPS_ID_BEACON            0x01
#define WPS_ID_PROBE_REQ         0x02
#define WPS_ID_PROBE_RESP        0x03
#define WPS_ID_MESSAGE_M1        0x04
#define WPS_ID_MESSAGE_M2        0x05
#define WPS_ID_MESSAGE_M2D       0x06
#define WPS_ID_MESSAGE_M3        0x07
#define WPS_ID_MESSAGE_M4        0x08
#define WPS_ID_MESSAGE_M5        0x09
#define WPS_ID_MESSAGE_M6        0x0A
#define WPS_ID_MESSAGE_M7        0x0B
#define WPS_ID_MESSAGE_M8        0x0C
#define WPS_ID_MESSAGE_ACK       0x0D
#define WPS_ID_MESSAGE_NACK      0x0E
#define WPS_ID_MESSAGE_DONE      0x0F
/* WSP private ID for local use */
#define WPS_PRIVATE_ID_FRAG_ACK     (WPS_ID_MESSAGE_ACK | 0x80)

/* WSC 2.0, WFA Vendor Extension Subelements */
#define WFA_VENDOR_EXT_ID                 "\x00\x37\x2A"
#define WPS_WFA_SUBID_VERSION2            0x00
#define WPS_WFA_SUBID_AUTHORIZED_MACS     0x01
#define WPS_WFA_SUBID_NW_KEY_SHAREABLE    0x02
#define WPS_WFA_SUBID_REQ_TO_ENROLL       0x03
#define WPS_WFA_SUBID_SETTINGS_DELAY_TIME 0x04

/* Device request/response type */
#define WPS_MSGTYPE_ENROLLEE_INFO_ONLY    0x00
#define WPS_MSGTYPE_ENROLLEE_OPEN_8021X   0x01
#define WPS_MSGTYPE_REGISTRAR             0x02
#define WPS_MSGTYPE_AP_WLAN_MGR           0x03

/* RF Band */
#define WPS_RFBAND_24GHZ    0x01
#define WPS_RFBAND_50GHZ    0x02

/* Simple Config state */
#define WPS_SCSTATE_UNCONFIGURED    0x01
#define WPS_SCSTATE_CONFIGURED      0x02


/* Device Type sub categories for primary and secondary device types */
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_PC         1
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_SERVER     2
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_MEDIA_CTR  3
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_UM_PC      4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_NOTEBOOK   5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_DESKTOP    6   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_MID        7   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_COMP_NETBOOK    8   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_Keyboard    1   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_MOUSE       2   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_JOYSTICK    3   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_TRACKBALL   4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_GAM_CTRL    5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_REMOTE      6   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_TOUCHSCREEN 7   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_BIO_READER  8   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_INP_BAR_READER  9   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PRTR_PRINTER    1
#define WPS_DEVICE_TYPE_SUB_CAT_PRTR_SCANNER    2
#define WPS_DEVICE_TYPE_SUB_CAT_PRTR_FAX        3   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PRTR_COPIER     4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PRTR_ALLINONE   5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_CAM_DGTL_STILL  1
#define WPS_DEVICE_TYPE_SUB_CAT_CAM_VIDEO_CAM   2   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_CAM_WEB_CAM     3   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_CAM_SECU_CAM    4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_STOR_NAS        1
#define WPS_DEVICE_TYPE_SUB_CAT_NW_AP           1
#define WPS_DEVICE_TYPE_SUB_CAT_NW_ROUTER       2
#define WPS_DEVICE_TYPE_SUB_CAT_NW_SWITCH       3
#define WPS_DEVICE_TYPE_SUB_CAT_NW_Gateway      4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_NW_BRIDGE       5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_DISP_TV         1
#define WPS_DEVICE_TYPE_SUB_CAT_DISP_PIC_FRAME  2
#define WPS_DEVICE_TYPE_SUB_CAT_DISP_PROJECTOR  3
#define WPS_DEVICE_TYPE_SUB_CAT_DISP_MONITOR    4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_MM_DAR          1
#define WPS_DEVICE_TYPE_SUB_CAT_MM_PVR          2
#define WPS_DEVICE_TYPE_SUB_CAT_MM_MCX          3
#define WPS_DEVICE_TYPE_SUB_CAT_MM_STB          4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_MM_MS_ME        5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_MM_PVP          6   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_GAM_XBOX        1
#define WPS_DEVICE_TYPE_SUB_CAT_GAM_XBOX_360    2
#define WPS_DEVICE_TYPE_SUB_CAT_GAM_PS          3
#define WPS_DEVICE_TYPE_SUB_CAT_GAM_GC          4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_GAM_PGD         5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PHONE_WM        1
#define WPS_DEVICE_TYPE_SUB_CAT_PHONE_PSM       2   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PHONE_PDM       3   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PHONE_SSM       4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_PHONE_SDM       5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_TUNER     1   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_SPEAKERS  2   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_PMP       3   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_HEADSET   4   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_HPHONE    5   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_MPHONE    6   /* WSC 2.0 */
#define WPS_DEVICE_TYPE_SUB_CAT_AUDIO_HTS       7   /* WSC 2.0 */

/* WSC 2.0, WFA Vendor Extension Subelements */
#define WPS_WFA_SUBID_VERSION2_S            1
#define WPS_WFA_SUBID_NW_KEY_SHAREABLE_S    1
#define WPS_WFA_SUBID_REQ_TO_ENROLL_S       1
#define WPS_WFA_SUBID_SETTINGS_DELAY_TIME_S 1

/* Association states */
#define WPS_ASSOC_NOT_ASSOCIATED  0
#define WPS_ASSOC_CONN_SUCCESS    1
#define WPS_ASSOC_CONFIG_FAIL     2
#define WPS_ASSOC_ASSOC_FAIL      3
#define WPS_ASSOC_IP_FAIL         4

/* Authentication types */
#define WPS_AUTHTYPE_OPEN        0x0001
#define WPS_AUTHTYPE_WPAPSK      0x0002 /* Deprecated in WSC 2.0 */
#define WPS_AUTHTYPE_SHARED      0x0004 /* Deprecated in WSC 2.0 */
#define WPS_AUTHTYPE_WPA         0x0008 /* Deprecated in WSC 2.0 */
#define WPS_AUTHTYPE_WPA2        0x0010
#define WPS_AUTHTYPE_WPA2PSK     0x0020

/* Config methods */
#define WPS_CONFMET_USBA            0x0001  /* Deprecated in WSC 2.0 */
#define WPS_CONFMET_ETHERNET        0x0002  /* Deprecated in WSC 2.0 */
#define WPS_CONFMET_LABEL           0x0004
#define WPS_CONFMET_DISPLAY         0x0008
#define WPS_CONFMET_EXT_NFC_TOK     0x0010
#define WPS_CONFMET_INT_NFC_TOK     0x0020
#define WPS_CONFMET_NFC_INTF        0x0040
#define WPS_CONFMET_PBC             0x0080
#define WPS_CONFMET_KEYPAD          0x0100
/* WSC 2.0 */
#define WPS_CONFMET_VIRT_PBC        0x0280
#define WPS_CONFMET_PHY_PBC         0x0480
#define WPS_CONFMET_VIRT_DISPLAY    0x2008
#define WPS_CONFMET_PHY_DISPLAY     0x4008

/* Connection types */
#define WPS_CONNTYPE_ESS    0x01
#define WPS_CONNTYPE_IBSS   0x02

/* WSC 2.0, deprecated and set to hardcode 0x10 for backwords compatibility resons */
#define WPS_VERSION                0x10 /* Do not change it anymore */

/* WSC 2.0 */
#define WPS_VERSION2               0x20

/* Size constants */
#define SIZE_SSID_LENGTH       33
#define WPS_UUID_LENGTH        16

#define SIZE_64_BITS        8
#define SIZE_128_BITS       16
#define SIZE_160_BITS       20
#define SIZE_256_BITS       32
#define SIZE_1536_BITS      192

#define SIZE_ETHERNET_ADDRESS    6

#define PRF_DIGEST_SIZE         SIZE_256_BITS

#define WPS_PADDED_AES_ROUND_UP(x,y) ((x) + (y)-((x)%(y)))

/* OUI constants */
#define WIFI_ALLIANCE_OUI       0x0050F204

/* String constants */
#define REGISTRAR_ID_STRING         "WFA-SimpleConfig-Registrar-1-0"
#define ENROLLEE_ID_STRING          "WFA-SimpleConfig-Enrollee-1-0"
#define KDK_PERSONALIZATION_STRING  (char*) "Wi-Fi Easy and Secure Key Derivation"

/* Timeout and delay constants */
#define WPS_EAPOL_PACKET_TIMEOUT       (5000) /* 5 second timeout when waiting for WPS EAPOL packets, except M2 */
#define WPS_PUBLIC_KEY_MESSAGE_TIMEOUT (8000) /* 8 second timeout for the enrollee when it has sent M1 and is waiting for the registrar's public key in M2 */
#define WPS_PBC_MODE_ASSERTION_DELAY   (5000) /* 5 second delay to account for enrollees that keep asserting PBC mode for some time after the handshake has completed */
#define WPS_PBC_NOTIFICATION_DELAY     (3000) /* 3 second delay between notifications sent to the application layer that an enrollee is trying to join using PBC mode */

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                    Variables
 ******************************************************/

extern const uint32_t wps_m2_timeout; /* Settable timeout for the enrollee when it is waiting for WPS message M2 from the registrar */

#ifdef __cplusplus
} /*extern "C" */
#endif
