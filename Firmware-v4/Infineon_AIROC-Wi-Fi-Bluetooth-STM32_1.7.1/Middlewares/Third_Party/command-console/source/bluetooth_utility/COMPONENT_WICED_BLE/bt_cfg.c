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
#ifndef DISABLE_COMMAND_CONSOLE_BT

#include "bt_cfg.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"

/* Note : For throughput measurement, min and max interval are set to 40 so that peer connects with 40 as connection interval */
#ifndef BLE_CONNECTION_INTERVAL
#define BLE_CONNECTION_INTERVAL  (40)
#endif

/* Null-Terminated Local Device Name */
/* Device Name */
uint8_t command_console_local_name[] = { 'L', 'E', ' ', 'C', 'o', 'C' ,'\0' };

wiced_bt_cfg_ble_scan_settings_t command_console_cfg_scan_settings =
{
    .scan_mode = BTM_BLE_SCAN_MODE_ACTIVE, /**< BLE scan mode ( BTM_BLE_SCAN_MODE_PASSIVE, BTM_BLE_SCAN_MODE_ACTIVE, or BTM_BLE_SCAN_MODE_NONE ) */

    /* Advertisement scan configuration */
    .high_duty_scan_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_INTERVAL, /**< High duty scan interval */
    .high_duty_scan_window = WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_WINDOW,   /**< High duty scan window */
    .high_duty_scan_duration = 5, /**< High duty scan duration in seconds ( 0 for infinite ) */

    .low_duty_scan_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_INTERVAL, /**< Low duty scan interval  */
    .low_duty_scan_window = WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_WINDOW,    /**< Low duty scan window */
    .low_duty_scan_duration = 5,  /**< Low duty scan duration in seconds ( 0 for infinite ) */

    /* Connection scan configuration */
    .high_duty_conn_scan_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_INTERVAL, /**< High duty cycle connection scan interval */
    .high_duty_conn_scan_window = WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_WINDOW,   /**< High duty cycle connection scan window */
    .high_duty_conn_duration = 30,      /**< High duty cycle connection duration in seconds ( 0 for infinite ) */

    .low_duty_conn_scan_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_INTERVAL, /**< Low duty cycle connection scan interval */
    .low_duty_conn_scan_window = WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_WINDOW,    /**< Low duty cycle connection scan window */
    .low_duty_conn_duration = 30,       /**< Low duty cycle connection duration in seconds ( 0 for infinite ) */

    .conn_min_interval = BLE_CONNECTION_INTERVAL,    /**< Minimum connection interval */
    .conn_max_interval = BLE_CONNECTION_INTERVAL,    /**< Maximum connection interval */

    .conn_latency = WICED_BT_CFG_DEFAULT_CONN_LATENCY,                         /**< Connection latency */
    .conn_supervision_timeout = WICED_BT_CFG_DEFAULT_CONN_SUPERVISION_TIMEOUT, /**< Connection link supervision timeout */
};

const wiced_bt_cfg_ble_advert_settings_t command_console_cfg_adv_settings =
{
    .channel_map = BTM_BLE_ADVERT_CHNL_37 | /**< Advertising channel map ( mask of BTM_BLE_ADVERT_CHNL_37, BTM_BLE_ADVERT_CHNL_38, BTM_BLE_ADVERT_CHNL_39 ) */
    BTM_BLE_ADVERT_CHNL_38 |
    BTM_BLE_ADVERT_CHNL_39,

    .high_duty_min_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MIN_INTERVAL, /**< High duty undirected connectable minimum advertising interval */
    .high_duty_max_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MAX_INTERVAL, /**< High duty undirected connectable maximum advertising interval */
    .high_duty_duration = 0,                                                   /**< High duty undirected connectable advertising duration in seconds ( 0 for infinite ) */

    .low_duty_min_interval = 1024, /**< Low duty undirected connectable minimum advertising interval */
    .low_duty_max_interval = 1024, /**< Low duty undirected connectable maximum advertising interval */
    .low_duty_duration = 0,        /**< Low duty undirected connectable advertising duration in seconds ( 0 for infinite ) */

    .high_duty_directed_min_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_DIRECTED_ADV_MIN_INTERVAL, /**< High duty directed connectable minimum advertising interval */
    .high_duty_directed_max_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_DIRECTED_ADV_MAX_INTERVAL, /**< High duty directed connectable maximum advertising interval */

    .low_duty_directed_min_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_DIRECTED_ADV_MIN_INTERVAL, /**< Low duty directed connectable minimum advertising interval */
    .low_duty_directed_max_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_DIRECTED_ADV_MAX_INTERVAL, /**< Low duty directed connectable maximum advertising interval */
    .low_duty_directed_duration = 30,                                                          /**< Low duty directed connectable advertising duration in seconds ( 0 for infinite ) */

    .high_duty_nonconn_min_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_NONCONN_ADV_MIN_INTERVAL, /**< High duty non-connectable minimum advertising interval */
    .high_duty_nonconn_max_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_NONCONN_ADV_MAX_INTERVAL, /**< High duty non-connectable maximum advertising interval */
    .high_duty_nonconn_duration = 30,                                                          /**< High duty non-connectable advertising duration in seconds ( 0 for infinite ) */

    .low_duty_nonconn_min_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_NONCONN_ADV_MIN_INTERVAL, /**< Low duty non-connectable minimum advertising interval */
    .low_duty_nonconn_max_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_NONCONN_ADV_MAX_INTERVAL, /**< Low duty non-connectable maximum advertising interval */
    .low_duty_nonconn_duration = 0                                                           /**< Low duty non-connectable advertising duration in seconds ( 0 for infinite ) */
};

wiced_bt_cfg_ble_t command_console_cfg_ble = {
    .ble_max_simultaneous_links = 4,
    .ble_max_rx_pdu_size = 512,

    .p_ble_scan_cfg = &command_console_cfg_scan_settings, /**< */
    .p_ble_advert_cfg = &command_console_cfg_adv_settings,
    .appearance = APPEARANCE_GENERIC_TAG,  /**< GATT appearance (see gatt_appearance_e) */

    .host_addr_resolution_db_size = 5, /**< LE Address Resolution DB settings - effective only for pre 4.2 controller*/
    .rpa_refresh_timeout = WICED_BT_CFG_DEFAULT_RANDOM_ADDRESS_NEVER_CHANGE, /**< Interval of  random address refreshing - secs */
};

wiced_bt_cfg_gatt_t command_console_cfg_gatt = {
    .max_db_service_modules = 0,   /**< Maximum number of service modules in the DB*/
    .max_eatt_bearers = 0,         /**< Maximum number of allowed gatt bearers */
};

wiced_bt_cfg_l2cap_application_t command_console_cfg_l2cap = {
    .max_app_l2cap_psms                 = 0,    /**< Maximum number of application-managed BR/EDR PSMs */
    .max_app_l2cap_channels             = 0,    /**< Maximum number of application-managed BR/EDR channels */

    .max_app_l2cap_le_fixed_channels    = 2,    /**< Maximum number of application-managed LE channels */

    .max_app_l2cap_br_edr_ertm_chnls    = 0,    /**< Maximum application ERTM channels, BR/EDR only */

    .max_app_l2cap_br_edr_ertm_tx_win   = 0     /**< Maximum application ERTM TX Window, BR/EDR only   */
};

const wiced_bt_cfg_settings_t wiced_bt_command_console_cfg_settings =
{
    .device_name = (uint8_t*)command_console_local_name,     /**< Local device name ( NULL terminated ) */
    .p_ble_cfg  = &command_console_cfg_ble,
    .p_gatt_cfg = &command_console_cfg_gatt,
    .p_l2cap_app_cfg = &command_console_cfg_l2cap,
};
#ifndef COMPONENT_CAT5
const cybt_platform_config_t bt_platform_cfg_settings =
{
    .hci_config =
    {
        .hci_transport = CYBT_HCI_UART,

        .hci =
        {
            .hci_uart =
            {
                .uart_tx_pin = CYBSP_BT_UART_TX,
                .uart_rx_pin = CYBSP_BT_UART_RX,
                .uart_rts_pin = CYBSP_BT_UART_RTS,
                .uart_cts_pin = CYBSP_BT_UART_CTS,

                .baud_rate_for_fw_download = 115200,
                .baud_rate_for_feature     = 115200,

                .data_bits = 8,
                .stop_bits = 1,
                .parity = CYHAL_UART_PARITY_NONE,
                .flow_control = WICED_TRUE
            }
        }
    },

    .controller_config =
    {
        .bt_power_pin      = CYBSP_BT_POWER,
        .sleep_mode =
        {
            .sleep_mode_enabled   = true,
            .device_wakeup_pin    = CYBSP_BT_DEVICE_WAKE,
            .host_wakeup_pin      = CYBSP_BT_HOST_WAKE,
            .device_wake_polarity = CYBT_WAKE_ACTIVE_LOW,
            .host_wake_polarity   = CYBT_WAKE_ACTIVE_LOW
        }
    },

    .task_mem_pool_size    = 2048
};
#endif
#endif