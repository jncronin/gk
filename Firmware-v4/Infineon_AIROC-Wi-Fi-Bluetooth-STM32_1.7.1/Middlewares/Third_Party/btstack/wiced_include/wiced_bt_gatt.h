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
 *  WICED Generic Attribute (GATT) Application Programming Interface
 */
#pragma once

#include "wiced_result.h"
#include "gattdefs.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_ble.h"
#include "l2cdefs.h"
#include "wiced_bt_uuid.h"


/**
 * @addtogroup wicedbt_gatt
 * @{
 */

 /**
 * Default GATT MTU size over LE link
 */
#define GATT_BLE_DEFAULT_MTU_SIZE 23
/**
 * Size of the signature appended to application data in signed write cmd
 */
#define GATT_AUTH_SIGN_LEN   12
/**
 * Max size of application data allowed to be sent using the signed write cmd.
 */
#define GATT_CLIENT_MAX_WRITE_SIGNED_DATA (23 - 3 - GATT_AUTH_SIGN_LEN)

 /**
  * GATT Header size (1 byte opcode + 2 byte handle)
  */
#define WICED_GATT_HDR_SIZE  3

/** GATT Status Codes*/
enum wiced_bt_gatt_status_e
{
    WICED_BT_GATT_SUCCESS                    = 0x00,         /**< Success */
    WICED_BT_GATT_INVALID_HANDLE             = 0x01,         /**< Invalid Handle */
    WICED_BT_GATT_READ_NOT_PERMIT            = 0x02,         /**< Read Not Permitted */
    WICED_BT_GATT_WRITE_NOT_PERMIT           = 0x03,         /**< Write Not permitted */
    WICED_BT_GATT_INVALID_PDU                = 0x04,         /**< Invalid PDU */
    WICED_BT_GATT_INSUF_AUTHENTICATION       = 0x05,         /**< Insufficient Authentication */
    WICED_BT_GATT_REQ_NOT_SUPPORTED          = 0x06,         /**< Request Not Supported */
    WICED_BT_GATT_INVALID_OFFSET             = 0x07,         /**< Invalid Offset */
    WICED_BT_GATT_INSUF_AUTHORIZATION        = 0x08,         /**< Insufficient Authorization */
    WICED_BT_GATT_PREPARE_Q_FULL             = 0x09,         /**< Prepare Queue Full */
    WICED_BT_GATT_ATTRIBUTE_NOT_FOUND        = 0x0a,         /**< Attribute Not Found */
    WICED_BT_GATT_NOT_LONG                   = 0x0b,         /**< Not Long Size */
    WICED_BT_GATT_INSUF_KEY_SIZE             = 0x0c,         /**< Insufficient Key Size */
    WICED_BT_GATT_INVALID_ATTR_LEN           = 0x0d,         /**< Invalid Attribute Length */
    WICED_BT_GATT_ERR_UNLIKELY               = 0x0e,         /**< Error Unlikely */
    WICED_BT_GATT_INSUF_ENCRYPTION           = 0x0f,         /**< Insufficient Encryption */
    WICED_BT_GATT_UNSUPPORT_GRP_TYPE         = 0x10,         /**< Unsupported Group Type */
    WICED_BT_GATT_INSUF_RESOURCE             = 0x11,         /**< Insufficient Resource */
    WICED_BT_GATT_DATABASE_OUT_OF_SYNC       = 0x12,         /**< GATT Database Out of Sync */
	WICED_BT_GATT_VALUE_NOT_ALLOWED          = 0x13,         /**< Value Not allowed */
                                                             /* 0xE0 ~ 0xFB reserved for future use */
    WICED_BT_GATT_WRITE_REQ_REJECTED         = 0xFC,         /**< Client Write operation rejected */
    WICED_BT_GATT_CCCD_IMPROPER_CONFIGURED   = 0xFD,         /**< Client Characteristic Configuration Descriptor Improperly Configured */
    WICED_BT_GATT_BUSY                       = 0xFE,         /**< Busy or Procedure already in progress */
    WICED_BT_GATT_OUT_OF_RANGE               = 0xFF,         /**< Value Out of Range */
                                                              /* WICED defined status  */
    WICED_BT_GATT_ILLEGAL_PARAMETER          = 0x8780,         /**< Illegal Parameter */
    WICED_BT_GATT_NO_RESOURCES               = 0x8781,         /**< No Resources */
    WICED_BT_GATT_INTERNAL_ERROR             = 0x8783,         /**< Internal Error */
    WICED_BT_GATT_WRONG_STATE                = 0x8784,         /**< Wrong State */
    WICED_BT_GATT_DB_FULL                    = 0x8785,         /**< DB Full */
    WICED_BT_GATT_UNUSED1                    = 0x8786,         /**< Unused */
    WICED_BT_GATT_ERROR                      = 0x8787,         /**< Error */
    WICED_BT_GATT_CMD_STARTED                = 0x8788,         /**< Command Started */
    WICED_BT_GATT_PENDING                    = 0x8789,         /**< Pending */
    WICED_BT_GATT_AUTH_FAIL                  = 0x878A,         /**< Authentication Fail */
    WICED_BT_GATT_MORE                       = 0x878B,         /**< More */
    WICED_BT_GATT_INVALID_CFG                = 0x878C,         /**< Invalid Configuration */
    WICED_BT_GATT_SERVICE_STARTED            = 0x878D,         /**< Service Started */
    WICED_BT_GATT_ENCRYPTED_MITM             = WICED_BT_GATT_SUCCESS, /**< Encrypted MITM */
    WICED_BT_GATT_ENCRYPTED_NO_MITM          = 0x878E,         /**< Encrypted No MITM */
    WICED_BT_GATT_NOT_ENCRYPTED              = 0x878F,         /**< Not Encrypted */
    WICED_BT_GATT_CONGESTED                  = 0x8791,         /**< Congested */
    WICED_BT_GATT_NOT_ALLOWED                = 0x8792,         /**< Operation not allowed */
    WICED_BT_GATT_HANDLED                    = 0x8793,         /**< Set by application to indicate it has responded to the message */
    WICED_BT_GATT_NO_PENDING_OPERATION       = 0x8794,         /**< No pending client operation for the response sent by app */
    WICED_BT_GATT_INDICATION_RESPONSE_PENDING= 0x8795,         /**< Indication response pending */
    WICED_BT_GATT_UNUSED2                    = 0x8796,         /**< Unused */
    WICED_BT_GATT_CCC_CFG_ERR                = 0x8797,         /**< Improper Client Char Configuration */
    WICED_BT_GATT_PRC_IN_PROGRESS            = 0x8798,         /**< Procedure Already in Progress */
    WICED_BT_GATT_UNUSED3                    = 0x8799,         /**< Unused */
    WICED_BT_GATT_BAD_OPCODE                 = 0x879A,         /**< Bad opcode */
    WICED_BT_GATT_NOT_IMPLEMENTED            = 0x879B,         /**< Not implemented */


    WICED_BT_GATT_INVALID_CONNECTION_ID      = 0xFFFF,         /**< Invalid connection id */
};
typedef uint16_t wiced_bt_gatt_status_t;     /**< GATT status (see #wiced_bt_gatt_status_e) */


/**
 * GATT Operation Codes.
 * All GATT_REQ_xxx are sent by the client and received on the server.
 * All GATT_RSP_xxx are sent by the server in response to the specific requests from client
 * All GATT_CMD_xxx are sent by client and received on the server. The server shall not send any response to the received GATT_CMD_xxx
 *
 * \p GATT_HANDLE_VALUE_NOTIF, \p GATT_HANDLE_VALUE_IND and \p GATT_HANDLE_VALUE_MULTI_NOTIF are sent by server
 * to notify/indicate changes to handle values on the server
 * \p GATT_HANDLE_VALUE_CONF is sent by the client in response to \p GATT_HANDLE_VALUE_IND
 *
 */
enum wiced_bt_gatt_opcode_e
{
    GATT_RSP_ERROR                  = 0x01,       /**< Error Response */
    GATT_REQ_MTU                    = 0x02,       /**< Exchange MTU Request */
    GATT_RSP_MTU                    = 0x03,       /**< Exchange MTU Response */
    GATT_REQ_FIND_INFO              = 0x04,       /**< Find Information Request */
    GATT_RSP_FIND_INFO              = 0x05,       /**< Find Information Response */
    GATT_REQ_FIND_TYPE_VALUE        = 0x06,       /**< Find By Type Value Request */
    GATT_RSP_FIND_TYPE_VALUE        = 0x07,       /**< Find By Type Value Response */
    GATT_REQ_READ_BY_TYPE           = 0x08,       /**< Read By Type Request */
    GATT_RSP_READ_BY_TYPE           = 0x09,       /**< Read By Type Response */
    GATT_REQ_READ                   = 0x0A,       /**< Read Request */
    GATT_RSP_READ                   = 0x0B,       /**< Read Response */
    GATT_REQ_READ_BLOB              = 0x0C,       /**< Read Blob Request */
    GATT_RSP_READ_BLOB              = 0x0D,       /**< Read Blob Response */
    GATT_REQ_READ_MULTI             = 0x0E,       /**< Read Multiple Request */
    GATT_RSP_READ_MULTI             = 0x0F,       /**< Read Multiple Response */
    GATT_REQ_READ_BY_GRP_TYPE       = 0x10,       /**< Read By Group Type Request */
    GATT_RSP_READ_BY_GRP_TYPE       = 0x11,       /**< Read By Group Type Response */
    GATT_REQ_WRITE                  = 0x12,       /**< Write Request */
    GATT_RSP_WRITE                  = 0x13,       /**< Write Response */
    GATT_REQ_PREPARE_WRITE          = 0x16,       /**< Prepare Write Request */
    GATT_RSP_PREPARE_WRITE          = 0x17,       /**< Prepare Write Response */
    GATT_REQ_EXECUTE_WRITE          = 0x18,       /**< Execute Write Request */
    GATT_RSP_EXECUTE_WRITE          = 0x19,       /**< Execute Write Response */
    GATT_HANDLE_VALUE_NOTIF         = 0x1B,       /**< Handle Value Notification */
    GATT_HANDLE_VALUE_IND           = 0x1D,       /**< Handle Value Indication */
    GATT_HANDLE_VALUE_CONF          = 0x1E,       /**< Handle Value Confirmation */
    GATT_REQ_READ_MULTI_VAR_LENGTH  = 0x20,       /**< Read Multiple Variable Length Request */
    GATT_RSP_READ_MULTI_VAR_LENGTH  = 0x21,       /**< Read Multiple Variable Length Response */
    GATT_HANDLE_VALUE_MULTI_NOTIF   = 0x23,       /**< Handle Value Multiple Notifications */

    GATT_CMD_WRITE                  = 0x52,       /**< Write Command */
    GATT_CMD_SIGNED_WRITE           = 0xD2,       /**< changed in V4.0 1101-0010 (signed write)  see write cmd above*/
};

typedef uint8_t wiced_bt_gatt_opcode_t;           /**< GATT Opcodes */

/**  GATT Disconnection reason */
enum wiced_bt_gatt_disconn_reason_e {
    GATT_CONN_UNKNOWN                       = 0,                                    /**< Unknown reason */
    GATT_CONN_L2C_FAILURE                   = 1,                                    /**< General L2cap failure  */
    GATT_CONN_TIMEOUT                       = HCI_ERR_CONNECTION_TOUT,              /**< Connection timeout  */
    GATT_CONN_TERMINATE_PEER_USER           = HCI_ERR_PEER_USER,                    /**< Connection terminated by peer user  */
    GATT_CONN_TERMINATE_LOCAL_HOST          = HCI_ERR_CONN_CAUSE_LOCAL_HOST,        /**< Connection terminated by local host  */
    GATT_CONN_FAIL_ESTABLISH                = HCI_ERR_CONN_FAILED_ESTABLISHMENT,    /**< Connection fail to establish  */
    GATT_CONN_LMP_TIMEOUT                   = HCI_ERR_LMP_RESPONSE_TIMEOUT,         /**< Connection fail due to LMP response tout */
    GATT_CONN_CANCEL                        = L2CAP_CONN_CANCEL                     /**< L2CAP connection cancelled  */
};
typedef uint16_t wiced_bt_gatt_disconn_reason_t;    /**< GATT disconnection reason (see #wiced_bt_gatt_disconn_reason_e) */

/** characteristic descriptor: client configuration value */
enum wiced_bt_gatt_client_char_config_e
{
    GATT_CLIENT_CONFIG_NONE          = 0x0000,      /**< Does not allow both notifications and indications */
    GATT_CLIENT_CONFIG_NOTIFICATION  = 0x0001,      /**< Allows notifications  */
    GATT_CLIENT_CONFIG_INDICATION    = 0x0002       /**< Allows indications  */
};
typedef uint16_t wiced_bt_gatt_client_char_config_t;     /**< GATT client config (see #wiced_bt_gatt_client_char_config_e) */

/** characteristic descriptor: server configuration value */
enum wiced_bt_gatt_server_char_config_e
{
    GATT_SERVER_CONFIG_NONE = 0x0000,     /**< No broadcast   */
    GATT_SERVER_CONFIG_BROADCAST = 0x0001 /**< Broadcast      */
};
typedef uint16_t wiced_bt_gatt_server_char_config_t; /**< GATT server config (see #wiced_bt_gatt_server_char_config_e) */

/**  GATT Characteristic Properties Mask */
enum wiced_bt_gatt_char_properties_e {
    GATT_CHAR_PROPERTIES_BIT_BROADCAST      = (1 << 0),     /**< bit 0: Broadcast */
    GATT_CHAR_PROPERTIES_BIT_READ           = (1 << 1),     /**< bit 1: Read */
    GATT_CHAR_PROPERTIES_BIT_WRITE_NR       = (1 << 2),     /**< bit 2: Write (No Response) */
    GATT_CHAR_PROPERTIES_BIT_WRITE          = (1 << 3),     /**< bit 3: Write */
    GATT_CHAR_PROPERTIES_BIT_NOTIFY         = (1 << 4),     /**< bit 4: Notify */
    GATT_CHAR_PROPERTIES_BIT_INDICATE       = (1 << 5),     /**< bit 5: Indicate */
    GATT_CHAR_PROPERTIES_BIT_AUTH           = (1 << 6),     /**< bit 6: Authenticate */
    GATT_CHAR_PROPERTIES_BIT_EXT_PROP       = (1 << 7)      /**< bit 7: Extended Properties */
};
typedef uint8_t wiced_bt_gatt_char_properties_t;            /**< GATT characteristic properties mask (see #wiced_bt_gatt_char_properties_e) */

/** Authentication requirement */
enum wiced_bt_gatt_auth_req_e {
    GATT_AUTH_REQ_NONE                  = 0,    /**< No Authentication Required */
    GATT_AUTH_REQ_NO_MITM               = 1,    /**< Unauthenticated encryption (No MITM) */
    GATT_AUTH_REQ_MITM                  = 2,    /**< Authenticated encryption (MITM) */
    GATT_AUTH_REQ_SIGNED_NO_MITM        = 3,    /**< Signed Data (No MITM) */
    GATT_AUTH_REQ_SIGNED_MITM           = 4     /**< Signed Data (MITM) */
};
typedef uint8_t wiced_bt_gatt_auth_req_t;   /**< GATT authentication requirement (see #wiced_bt_gatt_auth_req_e)*/


/** GATT Write Execute request flags */
enum wiced_bt_gatt_exec_flag_e {
    GATT_PREPARE_WRITE_CANCEL      = 0x00,         /**< GATT_PREP_WRITE_CANCEL */
    GATT_PREPARE_WRITE_EXEC        = 0x01          /**< GATT_PREP_WRITE_EXEC */
};

#define GATT_PREP_WRITE_CANCEL GATT_PREPARE_WRITE_CANCEL  /**< See #GATT_PREPARE_WRITE_CANCEL */
#define GATT_PREP_WRITE_EXEC   GATT_PREPARE_WRITE_EXEC    /**< See #GATT_PREPARE_WRITE_EXEC */
typedef uint8_t   wiced_bt_gatt_exec_flag_t;    /**< GATT execute flag (see #wiced_bt_gatt_exec_flag_e) */

/** Attribute read request */
/**< Opcode request, can be \ref GATT_REQ_READ, \ref GATT_REQ_READ_BLOB */
typedef struct
{
    uint16_t  handle;        /**< Handle of attribute to read */
    uint16_t  offset;        /**< Offset to read */
} wiced_bt_gatt_read_t;

/**
 * Response structure, containing the requested handle list for \ref GATT_RSP_READ_MULTI or \ref GATT_RSP_READ_MULTI_VAR_LENGTH
 * commands
 */
typedef struct wiced_bt_gatt_read_multiple_req_t_
{
    uint8_t *p_handle_stream;    /**< Stream containing the handles */
    int      num_handles;        /**< Number of handles pointed to by \p p_handle_stream */
} wiced_bt_gatt_read_multiple_req_t;

/**
 * Response structure for read multiple .
 */
typedef struct
{
    wiced_bt_gatt_opcode_t opcode;          /**< Response Opcode, can be \ref GATT_RSP_READ_MULTI or \ref GATT_RSP_READ_MULTI_VAR_LENGTH */
    wiced_bt_gatt_read_multiple_req_t req;  /**< The request parameters used by client in the \ref wiced_bt_gatt_client_send_read_multiple API */
    uint8_t *p_multi_rsp;                   /**< The response received from the peer.
                                                 The response is valid only for \ref GATT_RSP_READ_MULTI.
                                                 The peer response received for \ref GATT_RSP_READ_MULTI_VAR_LENGTH is split and sent to the app
                                                 using the \ref GATTC_OPTYPE_READ_HANDLE event.
                                            */
    uint16_t rsp_len;                       /**< Length of the data pointed to by \p p_multi_rsp */
} wiced_bt_gatt_read_multiple_rsp_t;

/** Parameters for GATT_READ_BY_TYPE and GATT_READ_CHAR_VALUE */
/**< Opcode request, can be \ref GATT_REQ_READ_MULTI, \ref GATT_REQ_READ_MULTI_VAR_LENGTH*/
typedef struct
{
    uint16_t               s_handle;       /**< Starting handle */
    uint16_t               e_handle;       /**< Ending handle */
    wiced_bt_uuid_t        uuid;           /**< uuid */
} wiced_bt_gatt_read_by_type_t;

/** Attribute header, used for GATT write operations, and read response callbacks */
typedef struct
{
    uint16_t                    handle;                     /**< Attribute handle */
    uint16_t                    offset;                     /**< Attribute value offset, ignored if not needed for a command */
    uint16_t                    len;                        /**< Length of attribute value */
    wiced_bt_gatt_auth_req_t    auth_req;                   /**< Authentication requirement (see @link wiced_bt_gatt_auth_req_e wiced_bt_gatt_auth_req_t @endlink)
                                                            * For most applications value is expected to be set to \ref GATT_AUTH_REQ_NONE
                                                            * Set value to \ref GATT_AUTH_REQ_SIGNED_NO_MITM or \ref GATT_AUTH_REQ_SIGNED_MITM if the data
                                                            * is required to be signed prior to sending over an unencrypted link.
                                                            * Refer BLUETOOTH CORE SPECIFICATION Version 5.3 | Vol 3, Part C, section 10.4
                                                            */
} wiced_bt_gatt_write_hdr_t;

/** Attribute write request */
typedef struct
{
    uint16_t      handle;     /**< Handle of attribute to write */
    uint16_t      offset;     /**< Offset to write */
    uint16_t      val_len;    /**< Value length */
    uint8_t       *p_val;     /**< Value pointer */
} wiced_bt_gatt_write_req_t;

/** Attribute handle confirmation, sent to app to indicate completion of server events on the server */
typedef struct
{
    uint16_t               handle;    /**< first handle on which the notification or indication was sent */
    wiced_bt_gatt_opcode_t opcode;    /**< \ref GATT_HANDLE_VALUE_NOTIF, \ref GATT_HANDLE_VALUE_IND, \ref GATT_HANDLE_VALUE_MULTI_NOTIF*/
} wiced_bt_gatt_req_conf_t;

/** Attribute handle execution write request received on the client */
typedef struct
{
    wiced_bt_gatt_exec_flag_t exec_write;   /**< GATT execute flag (see #wiced_bt_gatt_exec_flag_e) */
} wiced_bt_gatt_execute_write_req_t;

/** Attribute information for GATT attribute requests types received on the server */
typedef union
{
    wiced_bt_gatt_read_t              read_req;          /**< Parameters for \ref GATT_REQ_READ and \ref GATT_REQ_READ_BLOB */
    wiced_bt_gatt_read_multiple_req_t read_multiple_req; /**< Parameters for the \ref GATT_REQ_READ_MULTI and \ref GATT_REQ_READ_MULTI_VAR_LENGTH */
    wiced_bt_gatt_read_by_type_t      read_by_type;      /**< Parameters for \ref GATT_REQ_READ_BY_TYPE */
    wiced_bt_gatt_write_req_t         write_req;         /**< Parameters for \ref GATT_REQ_WRITE and \ref GATT_REQ_PREPARE_WRITE*/
    uint16_t                          remote_mtu;        /**< Parameters for \ref GATT_REQ_MTU */
    wiced_bt_gatt_req_conf_t          confirm;           /**< Parameters for \ref GATT_HANDLE_VALUE_CONF,
                                                              \ref GATT_HANDLE_VALUE_NOTIF, \ref GATT_HANDLE_VALUE_MULTI_NOTIF */
    wiced_bt_gatt_execute_write_req_t exec_write_req;    /**< Parameters for GATT_REQ_EXECUTE_WRITE */
} wiced_bt_gatt_request_params_t;

/** Discovery types */
enum wiced_bt_gatt_discovery_type_e
{
    GATT_DISCOVER_SERVICES_ALL = 1,             /**< discover all services */
    GATT_DISCOVER_SERVICES_BY_UUID,             /**< discover service by UUID */
    GATT_DISCOVER_INCLUDED_SERVICES,            /**< discover an included service within a service */
    GATT_DISCOVER_CHARACTERISTICS,              /**< discover characteristics of a service with/without type requirement */
    GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS,   /**< discover characteristic descriptors of a character */
    GATT_DISCOVER_MAX                           /**< maximum discovery types */
};
typedef uint8_t wiced_bt_gatt_discovery_type_t;    /**< GATT Discovery type (see #wiced_bt_gatt_discovery_type_e) */

/** Parameters used in a GATT Discovery */
typedef struct
{
    wiced_bt_uuid_t uuid;        /**< Service or Characteristic UUID */
    uint16_t        s_handle;    /**< Start handle for range to search */
    uint16_t        e_handle;    /**< End handle for range to search */
} wiced_bt_gatt_discovery_param_t;

/**  Response data for read operations */
typedef struct
{
    uint16_t handle;     /**< handle */
    uint16_t len;        /**< length of response data or notification or indication data.
                              In case of multiple notifications, it is total length of
                              notification tuples pointed by p_data */
    uint16_t offset;     /**< offset */
    uint8_t  *p_data;    /**< attribute data. In case of multiple notifications,
                              it is array of handle length and notification data tuples */
} wiced_bt_gatt_data_t;

/** Client Operation Complete response data */
typedef union
{
    wiced_bt_gatt_data_t    att_value;      /**< Response data for read operations (initiated using \ref wiced_bt_gatt_client_send_read_handle and
                                                 \ref wiced_bt_gatt_client_send_read_by_type )
                                                 (or) Notification or Indication data
                                                 */
    wiced_bt_gatt_read_multiple_rsp_t multi_rsp; /**< Data Response for \ref GATT_REQ_READ_MULTI  and
                                                 * \ref GATT_REQ_READ_MULTI_VAR_LENGTH
                                                 */
    uint16_t                mtu;            /**< Response data for configuration operations */
    uint16_t                handle;         /**< Response data for write operations (initiated using #wiced_bt_gatt_client_send_write) */
} wiced_bt_gatt_operation_complete_rsp_t;   /**< GATT client operation complete response type */

/** GATT client operation type, used in client callback function
*/
enum wiced_bt_gatt_optype_e
{
    GATTC_OPTYPE_NONE  = 0,               /**< None      */
    GATTC_OPTYPE_DISCOVERY,               /**< Discovery */
    GATTC_OPTYPE_READ_HANDLE,             /**< Read handle or Read blob */
    GATTC_OPTYPE_READ_BY_TYPE,            /**< Read by type operation   */
    GATTC_OPTYPE_READ_MULTIPLE,           /**< Read multiple, or read multiple var length */
    GATTC_OPTYPE_WRITE_WITH_RSP,          /**< Write with response */
    GATTC_OPTYPE_WRITE_NO_RSP,            /**< Write no response   */
    GATTC_OPTYPE_PREPARE_WRITE,           /**< Prepare Write */
    GATTC_OPTYPE_EXECUTE_WRITE,           /**< Execute Write */
    GATTC_OPTYPE_CONFIG_MTU,              /**< Configure MTU */
    GATTC_OPTYPE_NOTIFICATION,            /**< Notification */
    GATTC_OPTYPE_INDICATION,              /**< Indication */
};

/** GATT Client Operation Codes */
typedef uint8_t wiced_bt_gatt_optype_t; /**< GATT client operation type (see #wiced_bt_gatt_optype_e) */

/** GATT caching status of the peer(client) */
enum wiced_bt_gatt_caching_status_e
{
    GATT_PEER_CLIENT_CACHE_CHANGE_AWARE   = 0,     /**< Peer client is cache aware   */
    GATT_PEER_CLIENT_CACHE_CHANGE_UNAWARE = 1,     /**< Peer client is cache unaware */
    GATT_PEER_CLIENT_CACHE_READY_TO_BE_AWARE = 2   /**< Peer client is reading the database hash */
};

typedef uint8_t wiced_bt_gatt_caching_status_t; /**< GATT peer caching status (see #wiced_bt_gatt_caching_status_e) */

/** characteristic declaration */
typedef struct
{
    wiced_bt_gatt_char_properties_t characteristic_properties;  /**< characteristic properties (see @link wiced_bt_gatt_char_properties_e wiced_bt_gatt_char_properties_t @endlink) */
    uint16_t                        val_handle;                 /**< characteristic value attribute handle */
    uint16_t                        handle;                     /**< characteristic declaration handle */
    wiced_bt_uuid_t                 char_uuid;                  /**< characteristic UUID type */
} wiced_bt_gatt_char_declaration_t;

/** GATT group value */
typedef struct
{
    wiced_bt_uuid_t service_type;   /**< group type */
    uint16_t        s_handle;       /**< starting handle of the group */
    uint16_t        e_handle;       /**< ending handle of the group */
} wiced_bt_gatt_group_value_t;


/** included service attribute value */
typedef struct
{
    wiced_bt_uuid_t service_type;   /**< included service UUID */
    uint16_t        handle;         /**< included service handle */
    uint16_t        s_handle;       /**< included service starting handle */
    uint16_t        e_handle;       /**< included service ending handle */
} wiced_bt_gatt_included_service_t;

/** characteristic descriptor information */
typedef struct
{
    wiced_bt_uuid_t type;      /**< descriptor UUID type */
    uint16_t        handle;    /**< descriptor attribute handle */
} wiced_bt_gatt_char_descr_info_t;


/**
 * Discovery result data
 * Use  GATT_DISCOVERY_RESULT_SERVICE_* or GATT_DISCOVERY_RESULT_CHARACTERISTIC_* macros to parse discovery data)
 */
typedef union
{
    wiced_bt_gatt_included_service_t    included_service_type;      /**< Result for GATT_DISCOVER_INCLUDED_SERVICES */
    wiced_bt_gatt_group_value_t         group_value;                /**< Result for GATT_DISCOVER_SERVICES_ALL or GATT_DISCOVER_SERVICES_BY_UUID  */
    wiced_bt_gatt_char_declaration_t    characteristic_declaration; /**< Result for GATT_DISCOVER_CHARACTERISTICS */
    wiced_bt_gatt_char_descr_info_t     char_descr_info;            /**< Result for GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS */
} wiced_bt_gatt_discovery_data_t;


/*****************************************************************************
 *  GATT Database Defintions
 *****************************************************************************/
/** Attribute Permission bits (see Core Specification 5.2, Vol 3, Part F, 3.2.5) */
#define GATTDB_PERM_NONE                             (0x00)      /**<  No permissions set */
#define GATTDB_PERM_VARIABLE_LENGTH                  (0x1 << 0)  /**<  Attribute has variable length (not used by stack) */
#define GATTDB_PERM_READABLE                         (0x1 << 1)  /**<  Attribute is readable */
#define GATTDB_PERM_WRITE_CMD                        (0x1 << 2)  /**<  Attribute can be written using \ref GATT_CMD_WRITE */
#define GATTDB_PERM_WRITE_REQ                        (0x1 << 3)  /**<  Attribute can be written using \ref GATT_REQ_WRITE */
#define GATTDB_PERM_AUTH_READABLE                    (0x1 << 4)  /**<  Attribute can be read if the connection is encrypted or
                                                                       authenticated
                                                                       */
#define GATTDB_PERM_RELIABLE_WRITE                   (0x1 << 5)  /**<  Attribute supports reliable writes */
#define GATTDB_PERM_AUTH_WRITABLE                    (0x1 << 6)  /**<  Attribute can be written if the connection is encrypted or
                                                                       authenticated */

/**<  Permission mask for writable characteristics */
#define GATTDB_PERM_WRITABLE (GATTDB_PERM_WRITE_CMD | GATTDB_PERM_WRITE_REQ | GATTDB_PERM_AUTH_WRITABLE) /**<Writable permissions*/
#define GATTDB_PERM_MASK                             (0x7f)  /**< All the permission bits. */
#define GATTDB_PERM_SERVICE_UUID_128                 (0x1 << 7)  /**< Set for 128 bit services/characteristic UUIDs, check
                                                                       @ref GATT_DB_MACROS "Service and Characteristic macros"
                                                                      */

/**
 * Attribute permission bit masks.
 */
typedef uint8_t wiced_bt_gatt_permission_t;

#define GATTDB_CHAR_PROP_BROADCAST                  (0x1 << 0)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_READ                       (0x1 << 1)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_WRITE_NO_RESPONSE          (0x1 << 2)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_WRITE                      (0x1 << 3)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_NOTIFY                     (0x1 << 4)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_INDICATE                   (0x1 << 5)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_AUTHD_WRITES               (0x1 << 6)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */
#define GATTDB_CHAR_PROP_EXTENDED                   (0x1 << 7)  /**< GATT Characteristic Properties (see Vol 3, Part G, 3.3.1.1) */

#define GATTDB_CHAR_EXTENDED_PROP_RELIABLE             (0x1 << 0)  /**< GATT Characteristic Extended Properties (see Vol 3, Part G, 3.3.3.1) */
#define GATTDB_CHAR_EXTENDED_PROP_WRITABLE_AUXILIARIES (0x1 << 1)  /**< GATT Characteristic Extended Properties (see Vol 3, Part G, 3.3.3.1) */

/** Conversion macros */
#define BIT16_TO_8( val ) \
    (uint8_t)(  (val)        & 0xff),/* LSB */ \
    (uint8_t)(( (val) >> 8 ) & 0xff) /* MSB */

#define GATTDB_UUID16_SIZE                 2   /**< UUID lengths */
#define GATTDB_UUID128_SIZE                16  /**< UUID lengths */

/**
 * @anchor GATT_DB_MACROS
 * @name Service and Characteristic macros
 * @{
 */

/** Macro to assist 16 bit primary service declaration */
#define PRIMARY_SERVICE_UUID16(handle, service)  \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    4, \
    BIT16_TO_8((GATT_UUID_PRI_SERVICE)), \
    BIT16_TO_8((service))

/** Macro to assist 128 bit primary service declaration */
#define PRIMARY_SERVICE_UUID128(handle, service)  \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    18, \
    BIT16_TO_8(GATT_UUID_PRI_SERVICE), \
    service

/** Macro to assist 16 bit secondary service declaration */
#define SECONDARY_SERVICE_UUID16(handle, service)  \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    4, \
    BIT16_TO_8((GATT_UUID_SEC_SERVICE)), \
    BIT16_TO_8((service))

/** Macro to assist 128 bit secondary service declaration */
#define SECONDARY_SERVICE_UUID128(handle, service)  \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    18, \
    BIT16_TO_8(GATT_UUID_SEC_SERVICE), \
    service

/** Macro to assist included service declaration */
#define INCLUDE_SERVICE_UUID16(handle, service_handle, end_group_handle, service)  \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    8, \
    BIT16_TO_8(GATT_UUID_INCLUDE_SERVICE), \
    BIT16_TO_8(service_handle), \
    BIT16_TO_8(end_group_handle), \
    BIT16_TO_8(service)

/** Macro to assist 128 bit included service declaration */
#define INCLUDE_SERVICE_UUID128(handle, service_handle, end_group_handle)\
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    6, \
    BIT16_TO_8(GATT_UUID_INCLUDE_SERVICE), \
    BIT16_TO_8(service_handle), \
    BIT16_TO_8(end_group_handle)

/** Macro to assist readable 16 bit characteristic declaration */
#define CHARACTERISTIC_UUID16(handle, handle_value, uuid, properties, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    0x07, \
    BIT16_TO_8(GATT_UUID_CHAR_DECLARE), \
    (uint8_t)(properties), \
    BIT16_TO_8((uint16_t)(handle_value)), \
    BIT16_TO_8(uuid), \
    BIT16_TO_8((uint16_t)(handle_value)), \
    (uint8_t)(permission), \
    (uint8_t)(GATTDB_UUID16_SIZE), \
    BIT16_TO_8(uuid)

/** Macro to assist readable 128 bit characteristic declaration */
#define CHARACTERISTIC_UUID128(handle, handle_value, uuid, properties, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    21, \
    BIT16_TO_8(GATT_UUID_CHAR_DECLARE), \
    (uint8_t)(properties), \
    BIT16_TO_8((uint16_t)(handle_value)), \
    uuid, \
    BIT16_TO_8((uint16_t)(handle_value)), \
    (uint8_t)(permission | GATTDB_PERM_SERVICE_UUID_128), \
    (uint8_t)(GATTDB_UUID128_SIZE), \
    uuid

/** Macro to assist writable 16 bit characteristic declaration */
#define CHARACTERISTIC_UUID16_WRITABLE(handle, handle_value, uuid, properties, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    0x07, \
    BIT16_TO_8(GATT_UUID_CHAR_DECLARE), \
    (uint8_t)(properties), \
    BIT16_TO_8((uint16_t)(handle_value)), \
    BIT16_TO_8(uuid), \
    BIT16_TO_8((uint16_t)(handle_value)), \
    (uint8_t)(permission), \
    (uint8_t)(GATTDB_UUID16_SIZE), \
    (uint8_t)(0), \
    BIT16_TO_8(uuid)

/** Macro to assist writable 128 bit characteristic declaration */
#define CHARACTERISTIC_UUID128_WRITABLE(handle, handle_value, uuid, properties, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    GATTDB_PERM_READABLE, \
    21, \
    BIT16_TO_8(GATT_UUID_CHAR_DECLARE), \
    (uint8_t)(properties), \
    BIT16_TO_8((uint16_t)(handle_value)), \
    uuid, \
    BIT16_TO_8((uint16_t)(handle_value)), \
    (uint8_t)(permission | GATTDB_PERM_SERVICE_UUID_128), \
    (uint8_t)(GATTDB_UUID128_SIZE), \
    (uint8_t)(0), \
    uuid

/** Macro to assist writable 16 bit descriptor declaration */
#define CHAR_DESCRIPTOR_UUID16_WRITABLE(handle, uuid, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    (uint8_t)(permission), \
    (uint8_t)(GATTDB_UUID16_SIZE), \
    (uint8_t)(0), \
    BIT16_TO_8(uuid)

/** Macro to assist readable 16 bit descriptor declaration */
#define CHAR_DESCRIPTOR_UUID16(handle, uuid, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    (uint8_t)(permission), \
    (uint8_t)(GATTDB_UUID16_SIZE), \
    BIT16_TO_8(uuid)

/** Macro to assist writable 128 bit descriptor declaration */
#define CHAR_DESCRIPTOR_UUID128_WRITABLE(handle, uuid, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    (uint8_t)(permission | GATTDB_PERM_SERVICE_UUID_128), \
    (uint8_t)(GATTDB_UUID128_SIZE), \
    (uint8_t)(0), \
    uuid

/** Macro to assist readable 128 bit descriptor declaration */
#define CHAR_DESCRIPTOR_UUID128(handle, uuid, permission) \
    BIT16_TO_8((uint16_t)(handle)), \
    (uint8_t)(permission | GATTDB_PERM_SERVICE_UUID_128), \
    (uint8_t)(GATTDB_UUID128_SIZE), \
    uuid

/** Macro to assist extended properties declaration */
#define CHAR_DESCRIPTOR_EXTENDED_PROPERTIES(handle, ext_properties)\
    BIT16_TO_8((uint16_t)(handle)), \
    (uint8_t)(GATTDB_PERM_READABLE), \
    (uint8_t)(GATTDB_UUID16_SIZE + 1), \
    BIT16_TO_8(GATT_UUID_CHAR_EXT_PROP),\
    (uint8_t)(ext_properties)

/** @} GATT_DB_MACROS */

/**
* Format of the value of a characteristic.
* Enumeration types for the \sa UUID_DESCRIPTOR_CHARACTERISTIC_PRESENTATION_FORMAT descriptor
*/
enum wiced_bt_gatt_format_e
{
    GATT_CHAR_PRESENTATION_FORMAT_RES,            /* rfu */
    GATT_CHAR_PRESENTATION_FORMAT_BOOL,           /* 0x01 BOOL32 */
    GATT_CHAR_PRESENTATION_FORMAT_2BITS,          /* 0x02 2 bit */
    GATT_CHAR_PRESENTATION_FORMAT_NIBBLE,         /* 0x03 nibble */
    GATT_CHAR_PRESENTATION_FORMAT_UINT8,          /* 0x04 uint8 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT12,         /* 0x05 uint12 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT16,         /* 0x06 uint16 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT24,         /* 0x07 uint24 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT32,         /* 0x08 uint32 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT48,         /* 0x09 uint48 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT64,         /* 0x0a uint64 */
    GATT_CHAR_PRESENTATION_FORMAT_UINT128,        /* 0x0B uint128 */
    GATT_CHAR_PRESENTATION_FORMAT_SINT8,          /* 0x0C signed 8 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT12,         /* 0x0D signed 12 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT16,         /* 0x0E signed 16 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT24,         /* 0x0F signed 24 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT32,         /* 0x10 signed 32 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT48,         /* 0x11 signed 48 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT64,         /* 0x12 signed 64 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_SINT128,        /* 0x13 signed 128 bit integer */
    GATT_CHAR_PRESENTATION_FORMAT_FLOAT32,        /* 0x14 float 32 */
    GATT_CHAR_PRESENTATION_FORMAT_FLOAT64,        /* 0x15 float 64*/
    GATT_CHAR_PRESENTATION_FORMAT_SFLOAT,         /* 0x16 IEEE-11073 16 bit SFLOAT */
    GATT_CHAR_PRESENTATION_FORMAT_FLOAT,          /* 0x17 IEEE-11073 32 bit SFLOAT */
    GATT_CHAR_PRESENTATION_FORMAT_DUINT16,        /* 0x18 IEEE-20601 format */
    GATT_CHAR_PRESENTATION_FORMAT_UTF8S,          /* 0x19 UTF-8 string */
    GATT_CHAR_PRESENTATION_FORMAT_UTF16S,         /* 0x1a UTF-16 string */
    GATT_CHAR_PRESENTATION_FORMAT_STRUCT,         /* 0x1b Opaque structure*/
    GATT_CHAR_PRESENTATION_FORMAT_MAX             /* 0x1c or above reserved */
};
typedef uint8_t wiced_bt_gatt_format_t;/**< characteristic format specifiers (see #wiced_bt_gatt_format_e) */

/**
 * .Enumeration of known Client Supported Feature Bit assignments of the \ref wiced_bt_gatt_client_supported_features_t
 */
enum t_gatt_csf_assignments {
    GATT_CSF_ROBUST_CACHING = 0, /**< Client supports Robust Caching */
    GATT_CSF_EATT = 1,           /**< Client supports Enhanaced ATT bearers */
    GATT_CSF_MULTIPLE_HANDLE_VALUE_NOTIFICATIONS = 2 /**< Client supports receiving multiple handle value notifications */
};

/** macro to determine GATT Client Support features */
#define GATT_IS_CSF_FEATURE_SUPPORTED(csf, m) (csf[(m)/8] & (1 << (m)%8))

/** GATT Client Support features */
typedef uint8_t wiced_bt_gatt_csf_bits_t;

/**< Data Receive Buffer
 *  Data Receive Buffer or DRB is a memory area of type #tDRB and size(channel MTU size + #DRB_OVERHEAD_SIZE).
 * The DRB buffer is allocated by the application during channel creation.
 * The allocated DRBs are passed to the GATT layer when establishing an EATT connection.
 *       a) As a connection initiator with \ref wiced_bt_gatt_eatt_connect
 *       b) As a connection responder with \ref wiced_bt_gatt_eatt_connection_response_t
 * Application has to allocate one DRB per GATT bearer of size equal to that of desired local MTU.
*/
typedef tDRB * wiced_bt_eatt_drbs[EATT_CHANNELS_PER_TRANSACTION];                /**< list of Data Receive Blocks */

/** Discovery result (used by GATT_DISCOVERY_RESULT_EVT notification) */
typedef struct
{
    uint16_t                        conn_id;            /**< ID of the connection */
    wiced_bt_gatt_discovery_type_t  discovery_type;     /**< Discovery type (see @link wiced_bt_gatt_discovery_type_e wiced_bt_gatt_discovery_type_t @endlink) */
    wiced_bt_gatt_discovery_data_t  discovery_data;     /**< Discovery data  */
} wiced_bt_gatt_discovery_result_t;

/** Discovery Complete (used by GATT_DISCOVERY_CPLT_EVT notification) */
typedef struct
{
    uint16_t                        conn_id;         /**< ID of the connection */
    wiced_bt_gatt_discovery_type_t  discovery_type;  /**< Discovery type (see @link wiced_bt_gatt_discovery_type_e wiced_bt_gatt_discovery_type_t @endlink) */
    wiced_bt_gatt_status_t          status;          /**< Status of the discovery operation */
} wiced_bt_gatt_discovery_complete_t;


/** Response to read/write/disc/config operations (used by GATT_OPERATION_CPLT_EVT notification) */
typedef struct
{
    uint16_t                                conn_id;            /**< ID of the connection */
    wiced_bt_gatt_optype_t                  op;                 /**< Type of operation completed (see @link wiced_bt_gatt_optype_e wiced_bt_gatt_optype_t @endlink) */
    wiced_bt_gatt_status_t                  status;             /**< Status of operation  */
    uint8_t                                 pending_events;     /**< Number of pending events, used to initiate next read */
    wiced_bt_gatt_operation_complete_rsp_t  response_data;      /**< Response data (dependent on optype) */
} wiced_bt_gatt_operation_complete_t;

/** GATT connection status (used by GATT_CONNECTION_STATUS_EVT notification) */
typedef struct
{
    uint8_t                        *bd_addr;     /**< Remote device address */
    wiced_bt_ble_address_type_t     addr_type;   /**< Remmote device address type */
    uint16_t                        conn_id;     /**< ID of the connection */
    wiced_bool_t                    connected;   /**< TRUE if connected, FALSE if disconnected */
    wiced_bt_gatt_disconn_reason_t  reason;      /**< Reason code (see @link wiced_bt_gatt_disconn_reason_e wiced_bt_gatt_disconn_reason_t @endlink) */
    wiced_bt_transport_t            transport;   /**< Transport type of the connection */
    wiced_bt_dev_role_t             link_role;   /**< Link role on this connection */
} wiced_bt_gatt_connection_status_t;

/** GATT attribute request (used by GATT_ATTRIBUTE_REQUEST_EVT notification) */
typedef struct
{
    uint16_t                        conn_id;       /**< ID of the connection */
    uint16_t                        len_requested; /**< Max size of data requested as response */
    wiced_bt_gatt_opcode_t          opcode;        /**< GATT request opcode */
    wiced_bt_gatt_request_params_t  data;          /**< Information about attribute being request (dependent on request type) */
} wiced_bt_gatt_attribute_request_t;

/** GATT channel congestion/uncongestion (used by GATT_CONGESTION_EVT notification) */
typedef struct
{
    uint16_t       conn_id;    /**< ID of the connection */
    wiced_bool_t   congested;  /**< congestion state */
} wiced_bt_gatt_congestion_event_t;

/**
 * App context is returned back to application in event GATT_APP_BUFFER_TRANSMITTED_EVT on transmission of
 * application data buffer
 *
*/
typedef void * wiced_bt_gatt_app_context_t;

/**
 * Structure to return the application buffer and application buffer context given to the stack in various APIs
 * The application is expected to use this event to free allocated memory used for sending client requests or
 * responding to client requests.
 *
 * On client this is the buffer sent to the stack in
 *     - \sa wiced_bt_gatt_client_send_write
 * On server this is the buffer sent to the stack in
 *     - \sa GATT_GET_RESPONSE_BUFFER_EVT using \ref wiced_bt_gatt_app_response_buffer_t
 *     - \sa wiced_bt_gatt_server_send_indication
 *     - \sa wiced_bt_gatt_server_send_notification
 *     - \sa wiced_bt_gatt_server_send_multiple_notifications
 *     - \sa wiced_bt_gatt_server_send_read_handle_rsp
 *     - \sa wiced_bt_gatt_server_send_read_by_type_rsp
 *     - \sa wiced_bt_gatt_server_send_read_multiple_rsp
 *     - \sa wiced_bt_gatt_server_send_prepare_write_rsp
 *
 * @note Typically the application context is expected to be a function/hint (used by the application)
 * to allow it to free/deallocate the memory passed to the stack when writing data to the stack.
 * For e.g, when issueing wiced_bt_gatt_client_send_write, the app can allocate p_app_write_buffer and set the
 * p_app_ctxt to the appropriate free function. This memory allocated for this transaction has to be freed
 * when returned by the stack in the GATT_APP_BUFFER_TRANSMITTED_EVT.
 * The process is made simple since in this case as
 *  \p p_app_data is the application p_app_write_buffer and
 *  \p p_app_ctxt is the application free function/hint
 * @note \ref GATT_APP_BUFFER_TRANSMITTED_EVT is expected to be used only to free app memory as described. The
 * next transaction should be attempted on the respective \ref GATT_OPERATION_CPLT_EVT
 */
typedef struct
{
    uint8_t *p_app_data;                    /**< Application data buffer    */
    wiced_bt_gatt_app_context_t p_app_ctxt;  /**< Application context for \p p_app_data */
} wiced_bt_gatt_buffer_transmitted_t;


/**
 * Structure to hold the response buffer and the application context for that buffer.
 * Application fills the structure members with a pointer to send a server response.
 *
 * @note The application may free/release/deallocate the \p p_app_rsp_buffer pointer
 * on receiving a GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_rsp_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_app_ctxt
 *
 */
typedef struct
{
    uint8_t                    *p_app_rsp_buffer;   /**< Pointer to assign the buffer allocated by the app*/
    wiced_bt_gatt_app_context_t p_app_ctxt;         /**< Application context for \p p_app_rsp_buffer*/
} wiced_bt_gatt_app_response_buffer_t;

/**
 * Structure to get a response buffer of the \p len_requested and the application context for that buffer
 */
typedef struct
{
    uint16_t len_requested;                      /**< Size of the requested buffer to be returned in \p p_buffer */
    wiced_bt_gatt_app_response_buffer_t buffer;  /**< Structure to be filled by the app to return the allocated buffer */
} wiced_bt_gatt_buffer_request_t;


/** Structures for GATT event notifications */
typedef union
{
    wiced_bt_gatt_discovery_result_t        discovery_result;   /**< Data for GATT_DISCOVERY_RESULT_EVT */
    wiced_bt_gatt_discovery_complete_t      discovery_complete; /**< Data for GATT_DISCOVERY_CPLT_EVT */
    wiced_bt_gatt_operation_complete_t      operation_complete; /**< Data for GATT_OPERATION_CPLT_EVT */
    wiced_bt_gatt_connection_status_t       connection_status;  /**< Data for GATT_CONNECTION_STATUS_EVT */
    wiced_bt_gatt_attribute_request_t       attribute_request;  /**< Data for GATT_ATTRIBUTE_REQUEST_EVT */
    wiced_bt_gatt_congestion_event_t        congestion;         /**< Data for GATT_CONGESTION_EVT */
    wiced_bt_gatt_buffer_request_t          buffer_request;     /**< Data for GATT_GET_RESPONSE_BUFFER_EVT */
    wiced_bt_gatt_buffer_transmitted_t      buffer_xmitted;     /**< Data for GATT_APP_BUFFER_TRANSMITTED_EVT */
} wiced_bt_gatt_event_data_t;

/** GATT events */
typedef enum
{
    /**
     * GATT connection status change.
     * Event data: \ref wiced_bt_gatt_event_data_t.connection_status
     */
    GATT_CONNECTION_STATUS_EVT,        /* 0, 0x0*/

    /**
     * GATT client events, indication completion of app initiated client operations
     * Check specific client APIs for more details.
     * Applications can initiate the next client operation for the specific ATT bearer
     * on receiving this event.
     * Event data: \ref wiced_bt_gatt_event_data_t.operation_complete
     */
    GATT_OPERATION_CPLT_EVT,           /* 1, 0x1 */

    /**
     * GATT attribute discovery result.
     * Event data: \ref wiced_bt_gatt_event_data_t.discovery_result
     */
    GATT_DISCOVERY_RESULT_EVT,         /* 2, 0x2 */

    /**
     * GATT attribute discovery complete.
     * Event data: \ref wiced_bt_gatt_event_data_t.discovery_complete
     */
    GATT_DISCOVERY_CPLT_EVT,           /* 3, 0x3 */

    /**
     * GATT attribute request (from remote client).
     * Event data: \ref wiced_bt_gatt_event_data_t.attribute_request
     */
    GATT_ATTRIBUTE_REQUEST_EVT,        /* 4, 0x4 */

    /**
     * GATT congestion (running low in tx buffers).
     * Event data: \ref wiced_bt_gatt_event_data_t.congestion
     * @note Handling #GATT_CONGESTION_EVT
     * Applications may receive a GATT_CONGESTION_EVT to indicate a congestion
     * at the GATT layer.
     * On reception of this event with the #wiced_bt_gatt_congestion_event_t.congested variable set to #WICED_TRUE
     * the application should not attempt to send any further requests or commands to the GATT layer
     * as these will return with error code of #WICED_BT_GATT_CONGESTED
     * On reception of the #GATT_CONGESTION_EVT with the
     * #wiced_bt_gatt_congestion_event_t.congested variable set to #WICED_FALSE
     * the application may resume sending any further APIs
     */
    GATT_CONGESTION_EVT,               /* 5, 0x5 */

    /**
     * GATT buffer request, typically sized to max of bearer mtu - 1,
     * Event data: \ref wiced_bt_gatt_event_data_t.buffer_request
     */
    GATT_GET_RESPONSE_BUFFER_EVT,      /* 6, 0x6 */

    /**
    * GATT buffer transmitted event, indicates that the data in
    * \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data has been transmitted and may
    * be released/freed by the application using the application provided context in
    * \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt
    *
    * @note: For all cases where data is written or responded by the application this
    * event is received when the data is transferred over to the controller, except in
    * case of \ref GATT_REQ_PREPARE_WRITE where the event is sent after receiving a confirm
    * or error from the remote side
    *
    * Event data: \ref wiced_bt_gatt_event_data_t.buffer_xmitted
    */
    GATT_APP_BUFFER_TRANSMITTED_EVT,   /* 7, 0x7 */
} wiced_bt_gatt_evt_t;



/**
 * GATT event notification callback
 *
 * Callback for GATT event notifications
 * Registered using wiced_bt_gatt_register()
 *
 * @param[in] event             : Event ID
 * @param[in] p_event_data      : Event data
 *
 * @return Status of event handling
*/
typedef wiced_bt_gatt_status_t wiced_bt_gatt_cback_t(wiced_bt_gatt_evt_t event,
    wiced_bt_gatt_event_data_t *p_event_data);

/** GATT attribute value included in central role DB*/
typedef union
{
    uint8_t addr_resolution;  /**< binary value to indicate if addr. resolution is supported */
} wiced_bt_gatt_gap_ble_attr_value_t;

#pragma pack(1)
/**
 * Structure used by wiced_bt_gattdb APIS, to parse GATTDB
 */
typedef WICED_BT_STRUCT_PACKED wiced_gattdb_entry_s
{
    uint16_t handle;        /**< attribute Handle  */
    uint8_t  perm;          /**< attribute permission.*/
    uint8_t  len;           /**< attribute length . It excludes the header.*/
} wiced_gattdb_entry_t;
#pragma pack()

/** GATT connection request (used by GATT_EATT_CONNECTION_INDICATION_EVT notification)
* To be returned by application for accepting/rejecting the connection with the
* wiced_bt_eatt_connection_response
*/
typedef struct
{
    wiced_bt_device_address_t  bdaddr;                 /**< Bluetooth address of remote device */
    wiced_bt_transport_t       transport;              /**< Transport type of the connection */
    wiced_bt_ecrb_cid_list_t    lcids;                  /**< List of the l2cap cids (channel ids) */
    uint16_t                   mtu;                    /**< Peer MTU */
    uint8_t                    trans_id;               /**< Transaction id for the connection */
} wiced_bt_gatt_eatt_connection_indication_event_t;

/**
 * @brief structure to be used to respond to received EATT connection request
 *
 */
typedef struct
{
    wiced_bt_gatt_status_t response;    /**< Application response to be sent for the connection request */
    uint16_t               our_rx_mtu;  /**< Application MTU (Maximum Transmission Unit) to be used for the connection */
    uint16_t               our_rx_mps;  /**< Application MPS (Maximum Protocol Size) to be used for the connection */
    wiced_bt_eatt_drbs     ppDRBs;      /**< Data Receive Buffers of our_rx_mtu size to be used for receiving incoming data */
} wiced_bt_gatt_eatt_connection_response_t;

/**
* @brief structure used to deliver the confirmation status of the requested EATT connection
*/
typedef struct
{
    wiced_bt_device_address_t  bdaddr;     /**< Bluetooth address of remote device */
    wiced_bt_transport_t       transport;  /**< Transport type of the connection */
    uint16_t                   conn_id;    /**< conn_id of the connection */
    uint16_t                   mtu;        /**< Peer MTU */
    uint16_t                   result;     /**< Result of the connection for the conn_id in
                                           * \ref wiced_bt_gatt_eatt_connection_confirmation_event_t.conn_id
                                           */
} wiced_bt_gatt_eatt_connection_confirmation_event_t;


/**
 * @brief Function callbacks for EATT.
 */
typedef void (*wiced_bt_gatt_eatt_on_connect_ind_t)(wiced_bt_gatt_eatt_connection_indication_event_t *p_ind);   /**< callback upon GATT EATT connection */
typedef void (*wiced_bt_gatt_eatt_on_connect_complete_t)(wiced_bt_gatt_eatt_connection_confirmation_event_t * p_cfm); /**< callback upon GATT EATT connection complete */
typedef void (*wiced_bt_gatt_eatt_on_reconfigure_ind_t)(uint16_t conn_id, uint16_t mtu, uint16_t mps);  /**< callback upon GATT EATT reconnection complete */
typedef void (*wiced_bt_gatt_eatt_release_drb_t)(tDRB *p_drb);   /**< callback upon releasing the DRB */

/** callbacks for GATT EATT event notifications */
typedef struct
{
    wiced_bt_gatt_eatt_on_connect_ind_t      eatt_connect_ind_cb;     /**< callback upon GATT EATT connection */
    wiced_bt_gatt_eatt_on_connect_complete_t eatt_connect_cmpl_cb;    /**< callback upon GATT EATT connection complete */
    wiced_bt_gatt_eatt_on_reconfigure_ind_t  eatt_reconfigure_ind_cb; /**< callback upon GATT EATT reconnection complete */
    wiced_bt_gatt_eatt_release_drb_t         eatt_release_drb;        /**< callback upon releasing the DRB */
} wiced_bt_gatt_eatt_callbacks_t;


/**
 * @}
 */
/*****************************************************************************
 *  External Function Declarations
 ****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Generic Attribute (GATT) Functions.
 * The Generic Attribute Profile (GATT) defines a service framework which enables
 * Bluetooth low energy applications to configure themselves as a client or server device.
 *
 * The profile also provides the capability to perform discovery of services, read, write,
 * notification and indication of characteristics defined on a server.
 *
 * @addtogroup wicedbt_gatt Generic Attribute (GATT)
 * @ingroup wicedbt
 */

/**
 * BLE (Bluetooth Low Energy) Specific functions.
 *
 * @if DUAL_MODE
 * @addtogroup gatt_le BLE (Bluetooth Low Energy)
 * @ingroup gatt_common_api
 * @endif
 */

/**
 *  BR/EDR (Bluetooth Basic Rate / Enhanced Data Rate) Specific functions.
 *
 *  @if DUAL_MODE
 *  @addtogroup gatt_br BR/EDR (Bluetooth Basic Rate / Enhanced Data Rate)
 *  @ingroup gatt_common_api
 *  @endif
 */

/**
 * GATT Profile Module Init
 *
 *  @addtogroup  gatt_module_init       GATT Module Initialization
 *  @ingroup wicedbt_gatt
 *
 *  <b> GATT Module Initialization Functions </b> sub module for @b GATT.
 */

/**
 * Enables GATT database hashing calculations on the server.
 * Required to called for servers which need support for robust caching.
 * @note: This API should be called prior to calling \ref wiced_bt_gatt_db_init,
 * with an non-NULL \p wiced_bt_gatt_db_init.hash parameter
 *
 *  @return \ref wiced_result_t
 *  @ingroup gatt_module_init
 */
wiced_result_t wiced_bt_gatt_server_enable_caching(void);

/**
 * Enables code for enabling and checking data signing on the server and client
 *
 *  @return \ref wiced_result_t
 *  @ingroup gatt_module_init
 */
wiced_result_t wiced_bt_gatt_server_enable_signing(void);

/**
 * GATT Profile Server Functions
 *
 *  @addtogroup  gatt_server_api_functions       Server API
 *  @ingroup wicedbt_gatt
 *
 *  <b> Server API Functions </b> sub module for @b GATT.
 */

/**
 *  GATT Server Data API
 *
 *  @addtogroup  gattsr_api_functions GATT Server Data API
 *  @ingroup gatt_server_api_functions
 *
 */

/**
 * This API will send a long (1 upto (MTU -3) bytes) indication to the client for the specified
 *  handle with a persistent buffer in \p p_app_buffer.
 * The indication confirmation from the remote is indicated by an GATT_ATTRIBUTE_REQUEST_EVT
 * with wiced_bt_gatt_attribute_request_t.opcode = GATT_HANDLE_VALUE_IND
 *
 *  @param[in]  conn_id      : connection identifier.
 *  @param[in]  attr_handle  : Attribute handle of this handle value indication.
 *  @param[in]  val_len      : Length of indication value passed.
 *  @param[in]  p_app_buffer : Indication Value, peristent till the data is sent out to the controller
 *  @param[in]  p_app_ctxt   : Application context for \p p_app_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_buffer pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_app_ctxt
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gattsr_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_indication(uint16_t conn_id,
    uint16_t attr_handle, uint16_t val_len, uint8_t *p_app_buffer,
    wiced_bt_gatt_app_context_t p_app_ctxt);

/**
 * This API will send a long (1 upto (MTU -3) bytes) notification to the client for the
 * specified handle with a persistent buffer in \p p_app_buffer.
 * The send complete of the notification is indicated by an GATT_ATTRIBUTE_REQUEST_EVT
 * with wiced_bt_gatt_attribute_request_t.opcode = GATT_HANDLE_VALUE_NOTIF
 *
 *  @param[in]  conn_id      : connection identifier.
 *  @param[in]  attr_handle  : Attribute handle of this handle value notification.
 *  @param[in]  val_len      : Length of notification value passed.
 *  @param[in]  p_app_buffer : Notification Value, peristent till the data is sent out to the controller
 *  @param[in]  p_app_ctxt   : Application context for \p p_app_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_buffer pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_app_ctxt
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gattsr_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_notification(uint16_t conn_id, uint16_t attr_handle,
    uint16_t val_len, uint8_t *p_app_buffer, wiced_bt_gatt_app_context_t p_app_ctxt);

/**
 * This API will send a long (1 upto (MTU -1) bytes) multiple variable length notification to
 * the client with a persistent buffer in \p p_app_buffer.
 * The send complete of the notification is indicated by an GATT_ATTRIBUTE_REQUEST_EVT with
 * wiced_bt_gatt_attribute_request_t.opcode = GATT_HANDLE_VALUE_MULTI_NOTIF
 *
 *  @param[in]  conn_id        : connection identifier.
 *  @param[in]  app_buffer_len : Length of multiple notification values passed, should not exceed MTU - 1.
 *  @param[in]  p_app_buffer   : Notification Values, peristent till the data is sent out to the controller
 *                               @note: Notification values are formatted by the application by setting the
 *                                      handle(2 octets LE), len(2 octets LE) and data of handle of length for
 *                                      atleast 2 handle, len, data pairs.
 *  @param[in]  p_app_ctxt     : Application context for \p p_app_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_buffer pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_app_ctxt
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gattsr_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_multiple_notifications(uint16_t conn_id,
    uint16_t app_buffer_len, uint8_t *p_app_buffer, wiced_bt_gatt_app_context_t p_app_ctxt);

/**
 *  GATT Database Access Functions
 *
 *  @addtogroup  gattdb_api_functions GATT Database
 *  @ingroup gatt_server_api_functions
 *
 */

/**
 * Initialize the GATT database
 * (Please refer @ref GATT_DB_MACROS "Service and Characteristic macros" for
 * MACRO's to create or add entries to GATT database)
 *
 * @param[in]   p_gatt_db       : First element in new GATT database array
 * @param[in]   gatt_db_size    : Size (in bytes) of GATT database
 * @param[out]  hash            : The calculated database hash value. The hash pointer passed to
 *                                this function shall be NULL incase the application does not support
 *                                dynamic databases and does not support database caching
 *
 * @note GATT server apps which support Robust Caching need to invoke \sa wiced_bt_gatt_server_enable_caching
 * prior to invoking wiced_bt_gatt_db_init
 * @note In case \sa wiced_bt_gatt_server_enable_caching is not invoked, \p hash has to be set to NULL
 *
 * @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gattdb_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_db_init (const uint8_t *p_gatt_db, uint16_t gatt_db_size, wiced_bt_db_hash_t hash);

/**
 * Add a service module to the database.
 * Service modules define the complete service handles (i.e Service, characteristics,
 * descriptor, included service declaration handles)
 * The handle range in the service modules have to be distinct, i.e, cannot overlap
 * with any of the existing service modules in the database
 * @note Set the \ref wiced_bt_cfg_gatt_t.max_db_service_modules to the number of additional
 * services to be added
 *
 * @param[in]   p_gatt_db       : First element in GATT database array
 * @param[in]   gatt_db_size    : Size (in bytes) of GATT database
 * @param[out]  hash            : The calculated database hash value. The hash pointer passed
 *                                to this function can be NULL incase the application does not
 *                                support dynamic databases and does not support database caching
 *
 * @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gattdb_api_functions
 */

wiced_bt_gatt_status_t wiced_bt_gatt_add_services_to_db(const uint8_t* p_gatt_db,
    uint16_t gatt_db_size, wiced_bt_db_hash_t hash);

/**
 * Remove the service module from the database.
 *
 * @param[in]   p_gatt_db       : First element in GATT database array
 * @param[out]  hash            : The calculated database hash value. The hash pointer passed
 *                                to this function can be NULL incase the application does not
 *                                support dynamic databases and does not support database caching
 *
 * @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gattdb_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_db_remove_services_from_db(const uint8_t* p_gatt_db,
    wiced_bt_db_hash_t hash);

/**
 * @brief Server API to respond to a MTU request from the client
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] remote_mtu: The remote MTU received in \ref wiced_bt_gatt_request_params_t.remote_mtu
 * @param[in] my_mtu : The response MTU of the application
 *
 * @note: my_mtu shall be set to <= the configured pdu size in \ref wiced_bt_cfg_settings_t and shall
 * be >= \ref GATT_BLE_DEFAULT_MTU_SIZE
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_mtu_rsp(uint16_t conn_id, uint16_t remote_mtu, uint16_t my_mtu);

/**
 * @brief Server API to respond to a read request from the client
 * This API can be used to respond to \ref GATT_REQ_READ and \ref GATT_REQ_READ_BLOB request.
 * The application receivess a callback on the registered \ref wiced_bt_gatt_cback_t with the
 * \ref wiced_bt_gatt_attribute_request_t.opcode set to \ref GATT_REQ_READ or \ref GATT_REQ_READ_BLOB
 *
 * @param[in] conn_id    : GATT Connection ID
 * @param[in] opcode     : The opcode received in the event
 * @param[in] len        : Actual valid length pointed to by \p p_attr_rsp
 * @param[in] p_attr_rsp : The attribute handle \ref wiced_bt_gatt_read_t.handle data as requested,
 *                         at an offset of \ref wiced_bt_gatt_read_t.offset
 * @param[in] p_app_ctxt : Application context for \p p_attr_rsp
 *
 *  @note The application may free/release/deallocate the \p p_attr_rsp pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_attr_rsp
 *
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_read_handle_rsp(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode,
    uint16_t len,
    uint8_t *p_attr_rsp,
    wiced_bt_gatt_app_context_t p_app_ctxt);

/**
 * @brief Server API to respond to a read by type request from the client
 * This API can be used to respond to \ref GATT_REQ_READ_BY_TYPE
 * The application receivess a callback on the registered \ref wiced_bt_gatt_cback_t with the
 * \ref wiced_bt_gatt_attribute_request_t.opcode set to \ref GATT_REQ_READ_BY_TYPE
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] opcode : The opcode received in the event
 * @param[in] type_len: The length of the type in the response
 * @param[in] data_len : Actual valid length pointed to by \p p_app_rsp_buffer
 * @param[in] p_app_rsp_buffer : The formatted data response
 * @param[in] p_app_ctxt : Application context for \p p_app_rsp_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_rsp_buffer pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_rsp_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_app_ctxt
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_read_by_type_rsp (uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode,
    uint8_t type_len,
    uint16_t data_len, uint8_t*p_app_rsp_buffer, wiced_bt_gatt_app_context_t p_app_ctxt);

/**
 * @brief Server API to respond to read multiple request from the client
 * This API can be used to respond to \ref GATT_REQ_READ_MULTI or \ref GATT_REQ_READ_MULTI_VAR_LENGTH
 * The application receivess a callback on the registered \ref wiced_bt_gatt_cback_t with the
 * \ref wiced_bt_gatt_attribute_request_t.opcode set to \ref GATT_REQ_READ_MULTI or
 * \ref GATT_REQ_READ_MULTI_VAR_LENGTH
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] opcode : The opcode received in the event
 * @param[in] len : Actual valid length pointed to by \p p_app_rsp_buffer
 * @param[in] p_app_rsp_buffer : The formatted data response
 * @param[in] p_app_ctxt : Application context for \p p_app_rsp_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_rsp_buffer pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data = \p p_app_rsp_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt   = \p p_app_ctxt
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_read_multiple_rsp (uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode,
    uint16_t len, uint8_t* p_app_rsp_buffer, wiced_bt_gatt_app_context_t p_app_ctxt);

/**
 * @brief Server API to respond to a write request from the client
 * This API can be used to respond to \ref GATT_REQ_WRITE
 * The application receives a callback on the registered \ref wiced_bt_gatt_cback_t with the
 * \ref wiced_bt_gatt_attribute_request_t.opcode set to \ref GATT_REQ_WRITE or
 * \ref GATT_CMD_WRITE or \ref GATT_CMD_SIGNED_WRITE
 * The command parameters are in \ref wiced_bt_gatt_write_req_t
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] opcode : The opcode received in the event
 * @param[in] handle : The handle as received in the request
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_write_rsp(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode, uint16_t handle);

/**
 * @brief Server API to respond to a prepare write request from the client
 * This API can be used to respond to \ref GATT_REQ_PREPARE_WRITE
 * The application receives a callback on the registered \ref wiced_bt_gatt_cback_t with the
 * \ref wiced_bt_gatt_attribute_request_t.opcode set to \ref GATT_REQ_PREPARE_WRITE
 * The application is expected to queue up the writes till it receives a \ref GATT_REQ_EXECUTE_WRITE
 * The command parameters are in \ref wiced_bt_gatt_write_req_t
 *
 * The mechanism of prepare writes is only applicable for attributes which have set the
 * \ref GATTDB_PERM_RELIABLE_WRITE
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] opcode : The opcode received in the event
 * @param[in] handle : The handle as received in the request
 * @param[in] offset : The offset as received in the request
 * @param[in] len : The actual length of the \p p_data
 * @param[in] p_app_rsp_buffer: The data copied into the queue as received in the command.
 *  @note: \p p_app_rsp_buffer is a copy of the received data. The application should not
 *      send the received \ref wiced_bt_gatt_write_req_t.p_val pointer
 * @param[in] p_app_ctxt : Application context for \p p_app_rsp_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_rsp_buffer pointer
 *  on receiving a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data set to \p p_app_rsp_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt set to \p p_app_ctxt
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_prepare_write_rsp(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode,
    uint16_t handle, uint16_t offset,
    uint16_t len, uint8_t *p_app_rsp_buffer, wiced_bt_gatt_app_context_t p_app_ctxt);


/**
 * @brief Server API to respond to an execute write request from the client
 * The application receives a callback on the registered \ref wiced_bt_gatt_cback_t with the
 * \ref wiced_bt_gatt_attribute_request_t.opcode set to \ref GATT_REQ_EXECUTE_WRITE
 * The application is expected to write the prepare write queue on receiving this command.
 * The command parameters are in \ref wiced_bt_gatt_execute_write_req_t
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] opcode : The opcode received in the event
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_execute_write_rsp(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode);

/**
 * @brief Server API to send an error response to any of the received requests on
 * encountering an error in the command
 *
 * @param[in] conn_id : GATT Connection ID
 * @param[in] opcode : The opcode received in the event
 * @param[in] handle : The attribute handle as received in the event
 * @param[in] status : The reason code for the error
 *
 * @return wiced_bt_gatt_status_t
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_send_error_rsp(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode, uint16_t handle, wiced_bt_gatt_status_t status);


/**
 * @brief Utility function on the server to find a handle matching a uuid in a range
 *
 * @param[in] s_handle : Start handle of the range to search
 * @param[in] e_handle : End handle of the range to search
 * @param[in] p_uuid   : The attribute uuid to match
 *
 * @return wiced_bt_gatt_status_t
 */

uint16_t wiced_bt_gatt_find_handle_by_type(uint16_t s_handle, uint16_t e_handle, wiced_bt_uuid_t *p_uuid);

/**
 *  GATT Robust Caching  API
 *
 *  @addtogroup  gatt_robust_caching_api_functions GATT Robust Caching
 *  @ingroup gatt_server_api_functions
 *
 */

/**
 *
 * Set the remote client supported features upon write and for bonded device after
 *  reconnect as read from NVRAM
 *
 * @param[in]   conn_id : GATT Connection ID
 * @param[in]   csfs : Bit mask as received from the peer
 *
 * @return None
 * @ingroup gatt_robust_caching_api_functions
 */
void wiced_bt_gatt_set_client_supported_features(uint16_t conn_id,
    wiced_bt_gatt_client_supported_features_t csfs);

/**
 *
 * Get the remote client supported features
 *
 * @param[in]   conn_id : GATT Connection ID
 * @param[in]   csfs : Bit mask set in \ref wiced_bt_gatt_set_client_supported_features
 *
 * @return None
 * @ingroup gatt_robust_caching_api_functions
 */
void wiced_bt_gatt_get_client_supported_features(uint16_t conn_id,
    wiced_bt_gatt_client_supported_features_t csfs);


/**
 * Function     wiced_bt_gatt_get_peer_caching_status
 * Can be used by the server application to read the robust caching status of the peer.
 *
 * @param[in]       conn_id: connection identifier.
 * @param[out]      caching_status: see \ref wiced_bt_gatt_caching_status_t
 *
 * @return          None
 * @ingroup gatt_robust_caching_api_functions
 */
void wiced_bt_gatt_get_peer_caching_status(uint16_t conn_id, wiced_bt_gatt_caching_status_t* caching_status);

/**
 *
 * This function is used by the application to set peer's caching state.
 * The application is expected to store the last #wiced_bt_db_hash_t shared with the bonded peer in NVRAM
 * On connection, the application compares the stored #wiced_bt_db_hash_t with the current db_hash.
 * If the hashes match, the status is set to #GATT_PEER_CLIENT_CACHE_CHANGE_AWARE
 * else #GATT_PEER_CLIENT_CACHE_CHANGE_UNAWARE
 *
 * @param[in]       conn_id: connection identifier.
 * @param[in]       status: CHANGE_AWARE/CHANGE_UNAWARE.
 *
 * @return          None
 * @ingroup gatt_robust_caching_api_functions
 */
void wiced_bt_gatt_set_peer_caching_status(uint16_t conn_id, wiced_bt_gatt_caching_status_t status);

/**
 *  GATT Profile Client Functions
 *
 *  @addtogroup  gatt_client_api_functions   Client API
 *  @ingroup wicedbt_gatt
 */

/**
 * Configure the ATT MTU size for a connection on an LE transport.
 *
 *  @param[in]  conn_id             : GATT connection handle
 *  @param[in]  mtu                 : New MTU size
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions

 *  @note Allowed mtu range is 23 upto \ref wiced_bt_cfg_ble_t.ble_max_rx_pdu_size for
 *  BLE links as configured in #wiced_bt_cfg_settings_t
 *
*/
wiced_bt_gatt_status_t wiced_bt_gatt_client_configure_mtu (uint16_t conn_id, uint16_t mtu);

/**
 * Start an attribute discovery on an ATT server.
 * Discovery results are notified using <b> GATT_DISCOVERY_RESULT_EVT </b>;
 * completion is notified using <b> GATT_DISCOVERY_CPLT_EVT </b> of #wiced_bt_gatt_cback_t.
 *
 *  @param[in]  conn_id             : GATT connection handle
 *  @param[in]  discovery_type      : Discover type
 *  @param[in]  p_discovery_param   : Discover parameter
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_discover (uint16_t conn_id,
                                             wiced_bt_gatt_discovery_type_t discovery_type,
                                             wiced_bt_gatt_discovery_param_t *p_discovery_param );

/**
 * Read from remote ATT server.
 * Result is notified using <b> GATT_OPERATION_CPLT_EVT </b> of #wiced_bt_gatt_cback_t,
 * with \ref wiced_bt_gatt_operation_complete_t.op set to GATTC_OPTYPE_READ_HANDLE
 *
 *  @param[in]  conn_id     : Connection id
 *  @param[in]  handle      : Attribute handle to read
 *  @param[in]  offset      : Offset to start read from. To read the entire attribute set offset to 0.
 *      @note: In case the offset is set to 0, the stack first sends a GATT_REQ_READ and then sends out a
 *             series of GATT_REQ_READ_BLOB till the entire attribute is read or until the \p p_read_buf is
 *             filled upto \p len
 *  @param[out] p_read_buf : The buffer to save the read response
 *  @param[in]  len         : Length of the \p p_read_buf
 *  @param[in]  auth_req    : Authentication requirements
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
 *
 */
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_read_handle(uint16_t conn_id, uint16_t handle,
    uint16_t offset,
    uint8_t * p_read_buf,
    uint16_t len,
    wiced_bt_gatt_auth_req_t auth_req);

/**
 * Read by type from remote ATT server.
 * The handle information for the read by type request is received via
 * <b> GATT_OPERATION_CPLT_EVT </b> of #wiced_bt_gatt_cback_t with
 * \ref wiced_bt_gatt_operation_complete_t.op set to \ref GATTC_OPTYPE_READ_HANDLE
 * A non-zero \ref wiced_bt_gatt_operation_complete_t.pending_events indicates multiple handle responses
 * which may be returned from the server for some UUIDs
 *
 * The end of the read by type transaction is notified with result via
 * <b> GATT_OPERATION_CPLT_EVT </b> of #wiced_bt_gatt_cback_t with
 * \ref wiced_bt_gatt_operation_complete_t.op set to GATTC_OPTYPE_READ_BY_TYPE
 *
 *  @param[in]  conn_id     : Connection id
 *  @param[in] s_handle : Start handle of the range to search
 *  @param[in] e_handle : End handle of the range to search
 *  @param[in] p_uuid   : The attribute uuid to match
 *  @param[out] p_read_buf : The buffer to save the read response in case the first
 *                           response to the type does not fit into the available MTU.
 *  @param[in]  len         : Length of the \p p_read_buf
 *  @param[in]  auth_req    : Authentication requirements
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
 *
 */
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_read_by_type(uint16_t conn_id,
    uint16_t s_handle, uint16_t e_handle, wiced_bt_uuid_t *p_uuid,
    uint8_t *p_read_buf, uint16_t len,
    wiced_bt_gatt_auth_req_t auth_req);

/**
 * Read multiple from remote ATT server.
 * Result is notified using <b> GATT_OPERATION_CPLT_EVT </b> of #wiced_bt_gatt_cback_t, with
 * \ref wiced_bt_gatt_operation_complete_t.op
 * a) set to GATTC_OPTYPE_READ_MULTIPLE for \p opcode = GATT_REQ_READ_MULTI and
 * b) set to GATTC_OPTYPE_READ_HANDLE for \p opcode = GATT_REQ_READ_MULTI_VAR_LENGTH
 *
 *  @param[in] conn_id     : Connection id
 *  @param[in] opcode : Can be set to \ref GATT_REQ_READ_MULTI or \ref GATT_REQ_READ_MULTI_VAR_LENGTH
 *  @param[in] num_handles : Number of handles pointed to by \p p_handle_stream
 *  @param[in] p_handle_stream : The list of 16 bit handles arranged in Little Endian format
 *  @param[in] auth_req   : Authentication requirements
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
 *
 */
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_read_multiple(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode,
    int num_handles, uint8_t *p_handle_stream, wiced_bt_gatt_auth_req_t auth_req);

/**
 * Write to remote ATT server.
 * Result is notified using <b> GATT_OPERATION_CPLT_EVT </b> of #wiced_bt_gatt_cback_t.
 *
 *  @param[in] conn_id     : Connection handle
 *  @param[in] opcode      : Can be \ref GATT_REQ_WRITE, \ref GATT_CMD_WRITE, \ref GATT_REQ_PREPARE_WRITE
 *   @note: When opcode is set to \ref GATT_REQ_WRITE and the length to be written is greater than
 *          MTU - 3, then the stack splits the write into a series of \ref GATT_REQ_PREPARE_WRITE and finally
 *          sends a \ref GATT_REQ_EXECUTE_WRITE to complete the write.
 *  @param[in] p_hdr       : Pointer to the write parameters, with application data in \p p_app_write_buffer
 *  @param[in] p_app_write_buffer: Pointer to the application data buffer to be sent. The len of the buffer is in \p p_hdr->len
 *  @param[in] p_app_ctxt  : Application context for \p p_app_write_buffer
 *
 *  @note The application may free/release/deallocate the \p p_app_write_buffer pointer on receiving
 *    a \ref GATT_APP_BUFFER_TRANSMITTED_EVT with
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_data set to \p p_app_write_buffer and
 *   \ref wiced_bt_gatt_buffer_transmitted_t.p_app_ctxt set to \p p_app_ctxt
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
 */
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_write(uint16_t conn_id,
    wiced_bt_gatt_opcode_t opcode,
    wiced_bt_gatt_write_hdr_t *p_hdr, uint8_t *p_app_write_buffer,
    wiced_bt_gatt_app_context_t p_app_ctxt);


/**
 * Send Execute Write request to remote ATT server.
 *
 *  @param[in]  conn_id             : Connection handle
 *  @param[in]  is_execute          : <b>WICED_BT_TRUE </b> to execute, <b> WICED_BT_FALSE </b> to cancel
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
 *
 */
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_execute_write (uint16_t conn_id, wiced_bool_t is_execute);

/**
 * Send a handle value confirmation to remote ATT server.
 * in response to <b>GATTC_OPTYPE_INDICATION </b>  of #wiced_bt_gatt_cback_t
 *
 *  @param[in]  conn_id             : Connection handle
 *  @param[in]  handle              : Attribute handle
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *  @ingroup gatt_client_api_functions
*/
wiced_bt_gatt_status_t wiced_bt_gatt_client_send_indication_confirm (uint16_t conn_id, uint16_t handle);

/**
 *  GATT Profile Connection Functions
 *
 *  @addtogroup gatt_common_api Connection API
 *  @ingroup wicedbt_gatt
 *
 */

/**
 * Open GATT over LE connection to a remote device
 * Result is notified using <b> GATT_CONNECTION_STATUS_EVT </b> of #wiced_bt_gatt_cback_t.
 *
 *  @param[in]  bd_addr     : Remote device address
 *  @param[in]  bd_addr_type: Public or random address
 *  @param[in]  conn_mode   : connection scan mode
 *  @param[in]  is_direct   : Is direct connection or not
 *
 *  @return <b> TRUE </b>            : If connection started
 *          <b> FALSE </b>           : If connection start failure
 *
 * NOTE :  If is_direct = WICED_FALSE, it will create background connection.
 *         Default Background connection type is BTM_BLE_CONN_NONE.
 *         Before calling wiced_bt_gatt_le_connect please set background connection type (AUTO)
 *         using wiced_bt_ble_set_background_connection_type API
 *
 * @if DUAL_MODE
 * @ingroup gatt_le
 * @else
 * @ingroup gatt_common_api
 * @endif
 */
wiced_bool_t wiced_bt_gatt_le_connect (wiced_bt_device_address_t bd_addr,
                                    wiced_bt_ble_address_type_t bd_addr_type,
                                    wiced_bt_ble_conn_mode_t conn_mode,
                                    wiced_bool_t is_direct);
/**
 * Open GATT over BR/EDR connection to a remote device
 * Result is notified using <b> GATT_CONNECTION_STATUS_EVT </b> of #wiced_bt_gatt_cback_t.
 *
 *  @param[in]  bd_addr     : Remote device address
 *
 *  @return <b> TRUE </b>            : If connection started
 *          <b> FALSE </b>           : If connection start failure
 *
 *  @ingroup gatt_br
 */
wiced_bool_t wiced_bt_gatt_bredr_connect (wiced_bt_device_address_t bd_addr);

/**
 * Register an application callback for GATT.
 *
 *  @param[in]    p_gatt_cback      : The GATT notification callback
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *
 * @ingroup gatt_common_api
 */
wiced_bt_gatt_status_t wiced_bt_gatt_register (wiced_bt_gatt_cback_t *p_gatt_cback);

/**
 * Cancel initiating GATT connection
 *
 *  @param[in]  bd_addr     : Remote device address
 *  @param[in]  is_direct   : Is direct connection or not
 *
 *  @return <b> TRUE </b>            : If connection started
 *          <b> FALSE </b>           : If connection start failure
 *
 * @ingroup gatt_common_api
 */
wiced_bool_t wiced_bt_gatt_cancel_connect (wiced_bt_device_address_t bd_addr, wiced_bool_t is_direct);

/**
 * Close the specified GATT connection.
 * Result is notified using <b> GATT_CONNECTION_STATUS_EVT </b> of #wiced_bt_gatt_cback_t.
 *
 *  @param[in]   conn_id     : GATT connection ID
 *
 *  @return @link wiced_bt_gatt_status_e wiced_bt_gatt_status_t @endlink
 *
 * @ingroup gatt_common_api
 */
wiced_bt_gatt_status_t wiced_bt_gatt_disconnect (uint16_t conn_id);


/**
 * Start or stop LE advertisement and listen for connection.
 *
 *  @param[in]   start      : TRUE to add device to Filter Accept List / FALSE to remove
 *  @param[in]   bd_addr    : Device to add/remove from Filter Accept List
 *  @param[in]   type       : Address type of \p bd_addr
 *
 *  @return <b> TRUE </b>            : Success
 *          <b> FALSE </b>           : Failure
 *
 * @if DUAL_MODE
 * @ingroup gatt_le
 * @else
 * @ingroup gatt_common_api
 * @endif
 */
wiced_bool_t wiced_bt_gatt_listen (wiced_bool_t start, wiced_bt_device_address_t bd_addr,
    wiced_bt_ble_address_type_t type);

/**
 * EATT API
 *
 *  @addtogroup  gatt_eatt_functions EATT
 *  @ingroup gatt_common_api
 */

/**
 * Register an application callback for GATT.
 *
 *  @param[in]    p_gatt_cback      : The GATT notification callback
 *  @param[in]    max_eatt_bearers  : Total number of gatt bearers allowed across all devices (legacy + eatt)
 *
 *  @return Number of (legacy + eatt) bearers allocated
 *
 *  @ingroup gatt_eatt_functions
 */
uint32_t wiced_bt_gatt_eatt_register(wiced_bt_gatt_eatt_callbacks_t *p_gatt_cback, uint32_t max_eatt_bearers);

/**
 * API to create the enhanced gatt channels, using the legacy/unenhanced connection id.
 *
 *  @param[in]     legacy_conn_id : GATT DB characteristic handle
 *  @param[in]     mtu            : EATT bearer Maximum Transmission Unit
 *  @param[in]     mps            : EATT bearer Maximum PDU Payload Size
 *  @param[in]     num_bearers    : Number of EATT bearers to be established in this call
 *  @param[in]     ppDRBs         : Data Receive Buffers, each of MTU size for each of the
 *                                  bearers to be established
 *  @param[out]    conn_id_list_out : Connection Ids created for the EATT bearers
 *
 *  @returns #wiced_result_t
 *
 *  @ingroup gatt_eatt_functions
 */
wiced_result_t wiced_bt_gatt_eatt_connect(uint16_t legacy_conn_id, uint32_t mtu,
    uint32_t mps, uint32_t num_bearers,
    wiced_bt_eatt_drbs ppDRBs, wiced_bt_gatt_eatt_conn_id_list conn_id_list_out);

/**
 * API to create the respond to the enhanced gatt channels connection indication
 *
 *  @param[in]     p_indication   : #wiced_bt_gatt_eatt_connection_indication_event_t received
 *                                  in #wiced_bt_gatt_eatt_callbacks_t
 *  @param[in]     p_response     : #wiced_bt_gatt_eatt_connection_response_t response data sent
 *                                  by the receiver of #wiced_bt_gatt_eatt_callbacks_t
 *  @param[out]    conn_id_list_out : Connection Ids created for the EATT bearers
 *
 *  @returns characteristic descriptor handle
 *
 *  @ingroup gatt_eatt_functions
 */
wiced_result_t wiced_bt_gatt_eatt_connect_response(wiced_bt_gatt_eatt_connection_indication_event_t * p_indication,
    wiced_bt_gatt_eatt_connection_response_t * p_response,
    wiced_bt_gatt_eatt_conn_id_list conn_id_list_out);

/**
 * API the reconfigure the enhanced gatt channels
 *
 *  @param[in]     conn_id_list : #wiced_bt_gatt_eatt_conn_id_list to be reconfigured
 *  @param[in]     num_bearers  : Number of bearers in the conn_ids list
 *  @param[in]     mtu          : New MTU (Maximum Transmission Unit) value
 *  @param[in]     mps          : New MPS (Maximum Protocol Size) value
 *  @param[in]     ppDRBs       : New pointers to the DRBs, each of size MTU for each
 *                                of bearer in the conn_id_list
 *
 *  @returns #wiced_result_t
 *
 *  @ingroup gatt_eatt_functions
 */
wiced_result_t wiced_bt_gatt_eatt_reconfigure(wiced_bt_gatt_eatt_conn_id_list conn_id_list,
    uint32_t num_bearers, uint32_t mtu, uint32_t mps, wiced_bt_eatt_drbs ppDRBs);

/**
 * API to get the bluetooth device address of the connected gatt conn_id
 * @note : The API cannot be used to get the Bluetooth device address in case
 *  the device is disconnected
 *
 *  @param[in]  conn_id    : Connection handle of the gatt bearer
 *  @param[out] p_bdaddr   : #wiced_bt_device_address_t
 *  @param[out] p_transport: #wiced_bt_transport_t of the bearer
 *  @param[out] p_addr_type: #wiced_bt_ble_address_type_t of the bearer.
 *                           Valid only if device referred by the conn_id is of type \ref BT_TRANSPORT_LE
 *
 *  @returns #wiced_bt_gatt_status_t
 *
 *  @ingroup gatt_common_api
 */
wiced_bt_gatt_status_t wiced_bt_gatt_get_device_address(uint16_t conn_id,
    wiced_bt_device_address_t* p_bdaddr,
    wiced_bt_transport_t* p_transport, wiced_bt_ble_address_type_t* p_addr_type);

/**
 * API to validate connected gatt conn_id
 *
 *  @param[in]  conn_id    : Connection handle of the gatt bearer
 *
 *  @returns #wiced_bt_gatt_status_t
 *
 *  @ingroup gatt_common_api
 */
wiced_bt_gatt_status_t wiced_bt_gatt_validate_conn_id(uint16_t conn_id);


/**
 * @brief Utility function to compare UUIDs
 * @param[in] p_left : UUID to compare
 * @param[in] p_right : UUID to compare
 * @return WICED_TRUE if same, WICED_FALSE otherwise
*/
wiced_bool_t wiced_bt_is_same_uuid(const wiced_bt_uuid_t *p_left, const wiced_bt_uuid_t *p_right);

/**
 * @brief Utility function to get the configured ATT bearer mtu
 *
 * \param conn_id
 * \return MTU of the bearer
 */
uint16_t wiced_bt_gatt_get_bearer_mtu(uint16_t conn_id);

/**
 * @brief Utility function to check whether the attribute referred to by \p handle
 * has the permission to perform actions required by \p opcode received in the
 * attribute request from the peer
 *
 * @param[in] conn_id : Bearer Connection id
 * @param[in] handle : Attribute handle to check permission on
 * @param[in] opcode : Opcode received from peer.
 *
 * \return WICED_BT_GATT_SUCCESS if the \p perm_to_check is allowed on the \p handle,
 *  else the specific error
 */
wiced_bt_gatt_status_t wiced_bt_gatt_server_check_attribute_permission(uint16_t conn_id,
    uint16_t handle, wiced_bt_gatt_opcode_t opcode);

/**
 * @brief Utility function to copy a partial read by type response to the buffer
 *
 * @param[in] p_stream   : Pointer to the buffer to be filled
 * @param[in] stream_len : Writable length of buffer pointed to by \p p_stream
 * @param[out] p_pair_len : The value length for the read by type rsp.
 *           @note: (*p_pair_len) shall be initialized to 0 prior to the first call to this API
 * @param[in] attr_handle : Attribute handle to be filled
 * @param[in] attr_len : Attribute len to be filled
 * @param[in] p_attr : Attribute data
 *
 * \return length of data filled, 0 on error.
 */

int wiced_bt_gatt_put_read_by_type_rsp_in_stream(uint8_t *p_stream, int stream_len,
    uint8_t *p_pair_len, uint16_t attr_handle, uint16_t attr_len, const uint8_t *p_attr);


/**
 * @brief Utility function to put the handle, data into the multi handle response stream
 *
 * @param[in] opcode      : Can be either \ref GATT_REQ_READ_MULTI or \ref GATT_REQ_READ_MULTI_VAR_LENGTH
 * @param[in] p_stream    : Pointer to the buffer to be filled
 * @param[in] stream_len  : Writable length of buffer pointed to by \p p_stream
 * @param[in] attr_handle : Attribute handle to be filled
 * @param[in] attr_len    : Attribute len to be filled
 * @param[in] p_attr      : Attribute data
 *
 * \return length of data filled, 0 on error.
 */

int wiced_bt_gatt_put_read_multi_rsp_in_stream(wiced_bt_gatt_optype_t opcode,
    uint8_t *p_stream, int stream_len,
    uint16_t attr_handle, uint16_t attr_len, const uint8_t *p_attr);

/**
 * @brief Utility function to get the variable length handle data from the read multi var length response stream
 *
 * @param[in] p_stream    : Pointer to the buffer to be read
 * @param[in] stream_len  : Readable length of buffer pointed to by \p p_stream
 * @param[out] pp_addr  : Addr to the start of the handle data to be filled
 * @param[out] p_attr_len : Pointer to fill in the complete attribute length
 * @param[out] p_data_in_rsp: Pointer to fill in the actual received length in the rsp at (*pp_addr)
 *
 * \return size of the \p p_stream read
 */
int wiced_bt_gatt_get_multi_handle_data_from_stream(uint8_t *p_stream, int stream_len,
    uint8_t **pp_addr, int *p_attr_len, int *p_data_in_rsp);

/**
 * @brief Utility function to get the handle at index from the handle stream
 *
 * @param[in] p_handle_stream : Stream of handles each of 2 octets
 * @param[in] handle_index    : index into the handle stream
 *
 * \return
 */
uint16_t wiced_bt_gatt_get_handle_from_stream(uint8_t *p_handle_stream, uint16_t handle_index);

/**
 * @brief Utility function to put the handle into the handle stream
 *
 * @param[in] p_handle_stream : Stream of handles each of 2 octets
 * @param[in] stream_len      : Length of the stream
 * @param[in] handle          : handle
 *
 * \return
 */
int wiced_bt_gatt_put_handle_in_stream(uint8_t *p_handle_stream, int stream_len, uint16_t handle);

/**
 * @brief Utility function to get the handle value and handle value len of internal DB handles
 *
 * @param[in] handle : Attribute Handle
 * @param[out] p_len : pointer to store the length of the handle value
 *
 * \return pointer to the value of the handle in the db
 */
const uint8_t *wiced_bt_gatt_get_handle_value(uint16_t handle, int *p_len);

/**
 * @brief Utility function to get the number of packets queued to tx
 *
 * @param[in] conn_id : Connection identifier
 * @param[out] p_fragments_with_controller: pointer to receive the number of fragments with the controller
 *
 * \return number of packets queued to tx
 */
int wiced_bt_gatt_get_num_queued_tx_packets(uint16_t conn_id, int *p_fragments_with_controller);

/**
* @brief Utility function to read local registered db by type, by iterating to the next in a loop
* database entry. Initially \p p_db_start is set to NULL, which resets the search to the head of the local database.
* The database is traversed to locate the next entry which matches the intended \p type.
* If an entry is found the data of that entry is written to \p p_disc_data and the entry in the database is returned.
* The returned entry is passed in the subsequent call to this function in \p p_db_start.
* If an entry is not found the function returns a NULL, which indicates that the entire database has been iterated and the calling loop
* can end.
*
* @param[in] type: Discovery type
* @param[in] p_db_start: The database entry to search from. Value of NULL, resets the database pointer to the head
* @param[out] p_disc_data: discovered data, only valid if the return is not NULL
*
* @return wiced_gattdb_entry_t *
*/
const wiced_gattdb_entry_t *wiced_bt_gattdb_local_read_data_by_type(wiced_bt_gatt_discovery_type_t type,
                                                                    const wiced_gattdb_entry_t *p_db_start,
                                                                    wiced_bt_gatt_discovery_data_t *p_disc_data);

/**
 * @brief Utility function to return the 16 bit UUID of the database entry in the local database
 *  This will return a attribute UUID in the entry. If the attribute uuid is not 2 bytes. It will return 0x0 which is invalid uuid.
 *
 * @param[in] p_db_entry: Database entry for which the UUID is to be returned.
 *
 * @return 16 bit UUID value if available, or 0
*/
uint16_t wiced_bt_gattdb_getAttrUUID16(const wiced_gattdb_entry_t *p_db_entry);

/**
 * @brief Utility function to return the data of the database entry in the local database
 *
 * @param[in] p_db_entry: Database entry of which the data is to be returned.
 *
 * @return data of the database entry
*/
uint8_t *wiced_bt_gattdb_getAttrValue(const wiced_gattdb_entry_t *p_db_entry);

#ifdef __cplusplus
}

#endif
