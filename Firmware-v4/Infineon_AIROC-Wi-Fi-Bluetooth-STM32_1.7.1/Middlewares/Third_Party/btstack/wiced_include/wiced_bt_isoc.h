/*
 * Copyright 2021-2023, Cypress Semiconductor Corporation or
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
 * WICED Bluetooth Low Energy (BLE) Ischoronous function
 *
 *  Some Acronyms and Abbreviations :
 *      CIS   -  Connected Isochronous Stream
 *      CIG   -  Connected Isochronous Group
 *      BIS   -  Broadcast Isochronous Stream
 *      BIG   -  Broadcast Isochronous Group
 *      ISO   -  Isochronous
 *      ISOAL -  Isochronous Adaption Layer
 */
#pragma once

#include "wiced_bt_types.h"
#include "wiced_result.h"
#include "wiced_bt_cfg.h"

/**
 * @defgroup  wicedbt_isoc        Ischoronous (ISOC)
 *
 * This section describes the API's to use ISOC functionality.
 *
*/

/**
 * @addtogroup  wicedbt_isoc_defs         ISOC data types and macros
 * @ingroup     wicedbt_isoc
 *
 * @{
*/
/*****************************************************************************
**  constants
*****************************************************************************/

#define WICED_BLE_ISOC_MIN_TRANSPORT_LATENCY      0x0005     /**< ISOC Minimum Latency */
#define WICED_BLE_ISOC_MAX_TRANSPORT_LATENCY      0x0FA0     /**< ISOC Maximum Latency */

typedef enum
{
    WICED_BLE_ISOC_DPD_INPUT = 0,     // ISO driver is source (Input data path (Host to Controller))
    WICED_BLE_ISOC_DPD_OUTPUT,        // ISO driver is sink (Output data path (Controller to Host))
    WICED_BLE_ISOC_DPD_MAX_DIRECTIONS // must be last
} wiced_bt_isoc_data_path_direction_t;

typedef enum
{
    WICED_BLE_ISOC_DPD_UNUSED = 0,
    WICED_BLE_ISOC_DPD_INPUT_BIT = 1,
    WICED_BLE_ISOC_DPD_OUTPUT_BIT = 2,
    WICED_BLE_ISOC_DPD_INPUT_OUTPUT_BIT = WICED_BLE_ISOC_DPD_INPUT_BIT & WICED_BLE_ISOC_DPD_OUTPUT_BIT,
    WICED_BLE_ISOC_DPD_RESERVED
} wiced_bt_isoc_data_path_bit_t;

#define ISOC_SET_DATA_PATH_DIR(var, dir) (var |= (1 << dir))
#define ISOC_GET_DATA_PATH_DIR(var, dir) (var & (1 << dir))
#define ISOC_CLEAR_DATA_PATH_DIR(var, dir) (var &= ~(1 << dir))

typedef enum
{
    WICED_BLE_ISOC_DPID_HCI = 0,
    WICED_BLE_ISOC_DPID_DIABLED = 0xFF
} wiced_bt_isoc_data_path_id_t;

/** ISOC packing methods */
enum wiced_bt_isoc_packing_e
{
    WICED_BLE_ISOC_SEQUENTIAL_PACKING = 0,
    WICED_BLE_ISOC_INTERLEAVED_PACKING = 1
};
typedef uint8_t wiced_bt_isoc_packing_t; /**< ISOC packing methods (see #wiced_bt_isoc_packing_e) */

/** ISOC Framing types */
enum wiced_bt_isoc_framing_e
{
    WICED_BLE_ISOC_UNFRAMED = 0,
    WICED_BLE_ISOC_FRAMED   = 1
};
typedef uint8_t wiced_bt_isoc_framing_t; /**< ISOC Framing types (see #wiced_bt_isoc_framing_e) */

/** ISOC LE PHY */
enum wiced_bt_isoc_phy_e
{
    WICED_BLE_ISOC_LE_1M_PHY = 1,
    WICED_BLE_ISOC_LE_2M_PHY = 2,
    WICED_BLE_ISOC_LE_CODED  = 4,
};
typedef uint8_t wiced_bt_isoc_phy_t; /**< ISOC LE PHY (see #wiced_bt_isoc_phy_e) */

/** Broadcast ISOC Encryption */
enum wiced_bt_isoc_encryption_e
{
    WICED_BLE_ISOC_UNENCRYPTED = 0,
    WICED_BLE_ISOC_ENCRYPTED   = 1,
};
typedef uint8_t wiced_bt_isoc_encryption_t; /**< ISOC Encryption (see #wiced_bt_isoc_encryption_e) */

/** ISOC Events */
enum wiced_bt_isoc_event_e
{
    WICED_BLE_ISOC_SET_CIG_CMD_COMPLETE,  /**< CIG Command Response */
    WICED_BLE_ISOC_CIS_REQUEST,           /**< CIS connection Requested */
    WICED_BLE_ISOC_CIS_ESTABLISHED,       /**< CIS connection established */
    WICED_BLE_ISOC_SLAVE_CLOCK_ACCURACY,  /**< Slave Clock Accuracy */
    WICED_BLE_ISOC_CIS_DISCONNECTED,      /**< CIS disconnected */
    WICED_BLE_ISOC_DATA_PATH_SETUP,       /**< CIS Data path status */
    WICED_BLE_ISOC_DATA_PATH_REMOVED,     /**< CIS Data path status */
    WICED_BLE_ISOC_BIG_CREATED,           /**< BIG Connected */
    WICED_BLE_ISOC_BIG_SYNC_ESTABLISHED,  /**< BIG Sync Established */
    WICED_BLE_ISOC_BIG_TERMINATED,        /**< BIG Terminated */
    WICED_BLE_ISOC_BIG_SYNC_LOST,         /**< BIG Sync Lost */
};
typedef uint8_t wiced_bt_isoc_event_t; /**< ISOC Events (see #wiced_bt_isoc_event_e) */

/** ISOC CIS Request Event Data */
typedef struct
{
    uint16_t    cis_conn_handle;        /**< CIS Connection Handle */
    uint16_t    acl_handle;             /**< ACL Connection Handle */
    uint8_t     cig_id;                 /**< CIG ID */
    uint8_t     cis_id;                 /**< CIS ID */
}wiced_bt_isoc_cis_request_data_t;

/** ISOC CIS Disconnect Event Data */
typedef struct
{
    uint16_t    cis_conn_handle;        /**< CIS Connection Handle */
    uint8_t     cis_id;                 /**< CIS ID */
    uint8_t     cig_id;                 /**< CIG ID */
    uint8_t     reason;                 /**< Disconnection Reason */
}wiced_bt_isoc_cis_disconnect_data_t;

/** ISOC CIS Established Event Data */
typedef struct
{
    uint8_t                     status;                 /**< CIG Establishment Status  (0 = Success). Refer Core Spec v5.2 [Vol 1] Part F, Controller Error Codes */
    uint16_t                    cis_conn_handle;        /**< CIS Connection Handle */
    uint8_t                     cig_id;                 /**< CIG ID */
    uint8_t                     cis_id;                 /**< CIS ID */
    uint32_t                    cig_sync_delay;         /**< CIG Sync Delay in microseconds */
    uint32_t                    cis_sync_delay;         /**< CIS Sync Delay in microseconds */
    uint32_t                    latency_c_to_p;         /**< Maximum time, in microseconds, for an SDU to be transported from the master Controller to slave Controller */
    uint32_t                    latency_p_to_c;         /**< Maximum time, in microseconds, for an SDU to be transported from the slave Controller to master Controller */
    wiced_bt_isoc_phy_t     phy_c_to_p;             /**< The transmitter PHY of packets from the master */
    wiced_bt_isoc_phy_t     phy_p_to_c;             /**< The transmitter PHY of packets from the slave */
    uint8_t                     nse;                    /**< Maximum Number of Subevent in each isochronous event */
    uint8_t                     bn_c_to_p;              /**< Burst number for master to slave transmission */
    uint8_t                     bn_p_to_c;              /**< Burst number for slave to master transmission */
    uint8_t                     ft_c_to_p;              /**< Flush timeout, in multiples of the ISO_Interval for master to slave transmission */
    uint8_t                     ft_p_to_c;              /**< Flush timeout, in multiples of the ISO_Interval for slave to master transmission */
    uint16_t                    max_pdu_c_to_p;         /**< Maximum size, in bytes, of an SDU from the master’s Host */
    uint16_t                    max_pdu_p_to_c;         /**< Maximum size, in octets, of an SDU from the slave’s Host */
    uint16_t                    iso_interval;           /**< Time between two consecutive CIS anchor points */
}wiced_bt_isoc_cis_established_data_t;

/** ISOC CIG Command Status data */
typedef struct
{
    uint8_t        status;                       /**< CIG Establishment Status  (0 = Success). Refer Core Spec v5.2 [Vol 1] Part F, Controller Error Codes */
    uint8_t        cig_id;                       /**< CIG ID */
    uint8_t        cis_count;                    /**< CIS Count */
    uint16_t       *cis_connection_handle_list;  /**< CIS Connection Handle List */
    void           *ctx;                         /**< upper layer context */
}wiced_bt_isoc_cig_status_data_t;

/** ISOC Peer Slave Clock Accuracy data */
typedef struct
{
    uint8_t        status;                       /**< SCA Status (0 = Success). Refer Core Spec v5.2 [Vol 1] Part F, Controller Error Codes*/
    uint16_t       acl_handle;                   /**< ACL Connection Handle */
    wiced_bt_device_address_t peer_bda;          /**< Peer Bluetooth Address */
    uint8_t        sca;                          /**< Slave Clock Accuracy value */
}wiced_bt_isoc_sca_t;

typedef struct
{
    uint8_t status; /**< Data path Status  (0 = Success). Refer Core Spec v5.2 [Vol 1] Part F, Controller Error Codes */
    uint16_t conn_hdl;                                 /**< CIS/BIS Connection Handle  */
    wiced_bt_isoc_data_path_direction_t data_path_dir; /**< data path direction (valid for data path setup only) */
} wiced_bt_isoc_data_path_status_t;

/** ISOC BIG Terminate/Sync_Lost Event Data */
typedef struct
{
    uint8_t        big_handle;                   /**< BIG Handle */
    uint8_t        reason;                       /**< Reason for termination. Refer Core Spec v5.2 [Vol 1] Part F, Controller Error Codes */
}wiced_bt_isoc_terminated_data_t;

/** ISOC BIG Sync Establishment data */
typedef struct
{
    uint8_t        status;                       /**< Create BIG Status (0 = Success). Refer Core Spec v5.2 [Vol 1] Part F, Controller Error Codes */
    uint8_t        big_handle;                   /**< BIG Handle */
    uint32_t       trans_latency;                /**< The maximum delay time, in microseconds, for transmission of SDUs of all BISes in a BIG event */
    uint8_t        number_of_subevents;          /**< The number of subevents in each BIS event in the BIG */
    uint8_t        burst_number;                 /**< The number of new payloads in each BIS event */
    uint8_t        pretransmission_offset;       /**< Offset used for pre-transmissions */
    uint8_t        immediate_repetition_count;   /**< The number of times a payload is transmitted in a BIS event */
    uint16_t       max_pdu;                      /**< Maximum size, in octets, of the payload */
    uint16_t       iso_interval;                 /**< The time between two consecutive BIG anchor points. Time = N * 1.25 ms */
    uint8_t        num_bis;                      /**< Total number of BISes in the BIG */
    uint16_t       *bis_conn_hdl_list;           /**< The connection handles of the BISes */
}wiced_bt_isoc_big_sync_established_t;

/** ISOC BIG Command Status data */
typedef struct
{
    wiced_bt_isoc_big_sync_established_t sync_data; /**< BIG Sync Data */
    uint32_t sync_delay;                            /**< BIG Sync Delay in microseconds */
    wiced_bt_isoc_phy_t phy;                    /**< The transmitter PHY of packets */
} wiced_bt_isoc_create_big_complete_t;

/** ISOC Event Data */
typedef union
{
    wiced_bt_isoc_cig_status_data_t cig_status_data;           /**< CIG Command Status */
    wiced_bt_isoc_cis_established_data_t cis_established_data; /**< CIS Established */
    wiced_bt_isoc_cis_request_data_t cis_request;              /**< CIS Request     */
    wiced_bt_isoc_sca_t sca_data;                              /**< Slave Clock Accuracy */
    wiced_bt_isoc_cis_disconnect_data_t cis_disconnect;        /**< CIS Disconnect  */
    wiced_bt_isoc_data_path_status_t datapath;                 /**< Data Path Status (setup/remove) */
    wiced_bt_isoc_create_big_complete_t create_big;            /**< Create BIG Command Status */
    wiced_bt_isoc_terminated_data_t terminate_big;             /**< Terminate BIG Command Status */
    wiced_bt_isoc_big_sync_established_t big_sync_established; /**< BIG Sync Established data */
    wiced_bt_isoc_terminated_data_t big_sync_lost;             /**< BIG Sync Lost Data */
} wiced_bt_isoc_event_data_t;

/** ISOC CIS Configuration */
typedef struct
{
    uint8_t                     cis_id;           /**< CIS Id : ZERO if not created*/
    uint16_t                    max_sdu_c_to_p;   /**< Maximum size, in bytes, of an SDU from the master’s Host
                                                       Valid Range 0x000 to 0xFFF*/
    uint16_t                    max_sdu_p_to_c;   /**< Maximum size, in octets, of an SDU from the slave’s Host
                                                       Valid Range 0x000 to 0xFFF*/
    wiced_bt_isoc_phy_t     phy_c_to_p;       /**< The transmitter PHY of packets from the master */
    wiced_bt_isoc_phy_t     phy_p_to_c;       /**< The transmitter PHY of packets from the slave */
    uint8_t                     rtn_c_to_p;       /**< Maximum number of times every CIS Data PDU should be retransmitted from the master to slave */
    uint8_t                     rtn_p_to_c;       /**< Maximum number of times every CIS Data PDU should be retransmitted from the slave to master */
}wiced_bt_ble_cis_config_t;

/** ISOC CIG Configuration */
typedef struct
{
    uint8_t                     cig_id;                     /**< CIG ID if known */
    uint32_t                    sdu_interval_c_to_p;        /**< Time interval in microseconds between the start of consecutive SDUs from the master’s Host for all the CISes in the CIG */
    uint32_t                    sdu_interval_p_to_c;        /**< Time interval in microseconds between the start of consecutive SDUs from the slave’s Host for all the CISes in the CIG. */
    uint8_t                     peripheral_clock_accuracy;       /**< Slave Clock Accuracy */
    uint16_t                    max_trans_latency_c_to_p;   /**< Maximum time, in microseconds, for an SDU to be transported from the master Controller to slave Controller */
    uint16_t                    max_trans_latency_p_to_c;   /**< Maximum time, in microseconds, for an SDU to be transported from the slave Controller to master Controller */
    wiced_bt_isoc_packing_t packing;                    /**< Packing method  */
    wiced_bt_isoc_framing_t framing;                    /**< Framing parameter */
    uint8_t                     cis_count;                  /**< Total number of CISes in the CIG being added or modified
                                                                  Valid Range 0x00 to 0x10 */
    wiced_bt_ble_cis_config_t   *p_cis_config_list;           /**< CIS configurations */
}wiced_bt_ble_cig_param_t;

typedef struct
{
    uint8_t cis_id;                     /**< CIS Id : ZERO if not created*/
    uint8_t nse;                        /**< Maximum number of subevents in each CIS event */
    uint16_t max_sdu_c_to_p;            /**< Maximum size, in bytes, of an SDU from the master’s Host Valid Range 0x000 to 0xFFF*/
    uint16_t max_sdu_p_to_c;            /**< Maximum size, in octets, of an SDU from the slave’s Host Valid Range 0x000 to 0xFFF*/
    uint16_t max_pdu_c_to_p;            /**< Maximum size, in bytes, of an SDU from the master’s Host Valid Range 0x000 to 0xFFF*/
    uint16_t max_pdu_p_to_c;            /**< Maximum size, in octets, of an SDU from the slave’s Host Valid Range 0x000 to 0xFFF*/
    wiced_bt_isoc_phy_t phy_c_to_p; /**< The transmitter PHY of packets from the master */
    wiced_bt_isoc_phy_t phy_p_to_c; /**< The transmitter PHY of packets from the slave */
    uint8_t bn_c_to_p;                  /**< Maximum number of times every CIS Data PDU should be retransmitted from the master to slave */
    uint8_t bn_p_to_c;                  /**< Maximum number of times every CIS Data PDU should be retransmitted from the slave to master */
} wiced_bt_ble_cis_config_test_t;

typedef struct
{
    uint8_t cig_id;               /**< CIG ID if known */
    uint32_t sdu_interval_c_to_p; /**< Time interval in microseconds between the start of consecutive SDUs from the master’s Host for all the CISes in the CIG */
    uint32_t sdu_interval_p_to_c; /**< Time interval in microseconds between the start of consecutive SDUs from the slave’s Host for all the CISes in the CIG. */
    uint8_t ft_c_to_p;            /**< The flush timeout in multiples of ISO_Interval for each payload sent from the master to slave. */
    uint8_t ft_p_to_c;            /**< The flush timeout in multiples of ISO_Interval for each payload sent from the slave to master. */
    uint16_t iso_interval;        /**< Time between consecutive CIS anchor points */
    uint8_t peripheral_clock_accuracy;               /**< Slave Clock Accuracy */
    wiced_bt_isoc_packing_t packing;        /**< Packing method  */
    wiced_bt_isoc_framing_t framing;        /**< Framing parameter */
    uint8_t cis_count;                          /**< Total number of CISes in the CIG being added or modified Valid Range 0x00 to 0x10 */
    wiced_bt_ble_cis_config_test_t *p_cis_config_list; /**< CIS configurations */
} wiced_bt_ble_cig_param_test_t;

typedef struct
{
    uint8_t cis_count;
    uint16_t *cis_handle_list;
    uint16_t *acl_handle_list;
} wiced_bt_isoc_create_cis_param_t;

typedef struct
{
    uint8_t big_handle;
    uint8_t adv_handle;
    uint8_t num_bis;
    uint32_t sdu_interval;
    uint16_t max_sdu;
    uint16_t max_trans_latency;
    uint8_t rtn;
    uint8_t phy;
    uint8_t packing;
    uint8_t framing;
    uint8_t encrypt;
    uint8_t broadcast_code[16];
} wiced_bt_isoc_create_big_param_t;

typedef struct
{
    uint8_t big_handle;
    uint16_t sync_handle;
    uint8_t encrypt;
    uint8_t *broadcast_code;
    uint8_t max_sub_events;
    uint16_t big_sync_timeout;
    uint8_t num_bis;
    uint8_t *bis_idx_list;
} wiced_bt_isoc_big_create_sync_t;

/** ISOC Data callbacks */
/**
* Callback for receiving ISOC data
*
* @param[in] p_data : incoming ISOC data
* @param[in] length : length of the data
*/
typedef void (*wiced_bt_iso_rx_cb_t)(uint8_t *p_data, uint32_t length);

/**
* Callback for receiving number of completed packets
*
* @param[in] p_buf : incoming number of completed packets event
*
* @return wiced_bool_t, return FALSE in case any of the connection handles
* in the event are not handled/(ISOC)
*/
typedef wiced_bool_t (*wiced_bt_iso_num_complete_cb_t)(uint8_t *p_buf);


/******************************************************
 *               Function Declarations
 *
 ******************************************************/
#ifdef __cplusplus
extern "C" {
#endif


/**
 * ISOC event callback
 *
 * Callback for ISOC event notifications
 * Registered using #wiced_bt_isoc_register_cb
 *
 * @param event             : Event ID
 * @param p_event_data      : Event data
 *
 * @return Status of event handling
*/
typedef void wiced_bt_isoc_cback_t(wiced_bt_isoc_event_t event, wiced_bt_isoc_event_data_t *p_event_data);

/** @} wicedbt_isoc_defs         */

/**
 * @addtogroup  wicedbt_isoc_functions   Ischoronous (ISOC) functions
 * @ingroup     wicedbt_isoc
 *
 * Ischoronous(ISOC) Functions.
 *
 * @{
 */
/**
 *
 * Function         wiced_bt_isoc_register_cb
 *
 *                  ISOC Register event callback handler
 *
 * @param[in]       isoc_cb  : ISOC event callback
 *
 * @return      None
 *
 */
void wiced_bt_isoc_register_cb(wiced_bt_isoc_cback_t isoc_cb);

/**
 * @brief Register ISO data event callbacks
 *
 * @param rx_data_cb Callback upon receiving ISO data
 * @param num_complete_cb Callback after transmitting ISO data
 */
void wiced_bt_isoc_register_data_cb(wiced_bt_iso_rx_cb_t rx_data_cb, wiced_bt_iso_num_complete_cb_t num_complete_cb);

/**
 *
 * Function         wiced_bt_isoc_central_set_cig_param
 *
 * Used by a master’s Host to set the parameters of one or more CISes that are
 * associated with a CIG in the Controller. If none of the CISes in that CIG
 * have been created, this command may also be used to modify or add CIS(s)
 * to that CIG.
 *
 * @param[in]       cig_params  : CIG parameter (@ref wiced_bt_ble_cig_param_t)
 * @param[in]       ctx         : pointer to application data that will be given back
 *                                to application through the WICED_BLE_ISOC_SET_CIG_CMD_COMPLETE event
 *
 * @return      status
 *
 */
wiced_result_t wiced_bt_isoc_central_set_cig_param(wiced_bt_ble_cig_param_t *cig_params, void *ctx);

/**
 *
 * Function         wiced_bt_isoc_central_set_cig_param_test
 *
 * Command should only be used for testing purposes only.
 *
 * Used by a master’s Host to set the parameters of one or more CISes that are
 * associated with a CIG in the Controller. If none of the CISes in that CIG
 * have been created, this command may also be used to modify or add CIS(s)
 * to that CIG.
 *
 * @param[in]       cig_params  : CIG parameter (@ref wiced_bt_ble_cig_param_t)
 * @param[in]       ctx         : pointer to application data that will be given back
 *                                to application through the WICED_BLE_ISOC_SET_CIG_CMD_COMPLETE event
 *
 * @return      status
 *
 */
wiced_result_t wiced_bt_isoc_central_set_cig_param_test(wiced_bt_ble_cig_param_test_t *cig_params, void *ctx);

/**
 *
 * Function         wiced_bt_isoc_central_create_cis
 *
 *                  This API is invoked to create one or more CIS channels
 *
 * @param[in]       cis_count        : Number of elements in CIS handle list and ACL handle list.
 *                                     These lists must be of the same length
 * @param[in]       cis_handle_list  : List of connection handles of the cis to be established
 * @param[in]       acl_handle_list  : ACL connection handle associated with the cis handles.
 *                                     The ith acl handle corresponds to the ith cis handle, etc.
 *
 * @return      status
 *
 *  Note : Once CIS is establish WICED_BLE_ISOC_CIS_ESTABLISHED event will be received in registered application callback.
 */
wiced_result_t wiced_bt_isoc_central_create_cis(wiced_bt_isoc_create_cis_param_t *create_cis_param);

/**
 *
 * Function         wiced_bt_isoc_create_cis_by_bda
 *
 *                  Create CIS (Connected Isochronous Stream) connection
 *
 * @param[in]       cis_count         : Number of elements in Peer BDA list and CIS handle list.
 * @param[in]       peer_bda_list   : Peer Bluetooth Address List
 * @param[in]       cis_handle_list : CIS handles List
 *
 * @return      status
 *
 *  Note : Once CIS is establish WICED_BLE_ISOC_CIS_ESTABLISHED event will be received in registered application callback.
 */
wiced_result_t wiced_bt_isoc_create_cis_by_bda(uint8_t cis_count, uint8_t *peer_bda_list, uint16_t *cis_handle_list);

/**
 *
 * Function         wiced_bt_isoc_peripheral_accept_cis
 *
 *                  Accept CIS (Connected Isochronous Stream) connection request
 *                  Slave should call this API on receiving WICED_BLE_ISOC_CIS_REQUEST event in registered application callback
 *
 * @param[in]       cis_handle  : CIS handle
 *
 * @return      status
 *
 *  Note : Once CIS is establish WICED_BLE_ISOC_CIS_ESTABLISHED event will be received in registered application callback.
 */
wiced_result_t wiced_bt_isoc_peripheral_accept_cis(
    uint8_t cig_id, uint8_t cis_id, uint16_t cis_conn_handle, uint8_t src_ase_id, uint8_t sink_ase_id);

/**
 *
 * Function         wiced_bt_isoc_peripheral_reject_cis
 *
 *                  Reject CIS (Connected Isochronous Stream) connection request
 *                  Slave should call this API on receiving WICED_BLE_ISOC_CIS_REQUEST event in registered application callback
 *
 * @param[in]       cis_handle  : CIS handle
 * @param[in]       reason      : Reject Reason
 *
 * @return      status
 *
 */
wiced_result_t wiced_bt_isoc_peripheral_reject_cis(uint16_t cis_handle, uint8_t reason);

/**
 *
 * Function         wiced_bt_isoc_peripheral_remove_cig
 *
 *                  Remove CIG (Connected Isochronous Group)
 *                  Slave should call this API on receiving WICED_BLE_ISOC_CIS_DISCONNECTED event in registered application callback and if ASCS State is Releasing
 *
 * @param[in]       cig_id  : CIG ID
 *
 * @return      status
 *
 */
wiced_result_t wiced_bt_isoc_peripheral_remove_cig(uint8_t cig_id);

/**
 *
 * Function         wiced_bt_isoc_disconnect_cis
 *
 *                  Disconnect CIS (Connected Isochronous Stream) connection
 *
 * @param[in]       cis_handle  : CIS handle
 *
 * @return      status
 *
 *  Note : Once CIS is disconnected WICED_BLE_ISOC_CIS_DISCONNECTED event will be received in registered application callback.
 */
wiced_result_t wiced_bt_isoc_disconnect_cis(uint16_t cis_handle);

/**
 * @brief Get CIS connection status
 *
 * @param cig_id CIG identifier
 * @param cis_id CIS identifier
 * @return wiced_bool_t TRUE if CIS connection exists
 */
wiced_bool_t wiced_bt_isoc_is_cis_connected(uint8_t cig_id, uint8_t cis_id);

/**
 * @brief Get CIS connection status by CIS conn id
 *
 * @param cis_conn_id CIS conn id
 * @return wiced_bool_t TRUE if CIS connection exists
 */
wiced_bool_t wiced_bt_isoc_is_cis_connected_by_conn_id(uint16_t cis_conn_id);

/**
 *
 * Function         wiced_bt_isoc_central_request_peer_sca
 *
 *                  Request for Slave Clock Accuracy
 *
 * @param[in]       peer_bda  : Peer Bluetooth Address
 *
 * @return      status
 *
 *  Note : WICED_BLE_ISOC_SLAVE_CLOCK_ACCURACY event will be received in registered application callback.
 */
wiced_result_t wiced_bt_isoc_central_request_peer_sca(wiced_bt_device_address_t peer_bda);

/**
 *
 * Function         wiced_bt_isoc_central_remove_cig
 *
 *                  Remove given CIG
 *
 * @param[in]       cig_id  : CIG ID
 *
 * @return      status
 */
wiced_result_t wiced_bt_isoc_central_remove_cig(uint8_t cig_id);

/**
 * wiced_bt_isoc_central_get_psn_by_cis_handle
 *
 * @param[in]       handle  : CIS handle
 * @return      packet sequence number
 */
uint16_t wiced_bt_isoc_central_get_psn_by_cis_handle(uint16_t handle);

/**
 * wiced_bt_isoc_get_psn_by_bis_handle
 *
 * @param[in]       handle  : BIS handle
 * @return      packet sequence number
 **/
uint16_t wiced_bt_isoc_get_psn_by_bis_handle(uint16_t handle);

/**
 * wiced_bt_isoc_is_bis_created
 *
 * @param[in]       handle  : BIS Connection handle
 * @return     TRUE if BIG exists
 */
wiced_bool_t wiced_bt_isoc_is_bis_created(uint16_t bis_conn_handle);

/**
 * @brief Create BIG with provided parameters
 *
 * @param p_big_param Number of BIS, SDU Interval, Packing, Framing
 * @return WICED_SUCCESS if successful
 */
wiced_result_t wiced_bt_isoc_central_create_big(wiced_bt_isoc_create_big_param_t *p_big_param);

/**
 * @brief terminate a BIG identified by the BIG_Handle
 *
 * @param big_handle Used to identify the BIG
 * @param reason Reason the BIG is terminated.
 * @return wiced_result_t WICED_SUCCESS if successful
 */
wiced_result_t wiced_bt_isoc_central_terminate_big(uint8_t big_handle, uint8_t reason);

/**
 * @brief Sync to the BIS stream described by wiced_bt_isoc_big_create_sync_t
 *
 * @param p_create_sync sync_handle, list of BIS indices
 * @return wiced_bool_t TRUE if successful
 */
wiced_bool_t wiced_bt_isoc_peripheral_big_create_sync(wiced_bt_isoc_big_create_sync_t *p_create_sync);

/**
 * @brief Stop synchronizing or cancel the process of synchronizing to
 * the BIG identified by the BIG_Handle
 *
 * @param big_handle Used to identify the BIG
 * @return wiced_result_t wiced_result_t WICED_SUCCESS if successful
 */
wiced_result_t wiced_bt_isoc_peripheral_big_terminate_sync(uint8_t big_handle);

/**
 * @brief Remove data path setup for a CIS/BIS
 *
 * @param conn_hdl CIS/BIS Connection handle
 * @param is_cis TRUE if CIS connection handle is provided
 * @param data_path_dir_bitfield see #wiced_bt_isoc_data_path_bit_t
 *                               bit 0: Remove Input data path
 *                               bit 1: Remove output data path
 * @return wiced_bool_t TRUE if successful in sending the command
 */
wiced_bool_t wiced_bt_isoc_remove_data_path(uint16_t conn_hdl,
                                            wiced_bool_t is_cis,
                                            wiced_bt_isoc_data_path_bit_t data_path_dir_bitfield);

/**
 * @brief Get status of the ISO CIS/BIS data path
 *
 * @param cig_id CIG identifier
 * @param cis_id CIS identifier
 * @param data_path_dir INPUT/OUTPUT see #wiced_bt_isoc_data_path_direction_t
 * @return wiced_bool_t
 */
wiced_bool_t wiced_bt_isoc_is_data_path_active(uint8_t cig_id,
                                               uint8_t cis_id,
                                               wiced_bt_isoc_data_path_direction_t data_path_dir);

/**
 * @brief
 *
 * @param cig_id CIG identifier
 * @param cis_id CIS identifier
 * @return cis_conn_handle CIS connection handle
 */
void wiced_bt_isoc_update_cis_conn_handle(uint8_t cig_id, uint8_t cis_id, uint16_t cis_conn_handle);

/**
 * @brief
 *
 * @param cig_id CIG identifier
 * @param cis_id CIS identifier
 * @return uint16_t CIS connection handle
 */
uint16_t wiced_bt_isoc_get_cis_conn_handle(uint8_t cig_id, uint8_t cis_id);

/**
 * @brief Can be invoked twice per connection handle (once per direction)
 * Supports only HCI for datapath and Does not support configuring codec in the controller
 *
 * @param conn_hdl CIS/BIS Connection handle
 * @param is_cis TRUE if CIS connection handle is provided
 * @param data_path_dir see #wiced_bt_isoc_data_path_direction_t
 * @param data_path_id see #wiced_bt_isoc_data_path_id_t
 * @param controller_delay select a suitable Controller_Delay value from the range of values
 *                         provided by the HCI_Read_Local_Supported_Controller_Delay command
 * @return wiced_bool_t TRUE if successful in sending the command
 */
wiced_bool_t wiced_bt_isoc_setup_data_path(uint16_t conn_hdl,
                                           wiced_bool_t is_cis,
                                           wiced_bt_isoc_data_path_direction_t data_path_dir,
                                           wiced_bt_isoc_data_path_id_t data_path_id,
                                           uint32_t controller_delay);

/**
  * This function writes ISO buffer to the lower layer (or controller in the Hosted Stack)
  *
  * Called by application to send the isoc data.
  *
  * @param[in] p_data    : Pointer to the buffer
  * @param[in] len       : Length of data at p_data
  *
  * @return : wiced_bool_t TRUE if successful otherwise FALSE
  */

 wiced_bool_t wiced_bt_write_iso_data_to_lower(uint8_t* p_data, uint16_t len);

/** This function returns ISOC buffer size */

 uint16_t wiced_bt_isoc_get_max_data_pkt_len(void);


/**@} wicedbt_isoc_functions */

#ifdef __cplusplus
}

#endif
