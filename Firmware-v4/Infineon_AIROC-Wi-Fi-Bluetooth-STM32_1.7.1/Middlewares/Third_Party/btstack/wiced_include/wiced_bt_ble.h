/*
 * Copyright 2016-2023, Cypress Semiconductor Corporation or
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
 * WICED Bluetooth Low Energy (BLE) Functions
 *
 */
#pragma once

/**
 * @if DUAL_MODE
 * @addtogroup  btm_ble_api_functions        BLE (Bluetooth Low Energy)
 * @ingroup  wicedbt_DeviceManagement
 * This section describes the API's to use BLE functionality such as advertisement, scanning
 * BLE Connection, Data transfer, BLE Security etc.
 * @else
 * @addtogroup  wicedbt_DeviceManagement
 * @endif
 *
 *
 * @{
 */

#include "wiced_bt_cfg.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_isoc.h"

#define BTM_BLE_LEGACY_AD_DATA_LEN 31                       /**< Max legacy advertisement data len*/
#define BTM_AFH_CHNL_MAP_SIZE    HCI_AFH_CHANNEL_MAP_LEN    /**< AFH channel map size */
#define BLE_CHANNEL_MAP_LEN      5                          /**< AFH Channel Map len */
/** BLE Channel Map */
typedef uint8_t wiced_bt_ble_chnl_map_t[BLE_CHANNEL_MAP_LEN];


/** Scanner filter policy */
enum wiced_bt_ble_scanner_filter_policy_e {
    BTM_BLE_SCAN_POLICY_ACCEPT_ADV_RSP,              /**< accept adv packet from all, directed adv pkt not directed to local device is ignored */
    BTM_BLE_SCAN_POLICY_FILTER_ADV_RSP,        /**< accept adv packet from device in filter Accept List, directed adv packet not directed to local device is ignored */
    BTM_BLE_SCAN_POLICY_ACCEPT_RPA_DIR_ADV_RSP,      /**< accept adv packet from all, directed adv pkt not directed to local device is ignored except direct adv with RPA */
    BTM_BLE_SCAN_POLICY_FILTER_RPA_DIR_ADV_RSP,/**< accept adv packet from device in filter Accept List, directed adv pkt not directed to me is ignored except direct adv with RPA */
    BTM_BLE_SCAN_POLICY_MAX                       /**< Max Scan filter policy value */
};
/** BLE Scanner filter policy */
typedef uint8_t   wiced_bt_ble_scanner_filter_policy_t;  /**< Scanner filter policy (see #wiced_bt_ble_scanner_filter_policy_e) */


/** default advertising channel map */
#ifndef BTM_BLE_DEFAULT_ADVERT_CHNL_MAP
#define BTM_BLE_DEFAULT_ADVERT_CHNL_MAP   (BTM_BLE_ADVERT_CHNL_37| BTM_BLE_ADVERT_CHNL_38| BTM_BLE_ADVERT_CHNL_39)
#endif

/** Advertising filter policy */
enum wiced_bt_ble_advert_filter_policy_e {
    BTM_BLE_ADV_POLICY_ACCEPT_CONN_AND_SCAN     = 0x00,    /**< Process scan and connection requests from all devices (i.e., the Filter Accept List is not in use) (default) */
    BTM_BLE_ADV_POLICY_ACCEPT_CONN_FILTER_SCAN  = 0x01,    /**< Process connection requests from all devices and only scan requests from devices that are in the Filter Accept List. */
    BTM_BLE_ADV_POLICY_FILTER_CONN_ACCEPT_SCAN  = 0x02,    /**< Process scan requests from all devices and only connection requests from devices that are in the Filter Accept List */
    BTM_BLE_ADV_POLICY_FILTER_CONN_FILTER_SCAN  = 0x03,    /**< Process scan and connection requests only from devices in the Filter Accept List. */
    BTM_BLE_ADV_POLICY_MAX                                 /**< Max Adv filter value */
};
typedef uint8_t   wiced_bt_ble_advert_filter_policy_t;  /**< Advertising filter policy (see #wiced_bt_ble_advert_filter_policy_e) */

/** default advertising filter policy */
#define BTM_BLE_ADVERT_FILTER_DEFAULT   BTM_BLE_ADV_POLICY_ACCEPT_CONN_AND_SCAN

#define BTM_BLE_ADVERT_INTERVAL_MIN     0x0020  /**< adv parameter Min value */
#define BTM_BLE_ADVERT_INTERVAL_MAX     0x4000  /**< adv parameter Max value */

#define BTM_BLE_SCAN_INTERVAL_MIN       0x0004  /**< Scan interval minimum value */
#define BTM_BLE_SCAN_INTERVAL_MAX       0x4000  /**< Scan interval miximum value */
#define BTM_BLE_SCAN_WINDOW_MIN         0x0004  /**< Scan window minimum value */
#define BTM_BLE_SCAN_WINDOW_MAX         0x4000  /**< Scan window maximum value */
#define BTM_BLE_CONN_INTERVAL_MIN       0x0006  /**< Connection interval minimum value */
#define BTM_BLE_CONN_INTERVAL_MAX       0x0C80  /**< Connection interval maximum value */
#define BTM_BLE_CONN_LATENCY_MAX        500     /**< Maximum Connection Latency */
#define BTM_BLE_CONN_SUP_TOUT_MIN       0x000A  /**< Minimum Supervision Timeout */
#define BTM_BLE_CONN_SUP_TOUT_MAX       0x0C80  /**< Maximum Supervision Timeout */
#define BTM_BLE_CONN_PARAM_UNDEF        0xffff  /**< use this value when a specific value not to be overwritten */
#define BTM_BLE_CONN_SUP_TOUT_DEF       700     /**< Default Supervision Timeout */

/* default connection parameters if not configured, use GAP recommend value for auto connection */
/** default scan interval
 *  30 ~ 60 ms (use 60)  = 96 *0.625
 */
#define BTM_BLE_SCAN_FAST_INTERVAL      96

/** default scan window (in .625ms slots) for background auto connections
 * 30 ms = 48 *0.625
 */
#define BTM_BLE_SCAN_FAST_WINDOW        48

/** default scan interval used in reduced power cycle (background scanning)
 *  1.28 s   = 2048 *0.625
 */
#define BTM_BLE_SCAN_SLOW_INTERVAL_1    2048

/** default scan window used in reduced power cycle (background scanning)
 *   11.25 ms = 18 *0.625
 */
#define BTM_BLE_SCAN_SLOW_WINDOW_1      18

/** default scan interval used in reduced power cycle (background scanning)
 *  2.56 s   = 4096 *0.625
 */
#define BTM_BLE_SCAN_SLOW_INTERVAL_2    4096

/** default scan window used in reduced power cycle (background scanning)
 *  22.5 ms = 36 *0.625
 */
#define BTM_BLE_SCAN_SLOW_WINDOW_2      36

/** default connection interval min
 *  recommended min: 30ms  = 24 * 1.25
 */
#define BTM_BLE_CONN_INTERVAL_MIN_DEF   24

/** default connection interval max
 * recommended max: 50 ms = 56 * 1.25
 */
#define BTM_BLE_CONN_INTERVAL_MAX_DEF   40

/** default Peripheral latency */
#define BTM_BLE_CONN_PERIPHERAL_LATENCY_DEF  0

/** default supervision timeout */
#define BTM_BLE_CONN_TIMEOUT_DEF                    2000

/** BLE Signature
 *  BLE data signature length 8 Bytes + 4 bytes counter
 */
#define BTM_BLE_AUTH_SIGNATURE_SIZE                 12
typedef uint8_t wiced_dev_ble_signature_t[BTM_BLE_AUTH_SIGNATURE_SIZE];     /**< Device address (see #BTM_BLE_AUTH_SIGNATURE_SIZE) */

#define BTM_BLE_POLICY_REJECT_ALL                   0x00    /**< relevant to both */
#define BTM_BLE_POLICY_ALLOW_SCAN                   0x01    /**< relevant to advertiser */
#define BTM_BLE_POLICY_ALLOW_CONN                   0x02    /**< relevant to advertiser */
#define BTM_BLE_POLICY_ALLOW_ALL                    0x03    /**< relevant to both */

/* ADV data flag bit definition used for BTM_BLE_ADVERT_TYPE_FLAG */
#define BTM_BLE_LIMITED_DISCOVERABLE_FLAG           (0x01 << 0)     /**< Limited Discoverable */
#define BTM_BLE_GENERAL_DISCOVERABLE_FLAG           (0x01 << 1)     /**< General Discoverable */
#define BTM_BLE_BREDR_NOT_SUPPORTED                 (0x01 << 2)     /**< BR/EDR Not Supported */
/* 4.1 spec adv flag for simultaneous BR/EDR+LE connection support (see) */
#define BTM_BLE_SIMULTANEOUS_DUAL_MODE_TO_SAME_DEVICE_CONTROLLER_SUPPORTED      (0x01 << 3)   /**< Simultaneous LE and BR/EDR to Same Device Capable (Controller). */
#define BTM_BLE_SIMULTANEOUS_DUAL_MODE_TO_SAME_DEVICE_HOST_SUPPORTED            (0x01 << 4)   /**< Simultaneous LE and BR/EDR to Same Device Capable (Host). */
#define BTM_BLE_NON_LIMITED_DISCOVERABLE_FLAG       (0x00 )         /**< Non Discoverable */
#define BTM_BLE_ADVERT_FLAG_MASK                    (BTM_BLE_LIMITED_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED | BTM_BLE_GENERAL_DISCOVERABLE_FLAG) /**< BLE adverisement mask */
#define BTM_BLE_LIMITED_DISCOVERABLE_MASK           (BTM_BLE_LIMITED_DISCOVERABLE_FLAG )    /**< BLE Limited discovery mask*/


/** Advertisement data types */
enum wiced_bt_ble_advert_type_e {
    BTM_BLE_ADVERT_TYPE_FLAG                        = 0x01,                 /**< Advertisement flags */
    BTM_BLE_ADVERT_TYPE_16SRV_PARTIAL               = 0x02,                 /**< List of supported services - 16 bit UUIDs (partial) */
    BTM_BLE_ADVERT_TYPE_16SRV_COMPLETE              = 0x03,                 /**< List of supported services - 16 bit UUIDs (complete) */
    BTM_BLE_ADVERT_TYPE_32SRV_PARTIAL               = 0x04,                 /**< List of supported services - 32 bit UUIDs (partial) */
    BTM_BLE_ADVERT_TYPE_32SRV_COMPLETE              = 0x05,                 /**< List of supported services - 32 bit UUIDs (complete) */
    BTM_BLE_ADVERT_TYPE_128SRV_PARTIAL              = 0x06,                 /**< List of supported services - 128 bit UUIDs (partial) */
    BTM_BLE_ADVERT_TYPE_128SRV_COMPLETE             = 0x07,                 /**< List of supported services - 128 bit UUIDs (complete) */
    BTM_BLE_ADVERT_TYPE_NAME_SHORT                  = 0x08,                 /**< Short name */
    BTM_BLE_ADVERT_TYPE_NAME_COMPLETE               = 0x09,                 /**< Complete name */
    BTM_BLE_ADVERT_TYPE_TX_POWER                    = 0x0A,                 /**< TX Power level  */
    BTM_BLE_ADVERT_TYPE_DEV_CLASS                   = 0x0D,                 /**< Device Class */
    BTM_BLE_ADVERT_TYPE_SIMPLE_PAIRING_HASH_C       = 0x0E,                 /**< Simple Pairing Hash C */
    BTM_BLE_ADVERT_TYPE_SIMPLE_PAIRING_RAND_C       = 0x0F,                 /**< Simple Pairing Randomizer R */
    BTM_BLE_ADVERT_TYPE_SM_TK                       = 0x10,                 /**< Security manager TK value */
    BTM_BLE_ADVERT_TYPE_SM_OOB_FLAG                 = 0x11,                 /**< Security manager Out-of-Band data */
    BTM_BLE_ADVERT_TYPE_INTERVAL_RANGE              = 0x12,                 /**< Peripheral connection interval range */
    BTM_BLE_ADVERT_TYPE_SOLICITATION_SRV_UUID       = 0x14,                 /**< List of solicitated services - 16 bit UUIDs */
    BTM_BLE_ADVERT_TYPE_128SOLICITATION_SRV_UUID    = 0x15,                 /**< List of solicitated services - 128 bit UUIDs */
    BTM_BLE_ADVERT_TYPE_SERVICE_DATA                = 0x16,                 /**< Service data - 16 bit UUID */
    BTM_BLE_ADVERT_TYPE_PUBLIC_TARGET               = 0x17,                 /**< Public target address */
    BTM_BLE_ADVERT_TYPE_RANDOM_TARGET               = 0x18,                 /**< Random target address */
    BTM_BLE_ADVERT_TYPE_APPEARANCE                  = 0x19,                 /**< Appearance */
    BTM_BLE_ADVERT_TYPE_ADVERT_INTERVAL             = 0x1a,                 /**< Advertising interval */
    BTM_BLE_ADVERT_TYPE_LE_BD_ADDR                  = 0x1b,                 /**< LE device bluetooth address */
    BTM_BLE_ADVERT_TYPE_LE_ROLE                     = 0x1c,                 /**< LE role */
    BTM_BLE_ADVERT_TYPE_256SIMPLE_PAIRING_HASH      = 0x1d,                 /**< Simple Pairing Hash C-256 */
    BTM_BLE_ADVERT_TYPE_256SIMPLE_PAIRING_RAND      = 0x1e,                 /**< Simple Pairing Randomizer R-256 */
    BTM_BLE_ADVERT_TYPE_32SOLICITATION_SRV_UUID     = 0x1f,                 /**< List of solicitated services - 32 bit UUIDs */
    BTM_BLE_ADVERT_TYPE_32SERVICE_DATA              = 0x20,                 /**< Service data - 32 bit UUID */
    BTM_BLE_ADVERT_TYPE_128SERVICE_DATA             = 0x21,                 /**< Service data - 128 bit UUID */
    BTM_BLE_ADVERT_TYPE_CONN_CONFIRM_VAL            = 0x22,                 /**< LE Secure Connections Confirmation Value */
    BTM_BLE_ADVERT_TYPE_CONN_RAND_VAL               = 0x23,                 /**< LE Secure Connections Random Value */
    BTM_BLE_ADVERT_TYPE_URI                         = 0x24,                 /**< URI */
    BTM_BLE_ADVERT_TYPE_INDOOR_POS                  = 0x25,                 /**< Indoor Positioning */
    BTM_BLE_ADVERT_TYPE_TRANS_DISCOVER_DATA         = 0x26,                 /**< Transport Discovery Data */
    BTM_BLE_ADVERT_TYPE_SUPPORTED_FEATURES          = 0x27,                 /**< LE Supported Features */
    BTM_BLE_ADVERT_TYPE_UPDATE_CH_MAP_IND           = 0x28,                 /**< Channel Map Update Indication */
    BTM_BLE_ADVERT_TYPE_PB_ADV                      = 0x29,                 /**< PB-ADV */
    BTM_BLE_ADVERT_TYPE_MESH_MSG                    = 0x2A,                 /**< Mesh Message */
    BTM_BLE_ADVERT_TYPE_MESH_BEACON                 = 0x2B,                 /**< Mesh Beacon */
    BTM_BLE_ADVERT_TYPE_PSRI                        = 0x2E,                 /**< Generic Audio Provate Set Random Identifier */
    BTM_BLE_ADVERT_TYPE_3D_INFO_DATA                = 0x3D,                 /**< 3D Information Data */
    BTM_BLE_ADVERT_TYPE_MANUFACTURER                = 0xFF                  /**< Manufacturer data */
};
typedef uint8_t   wiced_bt_ble_advert_type_t;    /**< BLE advertisement data type (see #wiced_bt_ble_advert_type_e) */

/** security settings used with L2CAP LE COC */
enum wiced_bt_ble_sec_flags_e
{
    BTM_SEC_LE_LINK_ENCRYPTED                       = 0x01,                 /**< Link encrypted */
    BTM_SEC_LE_LINK_PAIRED_WITHOUT_MITM             = 0x02,                 /**< Paired without man-in-the-middle protection */
    BTM_SEC_LE_LINK_PAIRED_WITH_MITM                = 0x04                  /**< Link with man-in-the-middle protection */
};

/** Advertisement element */
typedef struct
{
    uint8_t                     *p_data;        /**< Advertisement data */
    uint16_t                    len;            /**< Advertisement length */
    wiced_bt_ble_advert_type_t  advert_type;    /**< Advertisement data type */
}wiced_bt_ble_advert_elem_t;

/** Scan result event type */
enum wiced_bt_dev_ble_evt_type_e {
    BTM_BLE_EVT_CONNECTABLE_ADVERTISEMENT           = 0x00,                 /**< Connectable advertisement */
    BTM_BLE_EVT_CONNECTABLE_DIRECTED_ADVERTISEMENT  = 0x01,                 /**< Connectable Directed advertisement */
    BTM_BLE_EVT_SCANNABLE_ADVERTISEMENT             = 0x02,                 /**< Scannable advertisement */
    BTM_BLE_EVT_NON_CONNECTABLE_ADVERTISEMENT       = 0x03,                 /**< Non connectable advertisement */
    BTM_BLE_EVT_SCAN_RSP                            = 0x04                  /**< Scan response */
};
typedef uint8_t wiced_bt_dev_ble_evt_type_t;    /**< Scan result event value (see #wiced_bt_dev_ble_evt_type_e) */

/** Background connection type */
enum wiced_bt_ble_conn_type_e
{
    BTM_BLE_CONN_NONE,                          /**< No background connection */
    BTM_BLE_CONN_AUTO,                          /**< Auto connection based on filter list */
    BTM_BLE_CONN_SELECTIVE = BTM_BLE_CONN_AUTO  /**< Selective not used */
};
typedef uint8_t wiced_bt_ble_conn_type_t;       /**< Connection type (see #wiced_bt_ble_conn_type_e) */

/** LE inquiry result type */
typedef struct
{
    wiced_bool_t                    is_extended;                            /**< True if an extended ADV */
    wiced_bt_device_address_t       remote_bd_addr;                         /**< Device address */
    uint8_t                         ble_addr_type;                          /**< LE Address type */
    wiced_bt_dev_ble_evt_type_t     ble_evt_type;                           /**< Scan result event type */
    int8_t                          rssi;                                   /**< Set to #BTM_INQ_RES_IGNORE_RSSI, if not valid */
    uint8_t                         flag;                                   /**< Adverisement Flag value */
    uint8_t                         primary_phy;                            /**< Primary PHY */
    uint8_t                         secondary_phy;                          /**< Secondary PHY */
    uint8_t                         adv_sid;                                /**< advertisement security ID */
    uint8_t                         tx_power;                               /**< Tx power */
    uint16_t                        periodic_adv_interval;                  /**< Periodic advertisement interval */
    uint8_t                         direct_addr_type;                       /**< Directed address type */
    wiced_bt_device_address_t       direct_bda;                             /**< Directed address */
} wiced_bt_ble_scan_results_t;

/** LE encryption method **/
enum wiced_bt_ble_sec_action_type_e
{
    BTM_BLE_SEC_NONE,               /**< No encryption */
    BTM_BLE_SEC_ENCRYPT,            /**< encrypt the link using current key */
    BTM_BLE_SEC_ENCRYPT_NO_MITM,    /**< encryption without MITM */
    BTM_BLE_SEC_ENCRYPT_MITM        /**< encryption with MITM*/
};
typedef uint8_t wiced_bt_ble_sec_action_type_t;  /**< BLE security type. refer #wiced_bt_ble_sec_action_type_e */

#define BTM_BLE_PREFER_1M_PHY              0x01    /**< LE 1M PHY preference */
#define BTM_BLE_PREFER_2M_PHY              0x02    /**< LE 2M PHY preference */
#define BTM_BLE_PREFER_LELR_PHY            0x04    /**< LE LELR PHY preference */

/**  Host preferences on PHY.
 *  bit field that indicates the transmitter PHYs that
 *  the Host prefers the Controller to use.Bit number 3 -7 reserved for future.
 */
typedef uint8_t   wiced_bt_ble_host_phy_preferences_t;

#define BTM_BLE_PREFER_NO_LELR    0x0000 /**< No preferred coding */
#define BTM_BLE_PREFER_LELR_S2    0x0001 /**< Preferred coding is S=2, 500 kb/s */
#define BTM_BLE_PREFER_LELR_S8    0x0002 /**< Preferred coding is S=8, 125 kb/s */

/**
 * @note BTM_BLE_PREFER_LELR_512K and BTM_BLE_PREFER_LELR_125K are going to be deprecated.
 * Use BTM_BLE_PREFER_LELR_S2 and BTM_BLE_PREFER_LELR_S8 instead.
 */
#define BTM_BLE_PREFER_LELR_512K    BTM_BLE_PREFER_LELR_S2 /**< @note To be deprecated use \ref BTM_BLE_PREFER_LELR_S2 */
#define BTM_BLE_PREFER_LELR_125K    BTM_BLE_PREFER_LELR_S8 /**< @note To be deprecated use \ref BTM_BLE_PREFER_LELR_S8 */

/**  The PHY_options parameter is a bit field that allows the Host to specify options
 *    for LE long range PHY. Default connection is with no LE coded PHY.The Controller may override any
 *    preferred coding (S2 coded phy for 512k speed and s8 coded phy for 128K) for
 *    transmitting on the LE Coded PHY.
 *    The Host may specify a preferred coding even if it prefers not to use the LE
 *    Coded transmitter PHY since the Controller may override the PHY preference.
 *    Bit 2-15 reserved for future use.
 *  @note  These preferences applicable only when BTM_BLE_PREFER_LELR_PHY flag is set
 */
typedef uint16_t  wiced_bt_ble_lelr_phy_preferences_t;

/** Host PHY preferences */
typedef struct
{
    wiced_bt_device_address_t               remote_bd_addr;     /**< Peer Device address */
    wiced_bt_ble_host_phy_preferences_t     tx_phys;            /**< Host preference among the TX PHYs */
    wiced_bt_ble_host_phy_preferences_t     rx_phys;            /**< Host preference among the RX PHYs */
    wiced_bt_ble_lelr_phy_preferences_t     phy_opts;           /**< Host preference on LE coded PHY */
}wiced_bt_ble_phy_preferences_t;

/** BLE connection parameteres */
typedef struct
{
    wiced_bt_dev_role_t role;           /**< Connection role 0: Central  1: Peripheral */
    uint16_t            conn_interval;          /**< Connection interval in slots */
    uint16_t            conn_latency;           /**< Connection latency */
    uint16_t            supervision_timeout;    /**< Supervision timeout */
}wiced_bt_ble_conn_params_t;

/** BLE preferred connection parameters */
typedef struct
{
    uint16_t  conn_interval_min;  /**< minimum connection interval */
    uint16_t  conn_interval_max;  /**< maximum connection interval */
    uint16_t  conn_latency;  /**< connection latency */
    uint16_t  conn_supervision_timeout;  /**< connection supervision timeout */
}wiced_bt_ble_pref_conn_params_t;


/* The power table for multi ADV Tx Power levels
    Min   : -12 dBm     #define BTM_BLE_ADV_TX_POWER_MIN        0
    Low   :  -8 dBm     #define BTM_BLE_ADV_TX_POWER_LOW        1
    Mid   :  -4 dBm     #define BTM_BLE_ADV_TX_POWER_MID        2
    Upper :   0 dBm     #define BTM_BLE_ADV_TX_POWER_UPPER      3
    Max   :   4 dBm     #define BTM_BLE_ADV_TX_POWER_MAX        4
*/
#define MULTI_ADV_TX_POWER_MIN_INDEX                0   /**< Multi adv tx min power index */
#define MULTI_ADV_TX_POWER_MAX_INDEX                4   /**< Multi adv tx max power index */

/** Transmit Power in dBm ( #MULTI_ADV_TX_POWER_MIN_INDEX to #MULTI_ADV_TX_POWER_MAX_INDEX ) */
typedef int8_t wiced_bt_ble_adv_tx_power_t;

/** Multi-advertisement start/stop */
enum wiced_bt_ble_multi_advert_start_e
{
    MULTI_ADVERT_STOP                               = 0x00,                 /**< Stop Multi-adverstisment */
    MULTI_ADVERT_START                              = 0x01                  /**< Start Multi-adverstisment */
};

/** Multi-advertisement type */
enum wiced_bt_ble_multi_advert_type_e
{
    MULTI_ADVERT_CONNECTABLE_UNDIRECT_EVENT         = 0x00,     /**< Multi adv Connectable undirected event */
    MULTI_ADVERT_CONNECTABLE_DIRECT_EVENT           = 0x01,     /**< Multi adv Connectable directed event */
    MULTI_ADVERT_DISCOVERABLE_EVENT                 = 0x02,     /**< Multi adv Discoverable event */
    MULTI_ADVERT_NONCONNECTABLE_EVENT               = 0x03,     /**< Multi adv NonConnectable event */
    MULTI_ADVERT_LOW_DUTY_CYCLE_DIRECT_EVENT        = 0x04      /**< Multi adv Low Cycle directed event */
};
typedef uint8_t wiced_bt_ble_multi_advert_type_t;    /**< BLE advertisement type (see #wiced_bt_ble_multi_advert_type_e) */


/** LE Multi advertising parameter */
typedef struct
{
    /**< BTM_BLE_ADVERT_INTERVAL_MIN to BTM_BLE_ADVERT_INTERVAL_MAX ( As per spec ) */
    uint16_t                             adv_int_min;            /**< Minimum adv interval */
    /**< BTM_BLE_ADVERT_INTERVAL_MIN to BTM_BLE_ADVERT_INTERVAL_MAX ( As per spec ) */
    uint16_t                             adv_int_max;            /**< Maximum adv interval */
    wiced_bt_ble_multi_advert_type_t     adv_type;               /**< Adv event type */
    wiced_bt_ble_advert_chnl_map_t       channel_map;            /**< Adv channel map */
    wiced_bt_ble_advert_filter_policy_t  adv_filter_policy;      /**< Advertising filter policy */
    wiced_bt_ble_adv_tx_power_t          adv_tx_power;           /**< Adv tx power */
    wiced_bt_device_address_t            peer_bd_addr;           /**< Peer Device address */
    wiced_bt_ble_address_type_t          peer_addr_type;         /**< Peer LE Address type */
    wiced_bt_device_address_t            own_bd_addr;            /**< Own LE address */
    wiced_bt_ble_address_type_t          own_addr_type;          /**< Own LE Address type */
}wiced_bt_ble_multi_adv_params_t;

/** Privacy mode
 * refer Spec version 5.0 Vol 3 Part C Section 10.7 privacy feature
 */
enum wiced_bt_ble_privacy_e
{
    BTM_BLE_PRIVACY_MODE_NETWORK,                           /**< network privacy mode*/
    BTM_BLE_PRIVACY_MODE_DEVICE                             /**< device privacy mode*/
};
/** BLE Privacy mode. Refer #wiced_bt_ble_privacy_e */
typedef uint8_t wiced_bt_ble_privacy_mode_t;

/** Multi-advertisement Filtering policy  */
enum wiced_bt_ble_multi_advert_filtering_policy_e
{
    MULTI_ADVERT_FILTER_POLICY_NOT_USED                         = 0x00,   /**< Multi adv filter filter Accept List not used */
    MULTI_ADVERT_FILTER_POLICY_ADV_ALLOW_UNKNOWN_CONNECTION     = 0x01,   /**< Multi adv filter filter Accept List for scan request */
    MULTI_ADVERT_FILTER_POLICY_ADV_ALLOW_UNKNOWN_SCANNING       = 0x02,   /**< Multi adv filter filter Accept List for connection request */
    MULTI_ADVERT_FILTER_POLICY_USE_FOR_ALL                      = 0x03    /**< Multi adv filter filter Accept List for all */
};
typedef uint8_t wiced_bt_ble_multi_advert_filtering_policy_t;    /**< BLE advertisement filtering policy (see #wiced_bt_ble_multi_advert_filtering_policy_e) */

/**
 * Callback wiced_bt_ble_scan_result_cback_t
 *
 * Scan result callback (from calling #wiced_bt_ble_scan)
 *
 * @param p_scan_result             : scan result data (NULL indicates end of scanning)
 * @param p_adv_data                : Advertisement data (parse using #wiced_bt_ble_check_advertising_data)
 *
 * @return Nothing
 */
typedef void (wiced_bt_ble_scan_result_cback_t) (wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data);

/**
 * Callback wiced_bt_ble_read_phy_complete_callback_t
 *
 * read phy complete callback (from calling #wiced_bt_ble_read_phy)
 *
 * @param p_phy_result             : read phys result
 *
 * @return Nothing
 */
typedef void (wiced_bt_ble_read_phy_complete_callback_t) (wiced_bt_ble_phy_update_t *p_phy_result);

/** ADV extension structures */
#define WICED_BT_BLE_MAX_EXT_ADV_DATA_LEN    251

/** BLE PHY */
enum
{
    WICED_BT_BLE_EXT_ADV_PHY_1M          = 0x1, /**< advetiser advertisement PHY is LE 1M */
    WICED_BT_BLE_EXT_ADV_PHY_2M          = 0x2, /**< advetiser advertisement PHY is LE 2M */
    WICED_BT_BLE_EXT_ADV_PHY_LE_CODED    = 0x3, /**< advetiser advertisement PHY is LE Coded (for long range) */
    WICED_BT_BLE_EXT_ADV_NUM_PHYS        = 0x3  /**< 3 PHYs are defined */
};
typedef uint8_t wiced_bt_ble_ext_adv_phy_t;     /**< BLE phy to be used for extended advertisement */

/** BLE PHY bit mask */
enum
{
    WICED_BT_BLE_EXT_ADV_PHY_1M_BIT         = (1 << 0), /**< Bit mask to specify for LE1M PHY */
    WICED_BT_BLE_EXT_ADV_PHY_2M_BIT         = (1 << 1), /**< Bit mask to specify for LE2M PHY */
    WICED_BT_BLE_EXT_ADV_PHY_LE_CODED_BIT   = (1 << 2), /**< Bit mask to specify for LE coded PHY */
};
typedef uint8_t wiced_bt_ble_ext_adv_phy_mask_t;  /**< BLE phy mask to be used for extended advertisement */

/** Advertising event properties: Describes the type of advertising event that is being configured and its basic properties */
enum
{
    WICED_BT_BLE_EXT_ADV_EVENT_CONNECTABLE_ADV                       = (1 << 0),   /**< Connectable ADV */
    WICED_BT_BLE_EXT_ADV_EVENT_SCANNABLE_ADV                         = (1 << 1),   /**< Scannable ADV */
    WICED_BT_BLE_EXT_ADV_EVENT_DIRECTED_ADV                          = (1 << 2),   /**< Low duty cycle directed advertisement */
    WICED_BT_BLE_EXT_ADV_EVENT_HIGH_DUTY_DIRECTED_CONNECTABLE_ADV    = (1 << 3),   /**< 3.75 ms Advertising Interval, only valid in legacy ADV */
    WICED_BT_BLE_EXT_ADV_EVENT_LEGACY_ADV                            = (1 << 4),   /**< Legacy Advertisement. Adv data cannot be more than 31 bytes.*/
    WICED_BT_BLE_EXT_ADV_EVENT_ANONYMOUS_ADV                         = (1 << 5),   /**< Omits advertisers address from all PDUs */
    WICED_BT_BLE_EXT_ADV_EVENT_INCLUDE_TX_POWER                      = (1 << 6),   /**< Include Tx power in ext ADV pdus */

    /** Other bits RFU */
};
typedef uint16_t wiced_bt_ble_ext_adv_event_property_t;  /**< BLE extended advertisement event property */

/** Advertisement set handle to identify adv set b/n host and controller */
enum
{
    WICED_BT_BLE_EXT_ADV_HANDLE_MIN = 0x00,     /**< min advertisement set handle value */
    WICED_BT_BLE_EXT_ADV_HANDLE_MAX = 0xef,     /**< max advertisement set handle value */
};

typedef uint8_t wiced_bt_ble_ext_adv_handle_t; /**< advertisement set handle value */

/** The Advertising set identifier(SID) is used to uniquely identify adv sets from advertiser.
    SID the value to be transmitted in the advertising SID subfield of the ADI field of the Extended ADV PDUs */
enum
{
    WICED_BT_BLE_EXT_ADV_SID_MIN = 0x00,        /**< min SID value */
    WICED_BT_BLE_EXT_ADV_SID_MAX = 0x0f,        /**< max SID value */
};
typedef uint8_t wiced_bt_ble_ext_adv_sid_t; /**< SID value */

/** Value to configure to receive scan request recived notification */
enum wiced_bt_ble_ext_adv_scan_req_notification_setting_e
{
    WICED_BT_BLE_EXT_ADV_SCAN_REQ_NOTIFY_DISABLE = 0x00,    /**< Do not send Notification on scan request */
    WICED_BT_BLE_EXT_ADV_SCAN_REQ_NOTIFY_ENABLE  = 0x01,    /**< Send Notification on scan request */
};
/** Enable or disable notification value (see #wiced_bt_ble_ext_adv_scan_req_notification_setting_e) */
typedef uint8_t wiced_bt_ble_ext_adv_scan_req_notification_setting_t;

/** Advertisement duration configuration for specified adv handle */
typedef struct
{
    wiced_bt_ble_ext_adv_handle_t   adv_handle;     /**< advertisement set handle */

    uint16_t adv_duration; /**< 0 = No advertising duration. Advertising to continue until the Host disables it.
                                                    0xXXXX = Range: 0x0001 - 0xFFFF (10 ms to 655,350 ms) */

    uint8_t max_ext_adv_events; /**< 0xXX: Maximum number of extended advertising events the Controller shall
                                            attempt to send prior to disabling the extended advertising set even if
                                            the adv_duration parameter has not expired.
                                            0x00: No maximum number of advertising events */

} wiced_bt_ble_ext_adv_duration_config_t;


/** Periodic adv property */
enum wiced_bt_ble_periodic_adv_prop_e
{
    WICED_BT_BLE_PERIODIC_ADV_PROPERTY_INCLUDE_TX_POWER      = (1 << 6) /**< Speicify Tx power in periodic adv events */
};
typedef uint16_t wiced_bt_ble_periodic_adv_prop_t;/**< Periodic adv property (see #wiced_bt_ble_periodic_adv_prop_e) */

/** Extended scan duplicate filter policy */
enum wiced_bt_ble_ext_scan_filter_duplicate_e
{
    WICED_BT_BLE_EXT_SCAN_FILTER_DUPLICATE_DISABLE,                     /**< send all advertisements received from advertisers*/
    WICED_BT_BLE_EXT_SCAN_FILTER_DUPLICATE_ENABLE,                      /**< duplicate advertisements should not be sent until scan disabled */
    WICED_BT_BLE_EXT_SCAN_FILTER_DUPLICATE_ENABLE_RESET_ON_SCAN_PERIOD, /**< filter duplicate adv during single scan Duration
                                                                        (see wiced_bt_ble_ext_scan_enable_t).Period should be non zero on using this option */
};

typedef uint8_t wiced_bt_ble_ext_scan_filter_duplicate_t; /**< Extended scan duplicate filter policy (see #wiced_bt_ble_ext_scan_filter_duplicate_e) */

/** Filter policy used in extended create connection command */
enum wiced_bt_ble_ext_filter_policy_e
{
    /* Initiator filter policy enums */
    WICED_BT_BLE_IGNORE_FILTER_ACCEPT_LIST_FOR_CONNS               = 0,           /**< Filter Accept List is not used to determine which advertiser to connect to.
                                                                Peer_Address_Type and Peer_Address shall be used */
    WICED_BT_BLE_USE_FILTER_ACCEPT_LIST_FOR_CONNS   = 1,           /**< Filter Accept List is used to determine which advertiser to connect to.
                                                                Peer_Address_Type and Peer_Address shall be ignored. */
};
typedef uint8_t wiced_bt_ble_ext_filter_policy_t;/**< Filter policy used. (see #wiced_bt_ble_ext_filter_policy_e) */

/** Options used in create periodic sync to periodic adv command */
enum wiced_bt_ble_adv_sync_options_e
{
    WICED_BT_BLE_IGNORE_SYNC_TO_PERIODIC_ADV_LIST =
        0, /**< Use the Advertising_SID, Advertising_Address_Type, and Advertising
                                                                Address parameters specified in create sync command to determine
                                                                which advertiser to listen to */

    WICED_BT_BLE_SYNC_TO_PERIODIC_ADV_LIST = 1, /**< Use the Periodic Advertiser List to determine which
                                                                advertiser to listen to.*/
};
/** Options used in create periodic sync to periodic adv command (see #wiced_bt_ble_adv_sync_options_e)*/
typedef uint8_t wiced_bt_ble_adv_sync_options_t;

/** Mode used in Periodic Advertising Sync Transfer Parameters */
enum wiced_bt_ble_periodic_adv_sync_transfer_mode_e
{
    WICED_BT_BLE_IGNORE_PA_SYNC_TRANSFER_EVT, /**< No attempt is made to synchronize to the periodic advertising and no
                                                                     HCI_LE_Periodic_Advertising_Sync_Transfer_Received event is sent to the Host (default). */
    WICED_BT_BLE_ENABLE_PA_SYNC_TRANSFER_DISABLE_PA_REPORT_EVT, /**< An HCI_LE_Periodic_Advertising_Sync_Transfer_Received event is
                                                                      sent to the Host. HCI_LE_Periodic_Advertising_Report events will be disabled. */
    WICED_BT_BLE_ENABLE_PA_SYNC_TRANSFER_ENABLE_PA_REPORT_EVT, /**< An HCI_LE_Periodic_Advertising_Sync_Transfer_Received event is
                                                                     sent to the Host. HCI_LE_Periodic_Advertising_Report events will be enabled. */
};
/** Mode used in create periodic sync to periodic adv command (see #wiced_bt_ble_periodic_adv_sync_transfer_mode_e)*/
typedef uint8_t wiced_bt_ble_periodic_adv_sync_transfer_mode_t;

enum
{
    /** The Host has no preferred or required coding when transmitting on the LE Coded PHY  */
    WICED_BT_BLE_PHY_ADV_OPTIONS_NO_PREFERENCE = 0,
    /** The Host prefers that S=2 coding be used when transmitting on the LE Coded PHY */
    WICED_BT_BLE_PHY_ADV_OPTIONS_PREFER_S2 = 1,
    /** The Host prefers that S=8 coding be used when transmitting on the LE Coded PHY */
    WICED_BT_BLE_PHY_ADV_OPTIONS_PREFER_S8 = 2,
    /** The Host requires that S=2 coding be used when transmitting on the LE Coded PHY */
    WICED_BT_BLE_PHY_ADV_OPTIONS_REQUIRE_S2 = 3,
    /** The Host requires that S=8 coding be used when transmitting on the LE Coded PHY */
    WICED_BT_BLE_PHY_ADV_OPTIONS_REQUIRE_S8 = 4,
};
/** Phy adv options to be set in \ref wiced_bt_ble_set_ext_adv_params_v2 */
typedef uint8_t wiced_bt_ble_phy_adv_options_t;

/** Parameters for extended adv */
typedef struct
{
    /** event_properties Bit mask to specify connectable, scannable, low duty, high duty, directed, legacy adv */
    wiced_bt_ble_ext_adv_event_property_t event_properties;
    /** primary_adv_int_min Range : 0x000020 to 0xFFFFFF(20 ms to 10, 485.759375 s) */
    uint32_t primary_adv_int_min;

    /** primary_adv_int_max Range : 0x000020 to 0xFFFFFF(20 ms to 10, 485.759375 s) */
    uint32_t primary_adv_int_max;

    /** primary_adv_channel_map BLE advertisement channel map(see #wiced_bt_ble_advert_chnl_map_e) */
    wiced_bt_ble_advert_chnl_map_t primary_adv_channel_map;

    /** own_addr_type Ignored in case of anonymous adv.See event_properties */
    wiced_bt_ble_address_type_t own_addr_type;

    /** peer_addr_type Peer address type */
    wiced_bt_ble_address_type_t peer_addr_type;

    /** peer_addr peer address */
    wiced_bt_device_address_t peer_addr;

    /** adv_filter_policy Adv filter policy */
    wiced_bt_ble_advert_filter_policy_t adv_filter_policy;

    /** adv_tx_power - 127 to + 126. 127 means host has no preference */
    int8_t adv_tx_power;

    /** primary_adv_phy Phy used to transmit ADV packets on Primary ADV channels */
    wiced_bt_ble_ext_adv_phy_t primary_adv_phy;

    /** secondary_adv_max_skip Valid only in case of extended ADV.Range 0 to FF.Maximum advertising events controller
        can skip before sending auxiliary adv packets on the secondary adv channel */
    uint8_t secondary_adv_max_skip;

    /** secondary_adv_phy Phy used to transmit ADV packets on secondary ADV channels .Valid only in case of extended ADV */
    wiced_bt_ble_ext_adv_phy_t secondary_adv_phy;

    /** adv_sid Advertisement set identifier is the value to be transmitted in extended ADV PDUs */
    wiced_bt_ble_ext_adv_sid_t adv_sid;

    /** scan_request_not scan request received notification enable/ disable */
    wiced_bt_ble_ext_adv_scan_req_notification_setting_t scan_request_not;

    /** primary phy adv options */
    wiced_bt_ble_phy_adv_options_t primary_phy_opts;

    /** secondary phy adv options */
    wiced_bt_ble_phy_adv_options_t secondary_phy_opts;
} wiced_bt_ble_ext_adv_params_t;


/** Extended ADV connection configuration structure */
typedef struct
{
    /**< Bit 0 = 1: Scan connectable advertisements on the LE 1M PHY. Connection
                  parameters for the LE 1M PHY are provided.
       Bit 1 = 1: Connection parameters for the LE 2M PHY are provided.
       Bit 2 = 1: Scan connectable advertisements on the LE Coded PHY. Connection
                  parameters for the LE Coded PHY are provided */
    wiced_bt_ble_ext_adv_phy_mask_t   initiating_phys;      /**< the PHY(s) bit mask on which the advertising
                                                            packets should be received on the primary advertising channel and the PHYs
                                                            for which connection parameters have been specified.*/

    uint16_t      scan_int[WICED_BT_BLE_EXT_ADV_NUM_PHYS];       /**< Range N: 0x0004 to 0xFFFF.Time = N * 0.625 ms. Time Range: 2.5 ms to 40.959375 s */
    uint16_t      scan_window[WICED_BT_BLE_EXT_ADV_NUM_PHYS];    /**< Range N: 0x0004 to 0xFFFF.Time = N * 0.625 ms. Time Range: 2.5 ms to 40.959375 s */
    uint16_t      min_conn_int[WICED_BT_BLE_EXT_ADV_NUM_PHYS];   /**< Range N: 0x0006 to 0x0C80 Time = N * 1.25 ms Time Range: 7.5 ms to 4 s */
    uint16_t      max_conn_int[WICED_BT_BLE_EXT_ADV_NUM_PHYS];   /**< Range N: 0x0006 to 0x0C80 Time = N * 1.25 ms Time Range: 7.5 ms to 4 s */
    uint16_t      conn_latency[WICED_BT_BLE_EXT_ADV_NUM_PHYS];   /**< Range N: 0x0000 to 0x01F3 */
    uint16_t      supervision_to[WICED_BT_BLE_EXT_ADV_NUM_PHYS]; /**< Range N: 0x000A to 0x0C80 Time = N * 10 ms Time Range: 100 ms to 32 s */
    uint16_t      min_ce_len[WICED_BT_BLE_EXT_ADV_NUM_PHYS];     /**< Range N: 0x0000 � 0xFFFF. Time = N * 0.625 ms */
    uint16_t      max_ce_len[WICED_BT_BLE_EXT_ADV_NUM_PHYS];     /**< Range N: 0x0000 � 0xFFFF. Time = N * 0.625 ms */

} wiced_bt_ble_ext_conn_cfg_t;

/** When controller receives succesfully periodic adv event based on create sync to periodic advertiser command,
    sync handle get generated by controller and reported in periodic sync established event
    scanner uniquely identifies periodic adv data from adv reports using this handle and advertismenet set id(SID) */
enum
{
    PERIODIC_SYNC_HANDLE_MIN = 0,
    PERIODIC_SYNC_HANDLE_MAX = 0x0EFF,
};

/** Sync_Handle to be used to identify the periodic advertiser. Range: 0x0000-0x0EFF */
typedef uint16_t wiced_bt_ble_periodic_adv_sync_handle_t;

#define IS_CONNECTABLE_ADV_REPORT(x) (x & (1 << 0))            /**< adv is connectable */
#define IS_SCANNABLE_ADV_REPORT(x) (x & (1 << 1))              /**< adv is scannable */
#define IS_DIRECTED_ADV_REPORT(x) (x & (1 << 2))               /**< directed adv */
#define IS_SCAN_RSP_ADV_REPORT(x) (x & (1 << 3))               /**< scan response */
#define IS_LEGACY_ADV_REPORT(x) (x & (1 << 4))                 /**< legacy adv */
#define IS_ADV_REPORT_DATA_STATUS_INCOMPLETE(x) (x & (1 << 5)) /**< adv data incomplete, more data to come */
#define IS_ADV_REPORT_DATA_STATUS_TRUNCATED(x) (x & (2 << 5))  /**< Incomplete, data truncated, no more to come */
/** Bit mask to identify the type of the adv received in extended adv report. (see #wiced_bt_ble_ext_adv_report_t) event_type filed */
typedef uint16_t wiced_bt_ble_adv_report_event_mask_t;

/**
Extended advertisement report data format

Note:When multiple advertising packets are used to complete a single advertising
report(i.e. if adv set data received as multipl adv reports), the RSSI and TxPower event parameters
shall be set based on the last packet received.*/
typedef struct
{
    wiced_bt_ble_adv_report_event_mask_t
        event_type;                            /**< Type of the adv. (See #wiced_bt_ble_adv_report_event_mask_t) */
    wiced_bt_ble_address_type_t addr_type;     /**< advertiser address type */
    wiced_bt_device_address_t bd_addr;         /**< advertiser address */
    wiced_bt_ble_ext_adv_phy_t prim_phy;       /**< PHY on which extended ADV PDUs received */
    wiced_bt_ble_ext_adv_phy_t sec_phy;        /**< PHY on which auxilary ADV PDUs received */
    wiced_bt_ble_ext_adv_sid_t adv_sid;        /**< advertising set identifier */
    int8_t tx_pwr;                             /**< advertisement transmit power */
    int8_t rssi;                               /**< advertisement RSSI */
    uint16_t periodic_adv_int;                 /**< Interval of the periodic advertisements if periodic adv enabled
                                                                on the same set. Range: N = 0x0006 to 0xFFFF, Time = N * 1.25 ms */
    wiced_bt_ble_address_type_t dir_addr_type; /**< Target device address type in case of directed adv report */
    wiced_bt_device_address_t dir_bdaddr;      /**< Target device address in case of directed adv report */
    uint8_t data_len;                          /**< adv data length */

    /* Place holder for adv data */
    uint8_t ad_data[]; /**< adv data  */
} wiced_bt_ble_ext_adv_report_t;

/** Min and Max possible number of reports in LE extended adv report event */
enum wiced_bt_ble_ext_adv_report_count_e
{
    ADV_REP_EVT_COUNT_MIN = 1,  /**< min number of reports in LE extended adv report event */
    ADV_REP_EVT_COUNT_MAX = 10, /**< max number of reports in LE extended adv report event */
};
typedef uint8_t
    wiced_bt_ble_ext_adv_report_count_t; /**< Min and Max reports (see #wiced_bt_ble_ext_adv_report_count_e)*/

/** Advertiser clock accuracy */
enum wiced_bt_ble_advertiser_clock_accuracy_e
{
    ADVERTISER_CLK_ACCURACY_500PPM,
    ADVERTISER_CLK_ACCURACY_250PPM,
    ADVERTISER_CLK_ACCURACY_150PPM,
    ADVERTISER_CLK_ACCURACY_100PPM,
    ADVERTISER_CLK_ACCURACY_75PPM,
    ADVERTISER_CLK_ACCURACY_50PPM,
    ADVERTISER_CLK_ACCURACY_30PPM,
    ADVERTISER_CLK_ACCURACY_20PPM,
};
typedef uint8_t
    wiced_bt_ble_advertiser_clock_accuracy_t; /**< Advertiser clock accuracy (see #wiced_bt_ble_advertiser_clock_accuracy_e) */

/** Sync extablished to periodic advertiser event data format.
    (The LE Periodic Advertising Sync Established event indicates that the
    Controller has received the first periodic advertising packet from an advertiser
    after the LE_Periodic_Advertising_Create_Sync Command has been sent to
    the Controller.)
*/
typedef struct
{
    uint8_t status;                                                     /**< HCI status */
    wiced_bt_ble_periodic_adv_sync_handle_t sync_handle;                /**< sync handle */
    wiced_bt_ble_ext_adv_sid_t adv_sid;                                 /**< advertisement set identifier */
    wiced_bt_ble_address_type_t adv_addr_type;                          /**< advertiser address type */
    wiced_bt_device_address_t adv_addr;                                 /**< advertiser address */
    wiced_bt_ble_ext_adv_phy_t adv_phy;                                 /**< advertiser phy */
    uint16_t periodic_adv_int;                                          /**< Periodic adv interval */
    wiced_bt_ble_advertiser_clock_accuracy_t advertiser_clock_accuracy; /**< advertiser clock accuracy */
} wiced_bt_ble_periodic_adv_sync_established_event_data_t;

/** Periodic advertising report data format */
typedef struct
{
    wiced_bt_ble_periodic_adv_sync_handle_t sync_handle; /**< sync handle */
    int8_t adv_tx_power; /**< advertisement transmit power Range: -127 to +126 dbm. 127 means tx power is not available */
    int8_t adv_rssi;  /**< RSSI. range: -127 to +126. 127 means RSSI is not available */
    uint8_t cte_type; /**< CTE Type */
    uint8_t data_status; /**< 0 = complete data, 1 = data incomplete more data to come, 2 = data truncated.other RFU */
    uint8_t data_len;    /**< Range: 0 -248. Other values RFU */
    uint8_t adv_data[WICED_BT_BLE_MAX_EXT_ADV_DATA_LEN]; /**< periodic adv data */
} wiced_bt_ble_periodic_adv_report_event_data_t;

/** sync handle and connection handle are same range */
typedef wiced_bt_ble_periodic_adv_sync_handle_t wiced_bt_ble_connection_handle_t;

/** extended adv set terminated event data format. This event generated asynchronously by the
    controller when adv set get terminated either adv duration expires or connection being created */
typedef struct
{
    uint8_t status;                                                     /**< HCI status */
    wiced_bt_ble_ext_adv_handle_t adv_handle;                           /**< advertisement set handle */
    wiced_bt_ble_connection_handle_t conn_handle;                       /**< connection handle. The conn_handle
                                                        parameter is only valid when advertising ends because a connection was created */
    uint8_t                             num_completed_ext_adv_events;   /**< number of completed extended advertising events the Controller had
                                                                        transmitted when either the duration elapsed or the maximum number of
                                                                        extended advertising events was reached; otherwise it shall be set to zero. */
} wiced_bt_ble_ext_adv_set_terminated_event_data_t;

/** scan request received event data format */
typedef struct
{
    wiced_bt_ble_ext_adv_handle_t   adv_handle;         /**< advertisement set handle */
    wiced_bt_ble_address_type_t     scanner_addr_type;  /**< Scanner address type */
    wiced_bt_device_address_t       scanner_address;    /**< Scanner address */
} wiced_bt_ble_scan_req_received_event_data_t;

/** BLE channel selection algorithms */
enum wiced_bt_ble_channel_sel_algo_e
{
    LE_CHANNEL_SEL_ALGO_1_USED,         /**< LE channel selection algorithm#1 used */
    LE_CHANNEL_SEL_ALGO_2_USED,         /**< LE channel selection algorithm#2 used */
};
typedef uint8_t wiced_bt_ble_channel_sel_algo_t;/**< LE channel algorithm selection (see #wiced_bt_ble_channel_sel_algo_e) */

/** Channel selection algorithm event data format */
typedef struct
{
    wiced_bt_ble_connection_handle_t      connection_handle;    /**< HCI connection handle */
    wiced_bt_ble_channel_sel_algo_t       channel_sel_algo;     /**< BLE channel selection algorithm used for this connection */

    /* remaining RFU */
} wiced_bt_ble_channel_sel_algo_event_data_t;

/** BIGInfo report */
typedef struct
{
    uint16_t sync_handle; /**< Sync_Handle identifying the periodic advertising train (Range: 0x0000 to 0x0EFF) */
    uint8_t num_bis;      /**< Value of the Num_BIS subfield of the BIGInfo field */
    uint8_t number_of_subevents;        /**< Value of the NSE subfield of the BIGInfo field */
    uint16_t iso_interval;              /**< Value of the ISO_Interval subfield of the BIGInfo field */
    uint8_t burst_number;               /**< Value of the BN subfield of the BIGInfo field */
    uint8_t pretransmission_offset;     /**< Value of the PTO subfield of the BIGInfo field */
    uint8_t immediate_repetition_count; /**< Value of the IRC subfield of the BIGInfo field */
    uint16_t max_pdu;                /**< Value of the Max_PDU subfield of the BIGInfo field in the Advertising PDU */
    uint32_t sdu_interval;           /**< Value of the SDU_Interval subfield of the BIGInfo field */
    uint16_t max_sdu;                /**< Value of the Max_SDU subfield of the BIGInfo field in the Advertising PDU */
    wiced_bt_isoc_phy_t phy;         /**< The transmitter PHY of packets */
    wiced_bt_isoc_framing_t framing; /**< Framing parameter */
    wiced_bt_isoc_encryption_t encryption; /**< BIG carries encrypted or unencrypted data */
} wiced_bt_ble_biginfo_adv_report_t;

/** Periodic Adv Sync Transfer Received Event Data */
typedef struct
{
    wiced_bt_ble_periodic_adv_sync_established_event_data_t sync_data; /**< Periodic Adv Sync Data */
    wiced_bt_ble_connection_handle_t conn_handle;                      /**< connection handle */
    uint16_t service_data; /**< Service Data value provided by the peer device */
} wiced_bt_ble_periodic_adv_sync_transfer_event_data_t;

/* @cond BETA_API
   beta APIs for Periodic Advertising with Response*/
#define WICED_BT_MAX_PAWR_SUBEVENT_DATA_LEN  251

/** Periodic Advertising with Response (PAWR) Sync Established Event Data */
typedef struct
{
    uint8_t                                 status;                     /**< HCI status */
    wiced_bt_ble_periodic_adv_sync_handle_t sync_handle;                /**< sync handle */
    wiced_bt_ble_ext_adv_sid_t              adv_sid;                    /**< advertisement set identifier */
    wiced_bt_ble_address_type_t             adv_addr_type;              /**< advertiser address type */
    wiced_bt_device_address_t               adv_addr;                   /**< advertiser address */
    wiced_bt_ble_ext_adv_phy_t              adv_phy;                    /**< advertiser phy */
    uint16_t                                periodic_adv_int;           /**< Periodic adv interval */
    wiced_bt_ble_advertiser_clock_accuracy_t advertiser_clock_accuracy; /**< advertiser clock accuracy */
    uint8_t                                 num_subevents;              /**< number of subevents */
    uint8_t                                 subevent_interval;          /**< subevent interval */
    uint8_t                                 response_slot_delay;        /**< response slot delay */
    uint8_t                                 response_slot_spacing;      /**< response slot spacing */
} wiced_bt_ble_pawr_sync_established_event_data_t;

/** Periodic Advertising with Response (PAWR) Subevent Data Request Event Data */
typedef struct
{
    wiced_bt_ble_ext_adv_handle_t   adv_handle;                 /**< advertisement set handle */
    uint8_t                         subevent_start;             /**< first subevent */
    uint8_t                         subevent_start_count;       /**< number of subevents */
} wiced_bt_ble_pawr_subevent_data_req_event_data_t;

/** Periodic Advertising with Response (PAWR) Response Report Event Data */
typedef struct
{
    uint8_t     adv_handle;
    uint8_t     subevent;
    uint8_t     tx_status;
    uint8_t     tx_power;
    uint8_t     rssi;
    uint8_t     cte_type;
    uint8_t     response_slot;
    uint8_t     data_status;
    uint8_t     data_len;
    uint8_t     data[WICED_BT_MAX_PAWR_SUBEVENT_DATA_LEN];
} wiced_bt_ble_pawr_rsp_report_event_data_t;

/** Periodic Advertising with Response (PAWR) Indication Report Event Data */
typedef struct
{
    wiced_bt_ble_periodic_adv_sync_handle_t sync_handle;                /**< sync handle */
    uint8_t     tx_power;
    uint8_t     rssi;
    uint8_t     cte_type;
    uint16_t    periodic_evt_counter;
    uint8_t     sub_event;
    uint8_t     data_status;
    uint8_t     data_length;                                /**< Length of the subevent indication data  */
    uint8_t     data[WICED_BT_MAX_PAWR_SUBEVENT_DATA_LEN];  /**< Subevent data  */
} wiced_bt_ble_pawr_ind_report_event_data_t;
/* @endcond */

/** ADV extension events to the application */
typedef enum
{
    WICED_BT_BLE_PERIODIC_ADV_SYNC_ESTABLISHED_EVENT,   /**< Sync established to periodic advertiser's periodic advertisement. Event Data : wiced_bt_ble_periodic_adv_sync_established_event_data_t */
    WICED_BT_BLE_PERIODIC_ADV_REPORT_EVENT,             /**< Periodic adv report. Event Data: wiced_bt_ble_periodic_adv_report_event_data_t */
    WICED_BT_BLE_PERIODIC_ADV_SYNC_LOST_EVENT,          /**< Periodic sync lost event. Event Data: wiced_bt_ble_periodic_adv_sync_handle_t */
    WICED_BT_BLE_ADV_SET_TERMINATED_EVENT,              /**< Advertising set terminated becaue either connection being created or adv timeout. Event data: wiced_bt_ble_ext_adv_set_terminated_event_data_t */
    WICED_BT_BLE_SCAN_REQUEST_RECEIVED_EVENT,           /**< scan request received event. Event data: wiced_bt_ble_scan_req_received_event_data_t */
    WICED_BT_BLE_CHANNEL_SEL_ALGO_EVENT,                /**< LE Channel selected algorithm event. Event Data: wiced_bt_ble_channel_sel_algo_event_data_t */
    WICED_BT_BLE_BIGINFO_ADV_REPORT_EVENT,              /**< BIGInfo adv report event. Event Data: wiced_bt_ble_biginfo_adv_report_t */
    WICED_BT_BLE_PERIODIC_ADV_SYNC_TRANSFER_EVENT,      /**< Periodic Adv Sync Transfer Event. Event Data: wiced_bt_ble_periodic_adv_sync_transfer_event_data_t */
	/* @cond BETA_API
	beta APIs for Periodic Advertising with Response*/
    WICED_BT_BLE_PAWR_SYNC_ESTABLISHED_EVENT,           /**< Periodic Adv Sync Transfer Event. Event Data: wiced_bt_ble_pawr_sync_established_event_data_t */
    WICED_BT_BLE_PAWR_SUBEVENT_DATA_REQ_EVENT,          /**< Periodic Adv Sync Transfer Event. Event Data: wiced_bt_ble_pawr_subevent_data_req_event_data_t */
    WICED_BT_BLE_PAWR_IND_REPORT_EVENT,                 /**< Periodic Adv Sync Transfer Event. Event Data: wiced_bt_ble_pawr_ind_report_event_data_t */
    WICED_BT_BLE_PAWR_RSP_REPORT_EVENT                  /**< Periodic Adv Sync Transfer Event. Event Data: wiced_bt_ble_pawr_rsp_report_event_data_t */
	/* @endcond */
} wiced_bt_ble_adv_ext_event_t;

/** union of events data */
typedef union
{
    wiced_bt_ble_periodic_adv_sync_established_event_data_t sync_establish;     /**< Data for WICED_BT_BLE_PERIODIC_ADV_SYNC_ESTABLISHED_EVENT*/
    wiced_bt_ble_periodic_adv_report_event_data_t           periodic_adv_report;/**< Data for WICED_BT_BLE_PERIODIC_ADV_REPORT_EVENT*/
    wiced_bt_ble_periodic_adv_sync_handle_t                 sync_handle;        /**< Data for WICED_BT_BLE_PERIODIC_ADV_SYNC_LOST_EVENT*/
    wiced_bt_ble_ext_adv_set_terminated_event_data_t        adv_set_terminated; /**< Data for WICED_BT_BLE_ADV_SET_TERMINATED_EVENT*/
    wiced_bt_ble_scan_req_received_event_data_t             scan_req_received;  /**< Data for WICED_BT_BLE_SCAN_REQUEST_RECEIVED_EVENT*/
    wiced_bt_ble_channel_sel_algo_event_data_t              channel_sel_algo;   /**< Data for WICED_BT_BLE_CHANNEL_SEL_ALGO_EVENT*/
    wiced_bt_ble_biginfo_adv_report_t                       biginfo_adv_report; /**< Data for WICED_BT_BLE_BIGINFO_ADV_REPORT_EVENT*/
    wiced_bt_ble_periodic_adv_sync_transfer_event_data_t    sync_transfer;      /**< Data for WICED_BT_BLE_PERIODIC_ADV_SYNC_TRANSFER_EVENT */
	/* @cond BETA_API
	beta APIs for Periodic Advertising with Response*/
    wiced_bt_ble_pawr_sync_established_event_data_t         pawr_sync;          /**< Data for WICED_BT_BLE_PAWR_SYNC_ESTABLISHED_EVENT*/
    wiced_bt_ble_pawr_subevent_data_req_event_data_t        pawr_data_req;      /**< Data for WICED_BT_BLE_PAWR_SUBEVENT_DATA_REQ_EVENT*/
    wiced_bt_ble_pawr_ind_report_event_data_t               pawr_ind_report;    /**< Data for WICED_BT_BLE_PAWR_IND_REPORT_EVENT*/
    wiced_bt_ble_pawr_rsp_report_event_data_t               pawr_rsp_report;    /**< Data for WICED_BT_BLE_PAWR_RSP_REPORT_EVENT*/
	/* @endcond */
} wiced_bt_ble_adv_ext_event_data_t;

/** Configuration for extended scanning */
typedef struct
{
    wiced_bt_ble_ext_adv_phy_mask_t  scanning_phys;      /**< The Scanning_PHYs parameter indicates the PHY(s) on which the advertising
                                                                packets should be received on the primary advertising channel.*/

    uint8_t     enc_phy_scan_type;          /**< encoded phy scan type. (active or passive) */
    uint16_t    enc_phy_scan_int;           /**< encoded phy scan interval */
    uint16_t    enc_phy_scan_win;           /**< encoded phy scan window */

    /** When the Duration and Period parameters are non-zero, the Controller shall
    scan for the duration of the Duration parameter within a scan period specified
    by the Period parameter. After the scan period has expired, a new scan period
    shall begin and scanning shall begin again for the duration specified. The scan
    periods continue until the Host disables scanning. Duration parameter cannot be greater than or equal to the Period parameter
    If the Duration parameter is zero or both the Duration parameter and Period parameter are non-zero controller continue
    scanning until host disable scanning with enable set to false */
    uint16_t    duration;                   /**< Scan duration */
    uint16_t    period;                     /**< Scan period */
} wiced_bt_ble_ext_scan_config_t;

/**
 * Callback wiced_bt_ble_adv_ext_event_cb_fp_t
 *
 * Adv extension command status, command complete event and LE adv extension meta event callback
 *
 * @param event             : Event type (see #wiced_bt_ble_adv_ext_event_t)
 * @param p_data            : Event data (see #wiced_bt_ble_adv_ext_event_data_t)
 *
 * @return Nothing
 */
typedef void (*wiced_bt_ble_adv_ext_event_cb_fp_t) (wiced_bt_ble_adv_ext_event_t event, wiced_bt_ble_adv_ext_event_data_t *p_data);

/* @cond BETA_API
   beta APIs for Periodic Advertising with Response*/
/** Configuration for Periodic Advertising with Response (PAWR) subevent indication data
**  which is sent by the central device at the start of each subevent
*/
typedef struct
{
    uint8_t     subevent_num;               /**< The subevent number */
    uint8_t     rsp_slot_start;             /**< Response slot start */
    uint8_t     rsp_slot_count;             /**< Response slot count */
    uint8_t     ind_data_length;            /**< Length of the subevent indication data  */
    uint8_t     ind_data[WICED_BT_MAX_PAWR_SUBEVENT_DATA_LEN]; /**< Subevent data  */
} wiced_bt_ble_pawr_subevent_ind_data_t;
/* @endcond */


/******************************************************
 *               Function Declarations
 *
 ******************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup  btm_ble_adv_scan_functions        Advertisement & Scan
 * @ingroup     btm_ble_api_functions
 *
 * This section provides functions for BLE advertisement and BLE scan operations.
 *
 * @{
 */

/**
 * Start advertising.
 *
 * Use #wiced_bt_ble_set_raw_advertisement_data to configure advertising data
 * prior to starting avertisements. The advertisements are stopped upon successful LE connection establishment.
 *
 * @note 1. Steps for undirected ADVs viz., BTM_BLE_ADVERT_UNDIRECTED_HIGH, BTM_BLE_ADVERT_UNDIRECTED_LOW,
 * and non connectable advs viz., BTM_BLE_ADVERT_NONCONN_HIGH, BTM_BLE_ADVERT_NONCONN_LOW
 *   a) Set ADV data
 *   b) Set Scan Response data if adv type is scannable
 * @note 2. if adv type is set to Directed then the stack resets any advertisement data which has been set for an earlier advertisement. Refer note 1 prior to attempting an undirected advertisement
 *
 * The <b>advert_mode</b> parameter determines what advertising parameters and durations
 * to use (as specified by the application configuration).
 *
 * @param[in]       advert_mode                         : advertisement mode
 * @param[in]       directed_advertisement_bdaddr_type  : BLE_ADDR_PUBLIC or BLE_ADDR_RANDOM (if using directed advertisement mode)
 * @param[in]       directed_advertisement_bdaddr_ptr   : Directed advertisement address (NULL if not using directed advertisement)
 *
 * @return      status
 *
 */
wiced_result_t wiced_bt_start_advertisements(wiced_bt_ble_advert_mode_t advert_mode, wiced_bt_ble_address_type_t directed_advertisement_bdaddr_type, wiced_bt_device_address_ptr_t directed_advertisement_bdaddr_ptr);

/**
 *
 * Get current advertising mode
 *
 * @return          Current advertising mode (refer #wiced_bt_ble_advert_mode_e)
 *
 */
wiced_bt_ble_advert_mode_t wiced_bt_ble_get_current_advert_mode(void);

/**
 * Set advertisement raw data to the controller. Application can invoke #wiced_bt_start_advertisements
 * after setting the adv data. Max length of the advertising data to be set is 31 bytes.
 * API returns an error if the length of data being set exceeds 31 bytes
 *
 * @param[in] num_elem :   number of ADV data element
 * @param[in] p_data :      advertisement raw data
 *
 * @return          wiced_result_t
 *
 */
wiced_result_t wiced_bt_ble_set_raw_advertisement_data(uint8_t num_elem, wiced_bt_ble_advert_elem_t *p_data);

/**
 * Write advertisement raw data to application buffer.
 *
 * @note: This API does not write data to the controller
 *
 * @param[in] p_adv : buffer pointer at which the raw data in \p p_type_data is to be written
 * @param[in] adv_len : length of the buffer pointer pointed to by \p p_adv, should be atleast (2 + \p type_len)
 *                     for a successful write operation
 * @param[in] adv_type: adveritsement type
 * @param[in] p_type_data: advertisement data of \p adv_type
 * @param[in] type_len : length of advertisement data at \p p_type_data of \p adv_type
 *
 * @return  length of the \p p_adv buffer written.
 *
 */
int wiced_bt_ble_build_raw_advertisement_data(uint8_t *p_adv,
                                              int adv_len,
                                              wiced_bt_ble_advert_type_t adv_type,
                                              uint8_t *p_type_data,
                                              uint16_t type_len);

/**
 * Parse advertising data (returned from scan results callback #wiced_bt_ble_scan_result_cback_t).
 * Look for specified advertisement data type.
 *
 * @param[in]       p_adv       : pointer to advertisement data
 * @param[in]       type        : advertisement data type to look for
 * @param[out]      p_length    : length of advertisement data (if found)
 *
 * @return          pointer to start of requested advertisement data (if found). NULL if requested data type not found.
 *
 */
uint8_t *wiced_bt_ble_check_advertising_data( uint8_t *p_adv, wiced_bt_ble_advert_type_t type, uint8_t *p_length);

/**
 * When multiple entry for same adv type is available in the adv data this api will help to get next entry of specified advertisement data type
 * when there is a single instance use wiced_bt_ble_check_advertising_data
 *
 * @param[in]       p_adv       : pointer to advertisement data
 * @param[in]       p_offset    : offset from the start of the adv data recevied, also returns next offset to start searching from
 * @param[in]       type        : advertisement data type to look for
 * @param[out]      p_length    : length of advertisement data (if found)
 *
 * @return          pointer to next requested advertisement data (if found). NULL if requested data type not found.
 *
 */
uint8_t *wiced_bt_ble_get_next_adv_entry(uint8_t *p_adv,
                                         int *p_offset,
                                         wiced_bt_ble_advert_type_t type,
                                         uint8_t *p_length);
    /**
 *
 * Update the filter policy of advertiser.
 *
 *  @param[in]      advertising_policy: advertising filter policy
 *
 *  @return         TRUE if successful
 */
wiced_bool_t wiced_btm_ble_update_advertisement_filter_policy(wiced_bt_ble_advert_filter_policy_t advertising_policy);

/**
*  Command to set LE Advertisement tx power
*
* @param[in]       power          :  power value in db
* @param[in]       p_cb           :  Result callback (wiced_bt_set_adv_tx_power_result_t will be passed to the callback)
*
* @return          wiced_result_t
*                  WICED_BT_PENDING if callback is not NULL.
*                  WICED_BT_SUCCESS if callback is NULL
*
*
**/
wiced_result_t wiced_bt_ble_set_adv_tx_power(int8_t power, wiced_bt_dev_vendor_specific_command_complete_cback_t *p_cb);

/**
* Read LE Advertisement transmit power
* @note: This API can be used to get Tx power for Legacy advertisements.
*        It will return an error if the application is using Extended Advertisements.
*        See Bluetool Core spec 5.2, Vol 4, Part E, section 3.1.1 Legacy and extended advertising
*
* @param[in]       p_cback         : Result callback (wiced_bt_tx_power_result_t will be passed to the callback)
*
* @return
*
*                  WICED_BT_PENDING if command issued to controller.
*                  WICED_BT_NO_RESOURCES if couldn't allocate memory to issue command
*                  WICED_BT_BUSY if command is already in progress
*                  WICED_BT_ILLEGAL_VALUE if the callback is NULL
*/
wiced_result_t wiced_bt_ble_read_adv_tx_power(wiced_bt_dev_cmpl_cback_t *p_cback);

/**
 *
 * Set scan response raw data
 *
 * @param[in] num_elem :   number of scan response data elements
 * @param[in] p_data :     scan response raw data
 *
 * @return          status of the operation
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_raw_scan_response_data(uint8_t num_elem,
                                                        wiced_bt_ble_advert_elem_t *p_data);

/**
 *
 * This function makes the device start or stop operating in the observer role.
 * The observer role device receives advertising events from a broadcast device.
 *
 * @note This API uses following parameters from the configuration settings \ref wiced_bt_cfg_ble_t.p_ble_scan_cfg, \n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.low_duty_scan_interval,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.low_duty_scan_window, \n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.scan_mode, \n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.scan_mode
 *
 *
 *
 * @param[in] start :               TRUE to start the observer role
 * @param[in] duration :            the duration for the observer role
 * @param[in] p_scan_result_cback : scan result callback
 *
 * @return          status of the operation
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_observe (wiced_bool_t start, uint8_t duration, wiced_bt_ble_scan_result_cback_t *p_scan_result_cback);

/**
 * Start LE scanning
 *
 * The <b>scan_type</b> parameter determines what scanning parameters and durations
 *                  to use (as specified by the application configuration).
 *
 *                  Scan results are notified using \p p_scan_result_cback
 *
 * @note This API uses following parameters from the configuration settings of \ref wiced_bt_cfg_ble_t.p_ble_scan_cfg,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.high_duty_scan_interval,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.high_duty_scan_window,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.high_duty_scan_duration,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.low_duty_scan_interval,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.low_duty_scan_window,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.low_duty_scan_duration,\n
 *       \ref wiced_bt_cfg_ble_scan_settings_t.scan_mode \n
 *
 *
 * @param[in]       scan_type : BTM_BLE_SCAN_TYPE_NONE, BTM_BLE_SCAN_TYPE_HIGH_DUTY,  BTM_BLE_SCAN_TYPE_LOW_DUTY
 * @param[in]       duplicate_filter_enable : TRUE or FALSE to enable or disable  duplicate filtering
 *
 * @param[in]       p_scan_result_cback : scan result callback
 *
 * @return          wiced_result_t \n
 *
 * <b> WICED_BT_PENDING </b>        : if successfully initiated \n
 * <b> WICED_BT_BUSY </b>           : if already in progress \n
 * <b> WICED_BT_ILLEGAL_VALUE </b>  : if parameter(s) are out of range \n
 * <b> WICED_BT_NO_RESOURCES </b>   : if could not allocate resources to start the command \n
 * <b> WICED_BT_WRONG_MODE </b>     : if the device is not up.
 */
wiced_result_t  wiced_bt_ble_scan (wiced_bt_ble_scan_type_t scan_type, wiced_bool_t duplicate_filter_enable, wiced_bt_ble_scan_result_cback_t *p_scan_result_cback);

/**
 *
 * Get current scan state
 *
 * @return          wiced_bt_ble_scan_type_t \n
 *
 * <b> BTM_BLE_SCAN_TYPE_NONE </b>         Not scanning \n
 * <b> BTM_BLE_SCAN_TYPE_HIGH_DUTY </b>    High duty cycle scan \n
 * <b> BTM_BLE_SCAN_TYPE_LOW_DUTY </b>     Low duty cycle scan
 */
wiced_bt_ble_scan_type_t wiced_bt_ble_get_current_scan_state(void);

/**
 *
 * Update the filter policy of scanning.
 *
 *  @param[in]      scanner_policy: scanning filter policy
 *
 *  @return         void
 */
void wiced_bt_ble_update_scanner_filter_policy(wiced_bt_ble_scanner_filter_policy_t scanner_policy);

/**@} btm_ble_adv_scan_functions */

/**
 * @addtogroup  btm_ble_conn_filter_accept_list_functions        Connection and Filter Accept List
 * @ingroup     btm_ble_api_functions
 *
 * This section provides functions for BLE connection related and Filter Accept List operations.
 *
 * @{
 */

/**
 *
 * Set BLE background connection procedure type.
 *
 * @param[in]       conn_type: BTM_BLE_CONN_NONE or BTM_BLE_CONN_AUTO
 * @param[in]       p_select_cback: UNUSED
 *
 * @return          TRUE if background connection set
 *
 */
wiced_bool_t wiced_bt_ble_set_background_connection_type (wiced_bt_ble_conn_type_t conn_type, void *p_select_cback);

/**
 *
 * This function is called to add or remove a device into/from the
 * filter list. The background connection procedure is decided by
 * the background connection type, it can be auto connection, or none.
 *
 * @param[in]       add_remove    : TRUE to add; FALSE to remove.
 * @param[in]       remote_bda    : device address to add/remove.
 * @param[in]       ble_addr_type : Address type.

 * @return          TRUE if successful
 *
 */
wiced_bool_t wiced_bt_ble_update_background_connection_device(wiced_bool_t add_remove, wiced_bt_device_address_t remote_bda, wiced_bt_ble_address_type_t ble_addr_type);

/**
 * To read LE connection parameters based on connection address received in gatt connection up indication.
 *
 * @param[in]       remote_bda          : remote device address.
 * @param[in]       p_conn_parameters   : Connection Parameters
 *
 * @return          wiced_result_t \n
 *
 * <b> WICED_BT_ILLEGAL_VALUE </b> : if p_conn_parameters is NULL. \n
 * <b> WICED_BT_UNKNOWN_ADDR </b>  : if device address is bad. \n
 * <b> WICED_BT_SUCCESS </b> otherwise.
 *
 */
wiced_result_t wiced_bt_ble_get_connection_parameters(wiced_bt_device_address_t remote_bda, wiced_bt_ble_conn_params_t *p_conn_parameters);

/**
 *
 * Add or remove device from advertising filter Accept List
 *
 * @param[in]       add: TRUE to add; FALSE to remove
 * @param[in]       addr_type: Type of the addr
 * @param[in]       remote_bda: remote device address.
 *
 * @return          wiced_bool_t (<b> WICED_TRUE </b> if successful else <b> WICED_FALSE </b>)
 *
 */
wiced_bool_t wiced_bt_ble_update_advertising_filter_accept_list(wiced_bool_t add, wiced_bt_ble_address_type_t addr_type, wiced_bt_device_address_t remote_bda);

/**
 *
 * Add or remove device from scanner filter Accept List
 *
 * @param[in]       add: TRUE to add; FALSE to remove
 * @param[in]       remote_bda: remote device address.
 * @param[in]       addr_type   : remote device address type .
 *
 * @return          WICED_TRUE if successful else WICED_FALSE
 *
 */
wiced_bool_t wiced_bt_ble_update_scanner_filter_list(wiced_bool_t add, wiced_bt_device_address_t remote_bda,  wiced_bt_ble_address_type_t addr_type);

/**
 *
 * Request clearing filter Accept List in controller side
 *
 *
 * @return          TRUE if request of clear is sent to controller side
 *
 */
wiced_bool_t wiced_bt_ble_clear_filter_accept_list(void);

/**
 *
 * Returns size of Filter Accept List size in controller side
 *
 *
 * @return          size of Filter Accept List in current controller
 *
 */
uint8_t wiced_bt_ble_get_filter_accept_list_size(void);

/**@} btm_ble_conn_filter_accept_list_functions */

/**
 * @addtogroup  btm_ble_phy_functions        Phy
 * @ingroup     btm_ble_api_functions
 *
 * This section provides functionality to read and update PHY.
 *
 * @{
 */

/**
 * Host to read the current transmitter PHY and receiver PHY on the connection identified by the remote bdaddr.
 * phy results notified using #wiced_bt_ble_read_phy_complete_callback_t callback
 *
 * @param[in]       remote_bd_addr                   - remote device address
 * @param[in]       p_read_phy_complete_callback     - read phy complete callback
 *
 * @return          wiced_result_t \n
 *
 * <b> WICED_BT_SUCCESS </b>        : if the request was successfully sent to HCI. \n
 * <b> WICED_BT_UNKNOWN_ADDR </b>   : if device address does not correspond to a connected remote device \n
 * <b> WICED_BT_ILLEGAL_VALUE </b>  : if p_read_phy_complete_callback is NULL \n
 * <b> WICED_BT_NO_RESOURCES </b>   : if could not allocate resources to start the command
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_read_phy (wiced_bt_device_address_t remote_bd_addr,
                        wiced_bt_ble_read_phy_complete_callback_t *p_read_phy_complete_callback);

/**
 * Host to configure default transmitter phy and receiver phy to
 * be used for all subsequent connections over the LE transport.
 *
 *
 * @param[in]       phy_preferences      - Phy preferences
 *
 * Note : remote_bd_addr field of the phy_preferences is ignored.
 *
 * @return          wiced_result_t
 *
 * <b> WICED_BT_SUCCESS </b>        : if the request was successfully sent to HCI. \n
 * <b> WICED_BT_ILLEGAL_VALUE </b>  : if phy_preferences is NULL \n
 * <b> WICED_BT_NO_RESOURCES </b>   : if could not allocate resources to start the command
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_default_phy (wiced_bt_ble_phy_preferences_t *phy_preferences);

/**
 * Host to configure the LE link to 1M or 2M and LE coding to be used
 *
 * @param[in]       phy_preferences      - Phy preferences
 *
 * @return          wiced_result_t \n
 *
 * <b> WICED_BT_SUCCESS </b>        : if the request was successfully sent to HCI. \n
 * <b> WICED_BT_ILLEGAL_VALUE </b>  : if phy_preferences is NULL \n
 * <b> WICED_BT_UNKNOWN_ADDR </b>   : if device address does not correspond to a connected remote device \n
 * <b> WICED_BT_NO_RESOURCES </b>   : if could not allocate resources to start the command
 *
 */
wiced_result_t wiced_bt_ble_set_phy (wiced_bt_ble_phy_preferences_t *phy_preferences);

/**
 * Set channel classification for the available 40 channels.
 *
 * Channel n is bad = 0.
 * Channel n is unknown = 1.
 *
 * At least one channel shall be marked as unknown.
 *
 * @param[in]       ble_channel_map
 *
 * @return          wiced_result_t \n
 *
 * <b> WICED_BT_SUCCESS </b>        if successfully initiated \n
 * <b> WICED_BT_NO_RESOURCES </b>   if could not allocate resources to start the command
 */
wiced_result_t wiced_bt_ble_set_channel_classification(const wiced_bt_ble_chnl_map_t ble_channel_map);
/**@} btm_ble_phy_functions */

/**
 * @addtogroup  btm_ble_multi_adv_functions        MultiAdv
 * @ingroup     btm_ble_api_functions
 *
 * This section describes Multiple Advertisement API, using this interface application can enable more than one advertisement train.
 * @note Controller should have support for this feature.
 *
 * @{
 */

/**
 * Start/Stop Mulit advertisements.
 * wiced_start_multi_advertisements gives option to start multiple adverstisment instances
 * Each of the instances can set different #wiced_set_multi_advertisement_params and #wiced_set_multi_advertisement_data.
 * Hence this feature allows the device to advertise to multiple Centrals at the same time like a multiple peripheral device,
 * with different advertising data, Random private addresses, tx_power etc.
 *
 * @param[in]       advertising_enable : MULTI_ADVERT_START  - Advertising is enabled
 *                                       MULTI_ADVERT_STOP   - Advertising is disabled
 *
 * @param[in]       adv_instance       : 1 to MULTI_ADV_MAX_NUM_INSTANCES
 *
 * @return          wiced_bt_dev_status_t
 *
 *                  TRUE if command succeeded
 */
 wiced_bt_dev_status_t wiced_start_multi_advertisements( uint8_t advertising_enable, uint8_t adv_instance );

/**
 * Set multi advertisement data for each adv_instance
 *
 *
 * @param[in]       p_data        : Advertising Data ( Max length 31 bytess)
 * @param[in]       data_len      : Advertising Data len ( Max 31 bytes )
 * @param[in]       adv_instance  : 1 to MULTI_ADV_MAX_NUM_INSTANCES
 *
 * @return          wiced_bt_dev_status_t \n
 *                  WICED_BT_SUCCESS if command succeeded
 */
wiced_bt_dev_status_t wiced_set_multi_advertisement_data( uint8_t * p_data, uint8_t data_len, uint8_t adv_instance );

/**
 * Set multi advertisement params for each adv_instance
 *
 *
 * @param[in]       adv_instance  : 1 to MULTI_ADV_MAX_NUM_INSTANCES
 * @param[in]       params        : Advertising params refer #wiced_bt_ble_multi_adv_params_t
 *
 * @return          wiced_bt_dev_status_t \n
 *                  WICED_BT_SUCCESS if command succeeded
 */

wiced_bt_dev_status_t wiced_set_multi_advertisement_params(uint8_t adv_instance, wiced_bt_ble_multi_adv_params_t *params);


/**
 * Set multi advertisement data for scan response
 *
 * @param[in]       p_data        : Advertising Data ( Max length 31 bytess)
 * @param[in]       data_len      : Advertising Data len ( Max 31 bytes )
 * @param[in]       adv_instance  : 1 to MULTI_ADV_MAX_NUM_INSTANCES
 *
 * @return          wiced_bt_dev_status_t \n
 *                  WICED_BT_SUCCESS if command succeeded
 */
wiced_bt_dev_status_t wiced_set_multi_advertisement_scan_response_data( uint8_t * p_data, uint8_t data_len, uint8_t adv_instance );

/**
 * Set multi advertisement random address for an instance
 *
 *
 * @param[in]       randomAddr    : own random address
 * @param[in]       adv_instance  : 1 to MULTI_ADV_MAX_NUM_INSTANCES
 *
 * @return          wiced_bt_dev_status_t \n
 *                  WICED_BT_SUCCESS if command succeeded
 */
wiced_bt_dev_status_t wiced_set_multi_advertisements_random_address( wiced_bt_device_address_t randomAddr, uint8_t adv_instance );

/**
 * Allows the application to register a callback that will be invoked
 * just before an ADV packet is about to be sent out and immediately after.
 *
 * @param[in]       adv_instance                : 1 to MULTI_ADV_MAX_NUM_INSTANCES
 * @param[in]       clientCallback              : Pointer to a function that will be invoked in application thread context
 *                  with WICED_BT_ADV_NOTIFICATION_READY for before ADV and WICED_BT_ADV_NOTIFICATION_DONE after ADV packet is complete.
 * @param[in]       advanceNoticeInMicroSeconds : Number of microseconds before the ADV the notification is to be sent. Will be rounded down to
 *                  the nearest 1.25mS. Has to be an even multiple of 625uS.
 *
 * @return          wiced_bool_t \n
 *                  WICED_TRUE if command succeeded
 */
wiced_bool_t wiced_bt_notify_multi_advertisement_packet_transmissions( uint8_t adv_instance, void (*clientCallback)( uint32_t ),
                                                                       uint32_t advanceNoticeInMicroSeconds );

/**@} btm_ble_multi_adv_functions */

/**
 * @} btm_ble_api_functions
 */

/**
 * @ingroup     btm_ble_sec_api_functions
 *
 * @{
 */

/**
 *
 * Grant or deny access.  Used in response to an BTM_SECURITY_REQUEST_EVT event.
 *
 * @param[in]       bd_addr     : peer device bd address.
 * @param[in]       res         : WICED_BT_SUCCESS to grant access;
                                  WICED_BT_UNSUPPORTED , if local device does not allow pairing;
                                  WICED_BT_REPEATED_ATTEMPTS otherwise
 *
 * @return          <b> None </b>
 *
 */
void wiced_bt_ble_security_grant(wiced_bt_device_address_t bd_addr, wiced_bt_dev_status_t res);

/**
 * Sign the data using AES128 CMAC algorith.
 *
 * @param[in]       bd_addr: target device the data to be signed for.
 * @param[in]       p_text: signing data
 * @param[in]       len: length of the signing data
 * @param[in]       signature: output parameter where data signature is going to be stored
 *
 * @return          TRUE if signing successful, otherwise FALSE.
 *
 */
wiced_bool_t wiced_bt_ble_data_signature(wiced_bt_device_address_t bd_addr,
                                         uint8_t *p_text,
                                         uint16_t len,
                                         wiced_dev_ble_signature_t signature);

/**
 * Verify the data signature
 *
 * @param[in]       bd_addr: target device the data to be signed for.
 * @param[in]       p_orig:  original data before signature.
 * @param[in]       len: length of the signing data
 * @param[in]       counter: counter used when doing data signing
 * @param[in]       p_comp: signature to be compared against.
 *
 * @return          TRUE if signature verified correctly; otherwise FALSE.
 *
 */
wiced_bool_t wiced_bt_ble_verify_signature(wiced_bt_device_address_t bd_addr,
                                           uint8_t *p_orig,
                                           uint16_t len,
                                           uint32_t counter,
                                           uint8_t *p_comp);

/**
 * Get security mode 1 flags and encryption key size for LE peer.
 *
 * @param[in]       bd_addr         : peer address
 * @param[out]      p_le_sec_flags  : security flags (see #wiced_bt_ble_sec_flags_e)
 * @param[out]      p_le_key_size   : encryption key size
 *
 * @return          TRUE if successful
 *
 */
wiced_bool_t wiced_bt_ble_get_security_state(wiced_bt_device_address_t bd_addr,
                                             uint8_t *p_le_sec_flags,
                                             uint8_t *p_le_key_size);

/**
 * Updates privacy mode if device is already available in controller resolving list
 *
 * @param[in]       remote_bda      -remote device address received during connection up
 * @param[in]       rem_bda_type    -remote device address type received during connection up
 * @param[in]       privacy_mode    - privacy mode (see #wiced_bt_ble_privacy_mode_t)
 *
 * @return          wiced_bt_dev_status_t \n
 * <b> WICED_BT_ILLEGAL_VALUE </b>  : if paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>    : if command not supported \n
 * <b> WICED_BT_UNKNOWN_ADDR </b>   : if bd_addr is wrong \n
 * <b> WICED_BT_ILLEGAL_ACTION </b> : if device not added to resolving list or peer irk is not valid \n
 * <b> WICED_BT_ERROR </b>      : error while processing the command \n
 * <b> WICED_BT_SUCCESS</b>     : if command started
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_privacy_mode(wiced_bt_device_address_t remote_bda,
                                                    wiced_bt_ble_address_type_t rem_bda_type,
                                                    wiced_bt_ble_privacy_mode_t privacy_mode);

/**
 * Get the configured local random device address.
 *
 * Note : random address depends on below settings in that priority order.
 *      1) Global privacy configuration using rpa_refresh_timeout (see #wiced_bt_cfg_settings_t).
 *      2) else configured for static random bd_address while downloading using BT_DEVICE_ADDRESS=random build setting.
 *
 * @param[out]       random_bd_addr     - device random bd address
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_SUCCESS </b> : if random address is configured.\n
 * <b> WICED_BT_WRONG_MODE </b> : if random address not configured.\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_read_device_random_address(wiced_bt_device_address_t random_bd_addr);

/**
 * Check if the local BT controller supports extended advertising
 *
 * @return          wiced_bool_t
 *
 */
wiced_bool_t wiced_bt_ble_is_ext_adv_supported(void);

/**
 * Check if the local BT controller supports periodic advertising
 *
 * @return          wiced_bool_t
 *
 */
wiced_bool_t wiced_bt_ble_is_periodic_adv_supported(void);

/**
 * Sends HCI command to set the random address for an adv set
 *
 * @param[in]       adv_handle  - handle of the advertising set
 * @param[in]       random_addr - random address to use for this set
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_SUCCESS </b>       : If random addr is set successfully\n
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_ext_adv_random_address(wiced_bt_ble_ext_adv_handle_t adv_handle,
                                                              wiced_bt_device_address_t random_addr);

/**
 * Sends the HCI command to set the parameters for extended advetisement
 *
 * @param[in]        adv_handle                  Advertisement set handle
 * @param[in]        event_properties            Bit mask to speicify connectable,scannable,low duty,high duty,directed,legacy adv
 * @param[in]        primary_adv_int_min         Range: 0x000020 to 0xFFFFFF (20 ms to 10,485.759375 s)
 * @param[in]        primary_adv_int_max         Range: 0x000020 to 0xFFFFFF(20 ms to 10,485.759375 s)
 * @param[in]        primary_adv_channel_map     BLE advertisement channel map (see #wiced_bt_ble_advert_chnl_map_e)
 * @param[in]        own_addr_type               Ignored in case of anonymous adv. See event_properties
 * @param[in]        peer_addr_type              Peer address type
 * @param[in]        peer_addr                   peer address
 * @param[in]        adv_filter_policy           Adv filter policy
 * @param[in]        adv_tx_power                -127 to +126. 127 means host has no preference
 * @param[in]        primary_adv_phy             Phy used to transmit ADV packets on Primary ADV channels
 * @param[in]        secondary_adv_max_skip      Valid only in case of extended ADV. Range 0 to FF.
                                                                                         Maximum advertising events controller can skip before sending
                                                                                         auxiliary adv packets on the secondary adv channel
 * @param[in]        secondary_adv_phy           Phy used to transmit ADV packets on secondary ADV channels. Valid only in case of extended ADV
 * @param[in]        adv_sid                     Advertisement set identifier is the value to be transmitted in extended ADV PDUs

 * @param[in]        scan_request_not            scan request received notification enable/disable

 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_SUCCESS </b>       : If all extended adv params are set successfully\n
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_ext_adv_parameters(
    wiced_bt_ble_ext_adv_handle_t adv_handle,
    wiced_bt_ble_ext_adv_event_property_t event_properties,
    uint32_t primary_adv_int_min,
    uint32_t primary_adv_int_max,
    wiced_bt_ble_advert_chnl_map_t primary_adv_channel_map,
    wiced_bt_ble_address_type_t own_addr_type,
    wiced_bt_ble_address_type_t peer_addr_type,
    wiced_bt_device_address_t peer_addr,
    wiced_bt_ble_advert_filter_policy_t adv_filter_policy,
    int8_t adv_tx_power,
    wiced_bt_ble_ext_adv_phy_t primary_adv_phy,
    uint8_t secondary_adv_max_skip,
    wiced_bt_ble_ext_adv_phy_t secondary_adv_phy,
    wiced_bt_ble_ext_adv_sid_t adv_sid,
    wiced_bt_ble_ext_adv_scan_req_notification_setting_t scan_request_not);

/**
 * Sends the HCI command to set the parameters for extended advetisement
 *
 * @param[in]        adv_handle Advertisement set handle
 * @param[in]        p_params   Extended adv parameters

 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_SUCCESS </b>       : If all extended adv params are set successfully\n
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_ext_adv_parameters_v2(wiced_bt_ble_ext_adv_handle_t adv_handle,
                                                             wiced_bt_ble_ext_adv_params_t *p_params);


/**
 * Sends HCI command to write the extended adv data
 * @note This API allows sending data formatted with \ref wiced_bt_ble_build_raw_advertisement_data.
 * @note This API cannot be used for the advertising handle with the event_properties that doesn't support advertising data;
 * viz., WICED_BT_BLE_EXT_ADV_EVENT_DIRECTED_ADV, WICED_BT_BLE_EXT_ADV_EVENT_HIGH_DUTY_DIRECTED_CONNECTABLE_ADV
 *
 * @param[in]       adv_handle  - handle of the advertising set
 * @param[in]       data_len    - length of the adv data to use for this set
 * @param[in]       p_data      - pointer to the adv data to use for this set
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_SUCCESS </b>       : If all extended adv data set successfully\n
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_ext_adv_data(wiced_bt_ble_ext_adv_handle_t adv_handle,
    uint16_t data_len,
    uint8_t *p_data);

/**
 * Sends HCI command to write the legacy adv data
 * @note This API allows sending data formatted with \ref wiced_bt_ble_build_raw_advertisement_data.
 *
 * @param[in]       data_len - length of the adv data to use, max size 31 bytes
 * @param[in]       p_data   - pointer to the adv data to use
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_SUCCESS </b>       : If all extended adv data set successfully\n
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_legacy_adv_data(uint16_t data_len, uint8_t *p_data);

/**
 * Sends HCI command to write the extended scan rsp data
 *
 * @param[in]       adv_handle  - handle of the advertising set
 * @param[in]       data_len    - length of the scan response data to use for this set
 * @param[in]       p_data      - pointer to the scan response data to use for this set
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_SUCCESS </b>       : If all extended scan response data set successfully\n
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_ext_scan_rsp_data(wiced_bt_ble_ext_adv_handle_t adv_handle,
                                                         uint16_t data_len,
                                                         uint8_t *p_data);

/**
 * Sends the HCI command to start/stop extended advertisements
 *
 * @param[in]       enable    - true to enable, false to disable
 * @param[in]       num_sets  - number of sets to enable, unused if disabling
 * @param[in]       p_dur     - pointer to adv handle(s) and duration configuration
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_start_ext_adv(uint8_t enable,
                                                 uint8_t num_sets,
                                                 wiced_bt_ble_ext_adv_duration_config_t *p_dur);

/**
 * Sends the HCI command to remove an extended advertisement set (which is currently not advertising)
 *
 * @param[in]       adv_handle    - handle to advertisement set
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_remove_adv_set(wiced_bt_ble_ext_adv_handle_t adv_handle);

/**
 * Sends the HCI command to remove all extended advertisement sets which are currently not advertising
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successfuly\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_clear_adv_sets(void);

/**
 * Read the number of ADV sets supported by the controller.
 *
 * @return          uint8_t
 *
 */
uint8_t wiced_bt_ble_read_num_ext_adv_sets(void);

/**
 * Read the maximum ADV data length supported by the controller.
 *
 * @return          uint16_t
 *
 */
uint16_t wiced_bt_ble_read_max_ext_adv_data_len(void);

/**
 * Sends the HCI command to set the parameters for periodic advertising
 *
 * @param[in]       adv_handle            advertisement set handle
 * @param[in]       periodic_adv_int_min  Range N: 0x0006 to 0xFFFF, Time = N * 1.25 ms
 * @param[in]       periodic_adv_int_max  Range N: 0x0006 to 0xFFFF, Time = N * 1.25 ms
 * @param[in]       periodic_adv_properties   periodic adv property indicates which field should be include in periodic adv
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_periodic_adv_params(wiced_bt_ble_ext_adv_handle_t adv_handle,
                                                           uint16_t periodic_adv_int_min,
                                                           uint16_t periodic_adv_int_max,
                                                           wiced_bt_ble_periodic_adv_prop_t periodic_adv_properties);

/**
 * Sends the HCI command to write the periodic adv data
 *
 * @param[in]    adv_handle        advertisement set handle
 * @param[in]    adv_data_length   periodic data length
 * @param[in]    p_adv_data        pointer to the periodic data
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_periodic_adv_data(wiced_bt_ble_ext_adv_handle_t adv_handle,
                                                         uint16_t adv_data_length,
                                                         uint8_t *p_adv_data);

/**
 * Sends the HCI command to start/stop periodic advertisements

 * @param[in]       adv_handle  - handle of the advertising set
 * @param[in]       enable      - true to enable, false to disable
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_start_periodic_adv(wiced_bt_ble_ext_adv_handle_t adv_handle, wiced_bool_t enable);

/**
 * Stores extended scan configuration to apply on start ext scan
 *
 * @param[in]       p_ext_scan_cfg    - pointer to scan configuration
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_cache_ext_scan_config(wiced_bt_ble_ext_scan_config_t *p_ext_scan_cfg);

/**
 * Stores the extended ADV connection configuration.
 *
 * @param[in]       p_ext_conn_cfg    - pointer to connection configuration
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_cache_ext_conn_config(wiced_bt_ble_ext_conn_cfg_t *p_ext_conn_cfg);

/**
 * Sends the HCI command to synchronize with periodic advertising from an advertiser and begin receiving periodic
 * advertising packets.
 *
 * @param[in]       options         - ref: wiced_bt_ble_adv_sync_options_t
 * @param[in]       adv_sid         - min SID / max SID
 * @param[in]       adv_addr_type   - address type
 * @param[in]       adv_addr        - address value
 * @param[in]       skip
 * @param[in]       sync_timeout    - timeout value
 * @param[in]       sync_cte_type   - bit 0 - Do not sync to packets with an AoA Constant Tone Extension
 *                                        1 - Do not sync to packets with an AoD Constant Tone Extension with 1 μs slots
 *                                        2 - Do not sync to packets with an AoD Constant Tone Extension with 2 μs slots
 *                                        3 - Do not sync to packets with a type 3 Constant Tone Extension
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_create_sync_to_periodic_adv(wiced_bt_ble_adv_sync_options_t options,
                                                               wiced_bt_ble_ext_adv_sid_t adv_sid,
                                                               wiced_bt_ble_address_type_t adv_addr_type,
                                                               wiced_bt_device_address_t adv_addr,
                                                               uint16_t skip,
                                                               uint16_t sync_timeout,
                                                               uint8_t sync_cte_type);

/**
 * Sends HCI command to cancel the create sync command while it is pending.
 *
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_cancel_sync_to_periodic_adv(void);

/**
 * Sends the HCI command to stop reception of periodic advertising identified by the sync_handle
 *
 * @param[in]       sync_handle      - Sync handle received in WICED_BT_BLE_PERIODIC_ADV_SYNC_ESTABLISHED_EVENT.
 *
 * @return          wiced_bt_dev_status_t
 *
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 * <b> WICED_BT_SUCCESS </b>       : If successful \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_terminate_sync_to_periodic_adv(uint16_t sync_handle);

/**
 * Sends the HCI command to add the given advertiser to Periodic Advertiser list.
 *
 * Note : Caller shall not attempt to add more than max list size
 *       Shall not attempt to call this API, while create to periodic sync command is pending.
 *
 * @param[in]      advertiser_addr_type : Periodic advertiser addr type
 * @param[in]      advetiser_addr       : Periodic advertiser addr
 * @param[in]      adv_sid              : Periodic advertiser sid
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_add_device_to_periodic_adv_list(wiced_bt_ble_address_type_t advertiser_addr_type,
                                                                   wiced_bt_device_address_t advetiser_addr,
                                                                   wiced_bt_ble_ext_adv_sid_t adv_sid);

/**
 * Sends the HCI command to remove the given advertiser from Periodic Advertiser list.
 *
 * Note : Shall not attempt to call this API, while create to periodic sync command is pending.
 *
 * @param[in]      advertiser_addr_type : Periodic advertiser addr type
 * @param[in]      advetiser_addr       : Periodic advertiser addr
 * @param[in]      adv_sid              : Periodic advertiser sid
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_ILLEGAL_VALUE </b> : If paramer is wrong \n
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_remove_device_from_periodic_adv_list(
    wiced_bt_ble_address_type_t advertiser_addr_type,
    wiced_bt_device_address_t advetiser_addr,
    wiced_bt_ble_ext_adv_sid_t adv_sid);

/**
 * Sends the HCI command to remove to remove all devices from the the Periodic Advertisers list.
 *
 * Note : Shall not attempt to call this API, while create to periodic sync command is pending.
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_clear_periodic_adv_list(void);

/**
 * Read the Periodic Advertisers list size.
 *
 * @return          uint8_t : list size
 *
 */
uint8_t wiced_bt_ble_read_periodic_adv_list_size(void);

/**
 * Register an application callback function to receive extended advertising events.
 *
 * @param[in]       p_app_adv_ext_event_cb      - pointer to function to receive extended adv events.
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_PENDING </b>       : If command queued to send down \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
void wiced_bt_ble_register_adv_ext_cback(wiced_bt_ble_adv_ext_event_cb_fp_t p_app_adv_ext_event_cb);

/**
 * Sends the HCI command enable or disable receiving periodic ADV data for a sync handle.
 *
 * @param[in]       sync_handle : Sync handle
 * @param[in]       enable      : Boolean for enable/disable.
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNSUPPORTED </b>   : If command not supported \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_periodic_adv_rcv_enable(wiced_bt_ble_periodic_adv_sync_handle_t sync_handle, wiced_bool_t enable);

/**
 * Sends the HCI command to send synchronization information about the periodic advertising train identified by the Sync_Handle parameter to given device
 *
 * @param[in]       peer_bda        - Peer Bluetooth Address
 * @param[in]       service_data    - Service Data value
 * @param[in]       sync_handle : Sync handle
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNKNOWN_ADDR </b> :  If Unknown remote BD address \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_periodic_adv_sync_transfer(wiced_bt_device_address_t peer_bda,
                                                              uint16_t service_data,
                                                              wiced_bt_ble_periodic_adv_sync_handle_t sync_handle);

/**
 * Sends the HCI command  to send synchronization information about the periodic advertising in an advertising set to given device.
 *
 * @param[in]       peer_bda        - Peer Bluetooth Address
 * @param[in]       service_data    - Service Data value
 * @param[in]       adv_handle  - handle of the advertising set
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNKNOWN_ADDR </b> :  If Unknown remote BD address \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If command queued to send down \n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_periodic_adv_set_info_transfer(wiced_bt_device_address_t peer_bda,
                                                                  uint16_t service_data,
                                                                  wiced_bt_ble_ext_adv_handle_t adv_handle);

/**
 * Sends the HCI command to set synchronize periodic transfer parameter
 *
 * @param[in]       peer_bda        - Peer Bluetooth Address
 * @param[in]       mode            - ref: wiced_bt_ble_periodic_adv_sync_transfer_mode_t
 * @param[in]       skip            -  The number of periodic advertising packets that can be skipped after a successful receive
 * @param[in]       sync_timeout    - timeout value
 * @param[in]       sync_cte_type   - bit 0 - Do not sync to packets with an AoA Constant Tone Extension
 *                                        1 - Do not sync to packets with an AoD Constant Tone Extension with 1 μs slots
 *                                        2 - Do not sync to packets with an AoD Constant Tone Extension with 2 μs slots
 *                                        3 - Do not sync to packets with a type 3 Constant Tone Extension
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_UNKNOWN_ADDR </b> :  If Unknown remote BD address \n
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_periodic_adv_sync_transfer_param(
    wiced_bt_device_address_t peer_bda,
    wiced_bt_ble_periodic_adv_sync_transfer_mode_t mode,
    uint16_t skip,
    uint16_t sync_timeout,
    uint8_t sync_cte_type);

/**
 * Sends the HCI command to set Default synchronize periodic transfer parameter
 *
 * @param[in]       mode         - ref: wiced_bt_ble_periodic_adv_sync_transfer_mode_t
 * @param[in]       skip            -  The number of periodic advertising packets that can be skipped after a successful receive
 * @param[in]       sync_timeout    - timeout value
 * @param[in]       sync_cte_type   - bit 0 - Do not sync to packets with an AoA Constant Tone Extension
 *                                        1 - Do not sync to packets with an AoD Constant Tone Extension with 1 μs slots
 *                                        2 - Do not sync to packets with an AoD Constant Tone Extension with 2 μs slots
 *                                        3 - Do not sync to packets with a type 3 Constant Tone Extension
 *
 * @return          wiced_bt_dev_status_t
 * <b> WICED_BT_NO_RESOURCES </b>  : If no memory to issue the command \n
 * <b> WICED_BT_SUCCESS </b>       : If successful\n
 *
 */
wiced_bt_dev_status_t wiced_bt_ble_set_default_periodic_adv_sync_transfer_param(
    wiced_bt_ble_periodic_adv_sync_transfer_mode_t mode,
    uint16_t skip,
    uint16_t sync_timeout,
    uint8_t sync_cte_type);


/**
 * Function         wiced_ble_private_device_address_resolution
 *
 *                  This API verifies whether given device address is Resolvable Private Address or not
 *
 * @param rpa       BLE Resolvable Private Address
 * @param irk       BLE IRK
 * @return          wiced_result_t
 *                  WICED_BT_SUCCESS the identity of device address has been resolved.
 *                  WICED_BT_ERROR   otherwise.
 */
wiced_result_t wiced_ble_private_device_address_resolution(wiced_bt_device_address_t rpa, BT_OCTET16 irk);

/**
 * Function         wiced_bt_ble_read_le_features
 *
 *                  This API returns the features supported by the \p bda
 *
 * @param[in]  bda       Device address pointer, pass NULL for local device
 * @param[out] features  Pointer to store the supported features
 * @return          wiced_result_t
 *                  WICED_BT_SUCCESS contents of features are valid
 *                  WICED_BT_ERROR   otherwise.
 */
wiced_result_t wiced_bt_ble_read_le_features(wiced_bt_device_address_t bda, wiced_bt_features_t features);

/**
 * Function         wiced_bt_ble_address_resolution_list_clear_and_disable
 *
 *                  This API clears the address resolution list and disables the address resolution feature.
 *
 * @return          wiced_result_t
 *                  WICED_BT_SUCCESS if address resolution list is cleared and adress resolution feature is disabled.
 *                  WICED_BT_ERROR   otherwise.
 */
wiced_result_t wiced_bt_ble_address_resolution_list_clear_and_disable(void);

/* @cond BETA_API
   beta APIs for Periodic Advertising with Response*/
/**
* Function         wiced_bt_ble_set_pawr_params
*
*                  This API is called on a central to set the PAWR parameters
*
* @param[in]  adv_handle    Handle of the Advertising Set
* @param[out] features  Pointer to store the supported features
* @return          wiced_result_t
*                  WICED_BT_SUCCESS contents of features are valid
*                  WICED_BT_ERROR   otherwise.
*/
wiced_bt_dev_status_t wiced_bt_ble_set_pawr_params (wiced_bt_ble_ext_adv_handle_t adv_handle,
                            uint16_t                         periodic_adv_int_min,
                            uint16_t                         periodic_adv_int_max,
                            wiced_bt_ble_periodic_adv_prop_t periodic_adv_properties,
                            uint8_t                          num_subevents,
                            uint8_t                          subevent_interval,
                            uint8_t                          response_slot_delay,
                            uint8_t                          response_slot_spacing,
                            uint8_t                          num_response_slots);

/**
* Function         wiced_bt_ble_set_pawr_subevent_ind_data
*
*                  This API is called on a peripheral to set the subevent indication data
*
* @param[in]  adv_handle    Handle of the Advertising Set
* @param[out] features  Pointer to store the supported features
* @return          wiced_result_t
*                  WICED_BT_SUCCESS contents of features are valid
*                  WICED_BT_ERROR   otherwise.
*/
wiced_bt_dev_status_t wiced_bt_ble_set_pawr_subevent_ind_data (wiced_bt_ble_ext_adv_handle_t adv_handle,
    int  num_subevents, wiced_bt_ble_pawr_subevent_ind_data_t *p_se_data);


/**
* Function         wiced_bt_ble_set_pawr_subevent_rsp_data
*
*                  This API is called on a peripheral to set the subevent response data
*
* @param[in]  sync_handle    Handle of the synchronized advertising train
* @param[out] features  Pointer to store the supported features
* @return          wiced_result_t
*                  WICED_BT_SUCCESS contents of features are valid
*                  WICED_BT_ERROR   otherwise.
*/
wiced_bt_dev_status_t wiced_bt_ble_set_pawr_subevent_rsp_data (uint16_t sync_handle,
    uint8_t subevent_num, uint8_t rsp_slot, uint8_t rsp_data_len, uint8_t *p_data);


/**
* Function         wiced_bt_ble_pawr_sync_subevents
*
*                  This API is called on a peripheral to set the PAWR sunevents
*                  that it wants to sync to.
*
* @param[in]  sync_handle     Handle of the synchronized periodic ADV train
* @param[in]  properties      Properties of the synchronized periodic ADV train
* @param[in]  num_subevents   Number of subevents
* @param[in]  p_subevents     Pointer to an array of subevents
*
* @return          wiced_result_t
*                  WICED_BT_SUCCESS contents of features are valid
*                  WICED_BT_ERROR   otherwise.
*/
wiced_bt_dev_status_t wiced_bt_ble_set_pawr_sync_subevents (uint16_t sync_handle, uint16_t properties,
                                int num_subevents, uint8_t *p_subevents);

/* @endcond */


/**
* Function wiced_bt_ble_set_host_features
*
*          This API is called to set the supported host features
*
* @param[in]  feature   bit position of required feature
* @param[in]  bit_value Value to enable or disable Advertising Coding Selection Host Support feature bit
*
* @return          wiced_result_t
*                  WICED_BT_SUCCESS Advertising Coding Selection Host Support feature bit is modified
*                  WICED_BT_ERROR   otherwise.
*/
wiced_bt_dev_status_t wiced_bt_ble_set_host_features(wiced_bt_ble_feature_bit_t feature, uint8_t bit_value);



/**@} btm_ble_api_functions */

#ifdef __cplusplus
}
#endif
