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
 * Runtime Bluetooth configuration parameters
 *
 */
#pragma once

#include "wiced_data_types.h"
#include "wiced_bt_types.h"
#include "gattdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup  wiced_bt_cfg Bluetooth Stack Initialize & Configuration
 *
 *
 * This section describes API and Data structures required to initialize and configure the BT-Stack.
 *
 * @{
 */
/*****************************************************************************
 * Default configuration values
 ****************************************************************************/
 /**  */
/**
 * @anchor WICED_DEFAULT_CFG_VALUES
 * @name Bluetooth Configuration Default Values
 * @{
 *
 * Bluetooth Configuration Default Values
 *
 * @note These are typical values for config parameters used for some common BLE, BR/EDR use cases.
 */
#define WICED_BT_CFG_DEFAULT_INQUIRY_SCAN_INTERVAL                  0x0800      /**< Inquiry scan interval (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_INQUIRY_SCAN_WINDOW                    0x0012      /**< Inquiry scan window (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_PAGE_SCAN_INTERVAL                     0x0800      /**< Page scan interval (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_PAGE_SCAN_WINDOW                       0x0012      /**< Page scan window (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_INTERVAL                96          /**< High duty scan interval (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_WINDOW                  48          /**< High duty scan window (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_INTERVAL                 2048        /**< Low duty scan interval (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_WINDOW                   18          /**< Low duty scan window (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_INTERVAL           96          /**< High duty cycle connection scan interval (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_WINDOW             48          /**< High duty cycle connection scan window (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_INTERVAL            2048        /**< Low duty cycle connection scan interval (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_WINDOW              18          /**< Low duty cycle connection scan window (in slots (1 slot = 0.625 ms)) */
#define WICED_BT_CFG_DEFAULT_CONN_MIN_INTERVAL                      80          /**< Minimum connection event interval ( in 1.25 msec) */
#define WICED_BT_CFG_DEFAULT_CONN_MAX_INTERVAL                      80          /**< Maximum connection event interval ( in 1.25 msec) */
#define WICED_BT_CFG_DEFAULT_CONN_LATENCY                           0           /**< Connection latency (in number of LL connection events) */
#define WICED_BT_CFG_DEFAULT_CONN_SUPERVISION_TIMEOUT               700         /**< Connection link supervision timeout (in 10 msec) */

/* undirected connectable advertisement high/low duty cycle interval default */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MIN_INTERVAL             48          /**< Tgap(adv_fast_interval1) = 48 *0.625  = 30ms*/
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MAX_INTERVAL             48          /**< Tgap(adv_fast_interval1) = 48 *0.625  = 30ms*/
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MIN_INTERVAL              2048        /**< Tgap(adv_slow_interval) = 2048 * 0.625 = 1.28s */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MAX_INTERVAL              2048        /**< Tgap(adv_slow_interval) = 2048 * 0.625 = 1.28s */

/* non-connectable advertisement high/low duty cycle advertisement interval default */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_NONCONN_ADV_MIN_INTERVAL     160         /**< Tgap(adv_fast_interval2) = 160 * 0.625 = 100 ms */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_NONCONN_ADV_MAX_INTERVAL     160         /**< Tgap(adv_fast_interval2) = 160 * 0.625 = 100 ms */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_NONCONN_ADV_MIN_INTERVAL      2048        /**< Tgap(adv_slow_interval) = 2048 * 0.625 = 1.28s */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_NONCONN_ADV_MAX_INTERVAL      2048        /**< Tgap(adv_slow_interval) = 2048 * 0.625 = 1.28s */

/* directed connectable advertisement high/low duty cycle interval default */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_DIRECTED_ADV_MIN_INTERVAL    400        /**< Tgap(dir_conn_adv_int_max) = 400 * 0.625 = 250 ms */
#define WICED_BT_CFG_DEFAULT_HIGH_DUTY_DIRECTED_ADV_MAX_INTERVAL    800        /**< Tgap(dir_conn_adv_int_min) = 800 * 0.625 = 500 ms */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_DIRECTED_ADV_MIN_INTERVAL     48         /**< Tgap(adv_fast_interval1) = 48 * 0.625 = 30 ms */
#define WICED_BT_CFG_DEFAULT_LOW_DUTY_DIRECTED_ADV_MAX_INTERVAL     48         /**< Tgap(adv_fast_interval1) = 48 * 0.625 = 30 ms */

/* refreshment timing interval of random private address */
#define WICED_BT_CFG_DEFAULT_RANDOM_ADDRESS_CHANGE_TIMEOUT          900        /**< default refreshment timing interval 900secs */
#define WICED_BT_CFG_DEFAULT_RANDOM_ADDRESS_NEVER_CHANGE            0          /**< value for disabling random address refresh */
#define WICED_BT_CFG_MAX_RANDOM_ADDRESS_CHANGE_TIMEOUT              3600       /**< max refreshment timing interval 3600secs*/

/** @} WICED_DEFAULT_CFG_VALUES */

/*****************************************************************************
 * Wiced_bt core stack configuration
 ****************************************************************************/
/** Scan modes */
enum wiced_bt_ble_scan_mode_e
{
    BTM_BLE_SCAN_MODE_PASSIVE = 0,  /**< Passive scan mode */
    BTM_BLE_SCAN_MODE_ACTIVE = 1,   /**< Active scan mode */
    BTM_BLE_SCAN_MODE_NONE = 0xff   /**< None */
};
typedef uint8_t wiced_bt_ble_scan_mode_t;   /**< scan mode (see #wiced_bt_ble_scan_mode_e) */

/** Security Service Levels (bit fields) */
enum wiced_bt_sec_level_e
{
    /** BTM_SEC_BEST_EFFORT : Recommended choice for most applications, to connect to the widest range of devices.
      * Allows stack to choose the highest level of security possible between the two devices */
    BTM_SEC_BEST_EFFORT = 1,

    /** BTM_SEC_SC_REQUIRED : Can be set by applications which need to enforce secure connections.
      * Note: If this bit is set, the stack will only allow connections to devices paired using Secure Connections */
    BTM_SEC_SC_REQUIRED = 2,

    /** BTM_SEC_AUTH_REQUIRED - Can be set by applications which need to enforce Authentication
      * Note: If this bit is set, the stack will only allow connections to devices paired using authentication */
    BTM_SEC_AUTH_REQUIRED = 4,

    /** BTM_SEC_SC_AUTH_REQUIRED : Can be set by applications which need to enforce secure connections with MITM protection.
     * Note: If this bit is set, the stack will only allow connections to devices paired using Secure Connections with Man In The Middle (MITM) protection */
    BTM_SEC_SC_AUTH_REQUIRED = (BTM_SEC_SC_REQUIRED | 4),
};
typedef uint8_t wiced_bt_sec_level_t; /**< Required security level */

/** LE Scan settings */
typedef struct
{
    wiced_bt_ble_scan_mode_t            scan_mode;                          /**< BLE scan mode \ref wiced_bt_ble_scan_mode_t */

    /* Advertisement scan configuration
    * @note Refer to 7.8.10 LE Set Scan Parameters command
    */
    uint16_t                            high_duty_scan_interval;            /**< High duty scan interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_INTERVAL) */
    uint16_t                            high_duty_scan_window;              /**< High duty scan window (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_WINDOW) */
    uint16_t                            high_duty_scan_duration;            /**< High duty scan duration in seconds (0 for infinite) */

    uint16_t                            low_duty_scan_interval;             /**< Low duty scan interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_INTERVAL) */
    uint16_t                            low_duty_scan_window;               /**< Low duty scan window (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_WINDOW) */
    uint16_t                            low_duty_scan_duration;             /**< Low duty scan duration in seconds (0 for infinite) */

    /* Connection scan configuration
    * @note Refer to 7.8.10 LE Set Scan Parameters command
    */
    uint16_t                            high_duty_conn_scan_interval;       /**< High duty cycle connection scan interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_INTERVAL) */
    uint16_t                            high_duty_conn_scan_window;         /**< High duty cycle connection scan window (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_WINDOW) */
    uint16_t                            high_duty_conn_duration;            /**< High duty cycle connection duration in seconds (0 for infinite) */

    uint16_t                            low_duty_conn_scan_interval;        /**< Low duty cycle connection scan interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_INTERVAL) */
    uint16_t                            low_duty_conn_scan_window;          /**< Low duty cycle connection scan window (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_WINDOW) */
    uint16_t                            low_duty_conn_duration;             /**< Low duty cycle connection duration in seconds (0 for infinite) */

    /* Connection configuration
    * @note: Refer to description of valid values in the BT SIG Spec Ver 5.2, section 7.8.12 LE Create Connection command
    */
    uint16_t                            conn_min_interval;                  /**< Minimum connection interval (in 1.25 msec) (default: #WICED_BT_CFG_DEFAULT_CONN_MIN_INTERVAL) */
    uint16_t                            conn_max_interval;                  /**< Maximum connection interval (in 1.25 msec) (default: #WICED_BT_CFG_DEFAULT_CONN_MAX_INTERVAL) */
    uint16_t                            conn_latency;                       /**< Connection latency */
    uint16_t                            conn_supervision_timeout;           /**< Connection link supervision timeout (in 10 msec) (default: #WICED_BT_CFG_DEFAULT_CONN_SUPERVISION_TIMEOUT) */
} wiced_bt_cfg_ble_scan_settings_t;

/** advertising channel map */
enum wiced_bt_ble_advert_chnl_map_e
{
    BTM_BLE_ADVERT_CHNL_37 = (0x01 << 0),  /**< ADV channel */
    BTM_BLE_ADVERT_CHNL_38 = (0x01 << 1),  /**< ADV channel */
    BTM_BLE_ADVERT_CHNL_39 = (0x01 << 2)   /**< ADV channel */
};
typedef uint8_t wiced_bt_ble_advert_chnl_map_t;  /**< BLE advertisement channel map (see #wiced_bt_ble_advert_chnl_map_e) */

/** Advertising settings */
/**
 * @note: Refer to valid ranges for the min and max intervals in the Core Specification 5.2, 7.8.5 LE Set Advertising Parameters command of the
 * Host Controller Interface Functional Specification
 */
typedef struct
{
    wiced_bt_ble_advert_chnl_map_t      channel_map;   /**< Advertising channel map (mask of #BTM_BLE_ADVERT_CHNL_37, #BTM_BLE_ADVERT_CHNL_38, #BTM_BLE_ADVERT_CHNL_39) */

    uint16_t high_duty_min_interval;             /**< High duty undirected connectable advert minimum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MIN_INTERVAL) */
    uint16_t high_duty_max_interval;             /**< High duty undirected connectable advert maximum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MAX_INTERVAL) */
    uint16_t high_duty_duration;                 /**< High duty advertising duration in seconds (0 for infinite) */

    uint16_t low_duty_min_interval;              /**< Low duty undirected connectable advert minimum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MIN_INTERVAL) */
    uint16_t low_duty_max_interval;              /**< Low duty undirected connectable advert maximum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MAX_INTERVAL) */
    uint16_t low_duty_duration;                  /**< Low duty advertising duration in seconds (0 for infinite) */

    uint16_t high_duty_directed_min_interval;    /**< high duty directed adv minimum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_DIRECTED_ADV_MIN_INTERVAL) */
    uint16_t high_duty_directed_max_interval;    /**< high duty directed adv maximum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_DIRECTED_ADV_MAX_INTERVAL) */

    uint16_t low_duty_directed_min_interval;     /**< Low duty directed adv minimum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_DIRECTED_ADV_MIN_INTERVAL) */
    uint16_t low_duty_directed_max_interval;     /**< Low duty directed adv maximum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_DIRECTED_ADV_MAX_INTERVAL) */
    uint16_t low_duty_directed_duration;         /**< Low duty directed advertising duration in seconds (0 for infinite) */

    uint16_t high_duty_nonconn_min_interval;     /**< High duty non-connectable adv minimum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_NONCONN_ADV_MIN_INTERVAL) */
    uint16_t high_duty_nonconn_max_interval;     /**< High duty non-connectable adv maximum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_HIGH_DUTY_NONCONN_ADV_MAX_INTERVAL) */
    uint16_t high_duty_nonconn_duration;         /**< High duty non-connectable advertising duration in seconds (0 for infinite) */

    uint16_t low_duty_nonconn_min_interval;      /**< Low duty non-connectable adv minimum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_NONCONN_ADV_MIN_INTERVAL) */
    uint16_t low_duty_nonconn_max_interval;      /**< Low duty non-connectable adv maximum advertising interval (in slots (1 slot = 0.625 ms)) (default: #WICED_BT_CFG_DEFAULT_LOW_DUTY_NONCONN_ADV_MAX_INTERVAL) */
    uint16_t low_duty_nonconn_duration;          /**< Low duty non-connectable advertising duration in seconds (0 for infinite) */

} wiced_bt_cfg_ble_advert_settings_t;

typedef uint16_t wiced_bt_gatt_appearance_t;     /**< GATT appearance (see gatt_appearance_e in gattdefs.h) */

/** GATT settings */
typedef struct
{
    uint8_t  max_db_service_modules;  /**< Maximum number of GATT database segments to be added into the GATT DB
                                      * @note: Should be set to 0 for most applications which do not update the GATT DB after invocation of
                                      * \ref wiced_bt_gatt_db_init
                                      */
    uint8_t  max_eatt_bearers;        /**< Maximum number of allowed EATT bearers
                                      * @note: Should be set to 0, for applications which support only legacy bearers.
                                      */
} wiced_bt_cfg_gatt_t;

/** Audio/Video Distribution configuration */
typedef struct
{
    uint8_t  max_links; /**< Maximum simultaneous audio/video links over AVDT
                        @note: This shall be <= \ref wiced_bt_cfg_br_t.br_max_simultaneous_links;
                        */
    uint8_t  max_seps;  /**< Maximum number of stream end points */
} wiced_bt_cfg_avdt_t;

/** Audio/Video Remote Control configuration */
typedef struct
{
    uint8_t max_links;  /**< Maximum simultaneous remote control links over AVRC
                        @note: This shall be <= \ref wiced_bt_cfg_br_t.br_max_simultaneous_links;
                        */
} wiced_bt_cfg_avrc_t;


/** RFCOMM configuration */
typedef struct
{
    uint8_t  max_links;  /**< Maximum number of simultaneous connected remote devices over RFCOMM
                         @note: This shall be <= \ref wiced_bt_cfg_br_t.br_max_simultaneous_links;
                         */
    uint8_t  max_ports;  /**< Maximum number of simultaneous RFCOMM ports */
} wiced_bt_cfg_rfcomm_t;

/** Ischoronous Connection configuration settings */
typedef struct
{
    uint16_t max_sdu_size;       /**< Max SDU size */
    uint8_t channel_count;       /**< maximum number of audio channels per packet (left, right, etc.,) */
    uint8_t max_cis_conn;        /**< Max Number of CIS connections */
    uint8_t max_cig_count;       /**< Max Number of CIG connections */
    uint8_t max_buffers_per_cis; /**< Max Number of buffers per CIS */
    uint8_t max_big_count;       /**< Max Number of BIG connections */
} wiced_bt_cfg_isoc_t;

/** BR/EDR configuration settings */
typedef struct {
    uint8_t                br_max_simultaneous_links; /**< Max number for simultaneous connections for a layer, profile, protocol */
    uint16_t               br_max_rx_pdu_size;        /**< Maximum size allowed for any received L2CAP PDU
                                                      * Minimum value - 48
                                                      * Maximum GATT MTU over legacy bearers shall be set to <= this value
                                                      * Maximum MPS for EATT channels shall be set to <= this value
                                                      */
    wiced_bt_dev_class_t   device_class;              /**< Local device class */
    wiced_bt_cfg_rfcomm_t  rfcomm_cfg;                /**< RFCOMM settings */
    wiced_bt_cfg_avdt_t    avdt_cfg;                  /**< Audio/Video Distribution configuration */
    wiced_bt_cfg_avrc_t    avrc_cfg;                  /**< Audio/Video Remote Control configuration */
}wiced_bt_cfg_br_t;

/** BLE configuration settings */
typedef struct {
    uint8_t    ble_max_simultaneous_links;   /**< Max number for simultaneous connections for a layer, profile, protocol */
    uint16_t   ble_max_rx_pdu_size;          /**< Maximum size allowed for any received L2CAP PDU
                                             * Minimum value - 65 (to support SM)
                                             * Maximum GATT MTU over legacy bearers shall be set to <= this value
                                             * Maximum MPS for EATT channels shall be set to <= this value
                                             */
    wiced_bt_gatt_appearance_t appearance;   /**< Device appearance to be sent out during advertising */

    uint16_t   rpa_refresh_timeout;          /**< Interval of random address refreshing - secs. The timeout value cannot be more than 1 hr = 3600s
                                              *
                                              * @note BLE Privacy is disabled if the value is 0.
                                              */
    uint16_t   host_addr_resolution_db_size; /**< addr resolution db size */

    const wiced_bt_cfg_ble_scan_settings_t   *p_ble_scan_cfg;     /**< BLE scan settings */
    const wiced_bt_cfg_ble_advert_settings_t *p_ble_advert_cfg;   /**< BLE advertisement settings */
    int8_t                                    default_ble_power_level;  /**< Default LE power level, Refer lm_TxPwrTable table for the power range */
}wiced_bt_cfg_ble_t;

/** Settings for application managed L2CAP protocols (optional) */
typedef struct
{
    uint8_t max_app_l2cap_psms;                 /**< Maximum number of application-managed PSMs        */
    uint8_t max_app_l2cap_channels;             /**< Maximum number of application-managed channels    */

    uint8_t max_app_l2cap_le_fixed_channels;    /**< Maximum number of application managed fixed channels supported. > */

    uint8_t max_app_l2cap_br_edr_ertm_chnls;    /**< Maximum application ERTM channels, BR/EDR only    */
    uint8_t max_app_l2cap_br_edr_ertm_tx_win;   /**< Maximum application ERTM TX Window, BR/EDR only   */
} wiced_bt_cfg_l2cap_application_t;

/** Bluetooth stack configuration */
typedef struct wiced_bt_cfg_settings_t_
{
    uint8_t *device_name;                                    /**< Local device name (NULL terminated) */
    wiced_bt_sec_level_t security_required;                  /**< BTM_SEC_BEST_EFFORT is recommended choice for most applications,
                                                                    to connect to the widest range of devices. Allows stack to choose
                                                                    the highest level of security possible between the two devices */
    const wiced_bt_cfg_br_t *p_br_cfg;                       /**< BR/EDR related configuration */
    const wiced_bt_cfg_ble_t *p_ble_cfg;                     /**< BLE related configuration */
    const wiced_bt_cfg_gatt_t *p_gatt_cfg;                   /**< GATT settings */
    const wiced_bt_cfg_isoc_t *p_isoc_cfg;                   /**< Ischoronous Connection configuration */
    const wiced_bt_cfg_l2cap_application_t *p_l2cap_app_cfg; /**< l2cap configuration fgitor application defined profiles/protocols */
} wiced_bt_cfg_settings_t;

/**
 * Returns the expected dynamic memory size required for the stack based on the p_bt_cfg_settings
 *
 * @param[in] p_bt_cfg_settings         : Bluetooth stack configuration
 *
 * @return    dynamic memory size requirements of the stack
 */
int32_t wiced_bt_stack_get_dynamic_memory_size_for_config(const wiced_bt_cfg_settings_t* p_bt_cfg_settings);


#ifdef __cplusplus
} /* extern "C" */
#endif


/**@} wiced_bt_cfg */
