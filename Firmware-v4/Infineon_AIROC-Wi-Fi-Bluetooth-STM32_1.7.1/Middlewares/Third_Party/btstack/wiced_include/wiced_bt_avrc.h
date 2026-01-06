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
 * Bluetooth AVRCP Application Programming Interface
 *
 */
#pragma once

#include "wiced_bt_sdp.h"
#include "wiced_bt_avrc_defs.h"
#include "wiced_bt_l2c.h"


/**
 * @cond DUAL_MODE
 * @defgroup  wicedbt_avrc        Audio/Video Remote Control (AVRC)
 *
 * This section describes the API's to use Audio/Video Remote Control Profile commands
 * which use underlying AVCT protocol.
 *
 * @addtogroup  wicedbt_avrc    Audio/Video Remote Control (AVRC)
 *
 * @ingroup     wicedbt
 *
 * @{
*/

/*****************************************************************************
**  constants
*****************************************************************************/

/**
 * @anchor AVRC_RESULT
 * @name AVRC result codes.
 * @{
 */
#define AVRC_SUCCESS        0                   /**< Function successful */
#define AVRC_NO_RESOURCES   1                   /**< Not enough resources */
#define AVRC_BAD_HANDLE     2                   /**< Bad handle */
#define AVRC_PID_IN_USE     3                   /**< PID already in use */
#define AVRC_NOT_OPEN       4                   /**< Connection not open */
#define AVRC_MSG_TOO_BIG    5                   /**< the message length exceed the MTU of the browsing channel */
#define AVRC_FAIL           0x10                /**< generic failure */
#define AVRC_BAD_PARAM      0x11                /**< bad parameter   */
#define AVRC_BUSY           0x12                /**< busy with another operation */
/** @} AVRC_RESULT */

/**
 * @anchor AVRC_CT_ROLE
 * @name AVRC Control Role
 * @{ */
#define AVRC_CT_TARGET      1                   /**< AVRC target */
#define AVRC_CT_CONTROL     2                   /**< AVRC controller */
#define AVRC_CT_PASSIVE     4                   /**< AVRC role determined by peer device */
/** @} AVRC_CT_ROLE */

/**
 * @anchor AVRC_CONN_ROLE
 * @name AVRC Connection Role
 * @{ */
#define AVRC_CONN_INITIATOR 0                   /**< AVRC initiator */
#define AVRC_CONN_ACCEPTOR  1                   /**< AVRC acceptor  */
/** @} AVRC_CONN_ROLE */

/** AVRC CTRL events */
enum wiced_bt_avrc_ctrl_evt_e
{
    AVRC_OPEN_IND_EVT,    /**< AVRC_OPEN_IND_EVT event is sent when the connection is successfully opened.T
    his eventis sent in response to an wiced_bt_avrc_open().*/
    AVRC_CLOSE_IND_EVT,    /**< AVRC_CLOSE_IND_EVT event is sent when a connection is closed.
    This event can result from a call to wiced_bt_avrc_close() or when the peer closes    the connection.  It is also sent when a connection attempted through
    wiced_bt_avrc_open() fails. */
    AVRC_CONG_IND_EVT,          /**< AVRC_CONG_IND_EVT event indicates that AVCTP is congested and cannot send any more messages. */
    AVRC_UNCONG_IND_EVT,        /**< AVRC_UNCONG_IND_EVT event indicates that AVCTP is uncongested and ready to send messages. */
    AVRC_BROWSE_OPEN_IND_EVT,    /**< AVRC_BROWSE_OPEN_IND_EVT event is sent when the browse channel is successfully opened.
    This eventis sent in response to an wiced_bt_avrc_open() or wiced_bt_avrc_open_browse() . */
    /**< AVRC_BROWSE_CLOSE_IND_EVT event is sent when a browse channel is closed.
    This event can result from a call to wiced_bt_avrc_close(), wiced_bt_avrc_close_browse() or when the peer closes
    the connection.  It is also sent when a connection attempted through
    wiced_bt_avrc_openBrowse() fails. */
    AVRC_BROWSE_CLOSE_IND_EVT,
    AVRC_BROWSE_CONG_IND_EVT,   /**< AVRC_BROWSE_CONG_IND_EVT event indicates that AVCTP browse channel is congested and cannot send any more messages. */
    AVRC_BROWSE_UNCONG_IND_EVT, /**< AVRC_BROWSE_UNCONG_IND_EVT event indicates that AVCTP browse channel is uncongested and ready to send messages. */
    AVRC_CMD_TIMEOUT_EVT,       /**< AVRC_CMD_TIMEOUT_EVT event indicates timeout waiting for AVRC command response from the peer */
    AVRC_APP_BUFFER_TX_EVT     /**< AVRC_APP_BUFFER_TX_EVT event indicates status of the data transmission */
};

typedef uint8_t wiced_bt_avrc_ctrl_evt_t; /**< @ref wiced_bt_avrc_ctrl_evt_e */

/** AVRC ctype events */
enum wiced_bt_avrc_ctype_e
{
    AVRC_CMD_CTRL = 0,  /**< Instruct a target to perform an operation */
    AVRC_CMD_STATUS,    /**< Check a device's current status */
    AVRC_CMD_SPEC_INQ,  /**< Check whether a target supports a particular
                                      control command; all operands are included */
    AVRC_CMD_NOTIF,     /**< Used for receiving notification of a change in a devices state */
    AVRC_CMD_GEN_INQ,   /**< Check whether a target supports a particular control command; operands are not included */

    /** Response type codes */
    AVRC_RSP_NOT_IMPL = 8, /**< The target does not implement the command specified by the opcode and operand,
                                or doesn't implement the specified subunit */
    AVRC_RSP_ACCEPT,   /**< The target executed or is executing the command */
    AVRC_RSP_REJ,   /**< The target implements the command specified by the
                         opcode but cannot respond because the current state
                         of the target doesn't allow it */
    AVRC_RSP_IN_TRANS,  /**< The target implements the status command but it is
                          in a state of transition; the status command may
                           be retried at a future time */
    AVRC_RSP_IMPL_STBL,  /**< For specific inquiry or general inquiy commands,
                           the target implements the command; for status
                           commands, the target returns stable and includes
                           the status results */
    AVRC_RSP_CHANGED,  /**< The response frame contains a notification that the
                            target device's state has changed */
    AVRC_RSP_INTERIM = 15 /**< For control commands, the target has accepted the
                            request but cannot return information within 100
                            milliseconds; for notify commands, the target accepted
                            the command, and will notify the controller of a change
                            of target state at a future time */

};

typedef uint8_t wiced_bt_avrc_ctype_t;  /**< @ref wiced_bt_avrc_ctype_e */

/** Supported categories */
#define AVRC_SUPF_CT_CAT1               0x0001      /**< Category 1 */
#define AVRC_SUPF_CT_CAT2               0x0002      /**< Category 2 */
#define AVRC_SUPF_CT_CAT3               0x0004      /**< Category 3 */
#define AVRC_SUPF_CT_CAT4               0x0008      /**< Category 4 */
#define AVRC_SUPF_CT_BROWSE             0x0040      /**< Browsing */

#define AVRC_SUPF_TG_CAT1               0x0001      /**< Category 1 */
#define AVRC_SUPF_TG_CAT2               0x0002      /**< Category 2 */
#define AVRC_SUPF_TG_CAT3               0x0004      /**< Category 3 */
#define AVRC_SUPF_TG_CAT4               0x0008      /**< Category 4 */
#define AVRC_SUPF_TG_APP_SETTINGS       0x0010      /**< Player Application Settings */
#define AVRC_SUPF_TG_GROUP_NAVI         0x0020      /**< Group Navigation */
#define AVRC_SUPF_TG_BROWSE             0x0040      /**< Browsing */
#define AVRC_SUPF_TG_MULTI_PLAYER       0x0080      /**< Muliple Media Player */

#define AVRC_META_SUCCESS               AVRC_SUCCESS    /**< AVRC success */
 #define AVRC_META_FAIL                  AVRC_FAIL      /**< AVRC fail */
#define AVRC_METADATA_CMD               0x0000          /**< AVRC metadata command */
#define AVRC_METADATA_RESP              0x0001          /**< AVRC metadata response */



/** AVRC received messages to sent to the upper layer */
typedef struct{
    uint8_t msg_type;       /**< Message type CMD/RSP */
    uint8_t handle;         /**< connection Handle */
    uint8_t label;          /**< label */
    uint8_t opcode;         /**< opcode of the command or response*/

    union{
    wiced_bt_avrc_rsp_t response;   /**< response message */
    wiced_bt_avrc_cmd_t command;    /**< command message */
    }type; /**<  Received message type */

}wiced_bt_avrc_msg_t;


/*****************************************************************************
**  data type definitions
*****************************************************************************/

/**
 * AVRC control callback function.
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       event       : AVRC ctrl event (see @ref wiced_bt_avrc_ctrl_evt_e)
 * @param[in]       result      : Result code (see @ref AVRC_RESULT "AVRC result codes")
 * @param[in]       p_buf       : pointer to application transmit buffer
 * @param[in]       peer_addr   : Peer device address
 *
 * @return          Nothing
 */
typedef void (wiced_bt_avrc_ctrl_cback_t) (uint8_t handle, wiced_bt_avrc_ctrl_evt_t event, uint16_t result,
             wiced_bt_avrc_xmit_buf_t* p_buf, wiced_bt_device_address_t peer_addr);


/**
 * AVRC data callback function.  It is executed when AVCTP has
 * a message packet ready for the application.  The implementation of this
 * callback function must copy the wiced_bt_avrc_msg_t structure passed to it as it
 * is not guaranteed to remain after the callback function exits.
 *
 * @param[in]       handle  : Connection handle
 * @param[in]       label   : Message label
 * @param[in]       opcode  : Message opcode (see @ref AVRC_OPCODES "AVRC opcodes")
 * @param[in]       p_msg   : AVRC message
 *
 * @return          Nothing
 */
typedef void (wiced_bt_avrc_msg_cback_t) (wiced_bt_avrc_msg_t *p_msg);


/** AVRC connection configuration structure; used when calling wiced_bt_avrc_open() to configure the AVRC connection and register the callbacks. */
typedef struct
{
    wiced_bt_avrc_ctrl_cback_t      *p_ctrl_cback;      /**< AVRC connection control callback */
    wiced_bt_avrc_msg_cback_t       *p_msg_cback;       /**< AVRC data callback */
    uint32_t                        company_id;         /**< Company ID  (see @ref AVRC_COMPANY_ID "Company IDs") */
    uint8_t                         connection_role;    /**< Connection role: AVRC_CONN_INT (initiator) or AVRC_CONN_ACP (acceptor) (see @ref AVRC_CONN_ROLE "AVRC connection roles") */
    uint8_t                         control;            /**< Control role: AVRC_CT_TARGET (target) or AVRC_CT_CONTROL (controller) (see @ref AVRC_CT_ROLE "AVRC control roles")*/
    uint16_t                        avrc_seg_buf_len;   /**< Maximum length of assembled AVRC data */
    uint8_t                         *p_avrc_buff;       /**< Pointer to AVRC assembly buffer */
    uint16_t                        avct_seg_buf_len;   /**< Maximum length of AVCT assembled data */
    uint8_t                         *p_avct_buff;       /**< Pointer to AVCT assembly buffer */
} wiced_bt_avrc_config_t;


#ifdef __cplusplus
extern "C"
{
#endif

/**
* This function initializes AVRCP and prepares the protocol stack for its use.
* This function must be called once by the system or platform using AVRCP
* before the other functions of the API an be used
*
*  @param[in]      mtu       : Control Channel MTU (Min Value = 48, default value = L2CAP MTU)
*  @param[in]      mtu_br    : Browsing Channel MTU (Min Value = 335, default value = L2CAP MTU)
*
*  @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
*/
uint16_t wiced_bt_avrc_init(uint16_t mtu, uint16_t mtu_br);

/**
 * Open AVRC connection (as intiator or acceptor); register notification callbacks.
 * The connection role may be AVRC controller or target.
 * The connection remains available to the application until
 * wiced_bt_avrc_close() is called.
 * On receiving AVRC_CLOSE_IND_EVT, acceptor connections remain in
 * acceptor mode (no need to re-open the connection)
 *
 * @param[out]      p_handle    : Connection handle (valid if AVRC_SUCCESS is returned)
 * @param[in]       p_config    : AVRC connection configuration structure (callbacks and role configuration)
 * @param[in]       peer_addr   : Peer device address (if initiator)
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 */
uint16_t wiced_bt_avrc_open(uint8_t *p_handle, wiced_bt_avrc_config_t *p_config,
                            wiced_bt_device_address_t peer_addr);

/**
 * Close AVRCP connection
 *
 * @param[in]       handle      : Handle of connection to close
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_close(uint8_t handle);

/**
 * Open AVRCP browsing connection, either as initiator or acceptor.
 *
 * @param[in]       handle      : Connection handle (obtained from wiced_bt_avrc_open)
 * @param[in]       conn_role   : Initiator or acceptor of the connection
 *                                (see @ref AVRC_CONN_ROLE "AVRC connection roles")
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_open_browse(uint8_t handle, uint8_t conn_role);

/**
 * Close AVRCP browsing connection
 *
 * @param[in]       handle      : Connection handle
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_close_browse(uint8_t handle);

/**
 * Send an AVRC vendor dependent messages
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Transaction label
 * @param[in]       ctype       : Message type (see @ref wiced_bt_avrc_ctype_e)
 * @param[in]       p_cmdbuf    : Pointer to the buffer holding the AVRC message
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_send_metadata_msg (uint8_t handle, uint8_t label, wiced_bt_avrc_ctype_t ctype, wiced_bt_avrc_xmit_buf_t *p_cmdbuf);

/**
 * Send a UNIT INFO command to the peer device. This
 * function can only be called for controller role connections.
 * Any response message from the peer is passed back through
 * the wiced_bt_avrc_msg_cback_t callback function.
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Transaction label
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_send_unit_cmd(uint8_t handle, uint8_t label);

/**
 * Send a SUBUNIT INFO command to the peer device.  This
 * function can only be called for controller role connections.
 * Any response message from the peer is passed back through
 * the wiced_bt_avrc_msg_cback_t callback function.
 *
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Transaction label
 * @param[in]       page        : Specifies which subunit table is requested.
 *                                For AVRCP it is typically zero. Value range is 0-7.
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_send_subunit_cmd(uint8_t handle, uint8_t label, uint8_t page);


/**
 * Send a PASS THROUGH command to the peer device.  This
 * function can only be called for controller role connections.
 * Any response message from the peer is passed back through
 * the wiced_bt_avrc_msg_cback_t callback function.
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Transaction label
 * @param[in]       p_msg       : Pointer to the pass through command
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_send_passthrough_cmd(uint8_t handle, uint8_t label, wiced_bt_avrc_pass_thru_cmd_t *p_msg);

/**
 * Send a PASS THROUGH response to the peer device.  This
 * function can only be called for target role connections.
 * This function must be called when a PASS THROUGH command
 *                  message is received from the peer through the
 * wiced_bt_avrc_msg_cback_t callback function.
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Transaction label
 * @param[in]       ctype      : msg type(AVRC_RSP_ACCEPT/AVRC_RSP_REJ)(see @ref wiced_bt_avrc_ctype_e)
 * @param[in]       p_msg       : Pointer to the pass through response
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_send_passthrough_rsp(uint8_t handle, uint8_t label, wiced_bt_avrc_ctype_t ctype, wiced_bt_avrc_pass_thru_cmd_t *p_msg);


/**
 * Send a VENDOR DEPENDENT message to the peer device.  This
 * function can  be called for CT/TG role connections.
 * Any response message from the peer is passed back through
 * the wiced_bt_avrc_msg_cback_t callback function.
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Transaction label
 * @param[in]       cr          : Command or resonse
 * @param[in]       p_hdr      :  Pointer to the avrc header, which gets copied to be used as protocol header
 * @param[in]       company_id  : This unique Company_ID
 * @param[in]       p_msg       : Pointer to the vendor dependent data \p p_msg
 *                                can be freed by application on receiving the AVRC_APP_BUFFER_TX_EVT.
 *                                The p_msg pointer shall be valid and kept to the contents of location
 *                                pointed to by the p_msg pointer shall be valid and kept.
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */
uint16_t wiced_bt_avrc_send_vendor_msg(uint8_t handle, uint8_t label, uint8_t cr, wiced_bt_avrc_hdr_t *p_hdr, uint32_t company_id, wiced_bt_avrc_xmit_buf_t  *p_msg);

/**
 * Sets the trace level for AVRC. If 0xff is passed, the
 * current trace level is returned.
 *
 * @param[in]       new_level   : New trace level
 *
 * @return          The new trace level or current trace level if
 *                  the input parameter is 0xff.
 *
 */
uint8_t wiced_bt_avrc_set_trace_level (uint8_t new_level);


/**
 * Build AVRCP metadata command
 *
 * @param[in]       p_cmd       : Pointer to the structure to build the command from
 * @param[in]       p_xmit_buf  : Pointer to the buffer to build the command into
 *
 * @return          Status code (see @ref AVRC_STS "AVRC status codes")
 *                  AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
 *                  Otherwise, the error code defined by AVRCP 1.4
 *
 */
wiced_bt_avrc_sts_t wiced_bt_avrc_bld_metadata_cmd(wiced_bt_avrc_metadata_cmd_t *p_cmd, wiced_bt_avrc_xmit_buf_t *p_xmit_buf);

/**
 * Build AVRCP metadata response
 *
 * @param[in]       p_rsp       : Pointer to the structure to build the response from
 * @param[in]       p_rspbuf    : Pointer to the buffer to build the response into
 *
 *
 * @return          Status code (see @ref AVRC_STS "AVRC status codes")
 *                  AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
 *                  Otherwise, the error code defined by AVRCP 1.4
 *
 */
wiced_bt_avrc_sts_t wiced_bt_avrc_bld_metadata_response(  wiced_bt_avrc_metadata_rsp_t *p_rsp, wiced_bt_avrc_xmit_buf_t *p_rspbuf);

/**
 * Build AVRCP Browse response
 *
 * @param[in]       p_cmd       : Pointer to the structure to build the command from
 * @param[in]       p_xmit_buf  : Pointer to the buffer to build the command into
 *
 *
 * @return          Status code (see @ref AVRC_STS "AVRC status codes")
 *                  AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
 *                  Otherwise, the error code defined by AVRCP 1.4
 *
 */
wiced_bt_avrc_sts_t wiced_bt_avrc_bld_browse_command (wiced_bt_avrc_browse_cmd_t *p_cmd, wiced_bt_avrc_xmit_buf_t *p_xmit_buf);

/**
 * Build AVRCP Browse response
 *
 * @param[in]       p_rsp       : Pointer to the structure to build the response from
 * @param[in]       p_rspbuf    : Pointer to the buffer to build the response into
 *
 *
 * @return          Status code (see @ref AVRC_STS "AVRC status codes")
 *                  AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
 *                  Otherwise, the error code defined by AVRCP 1.4
 *
 */

wiced_bt_avrc_sts_t wiced_bt_avrc_bld_browse_response (wiced_bt_avrc_browse_rsp_t *p_rsp, wiced_bt_avrc_xmit_buf_t *p_rspbuf);


/**
 * send the Browsing data
 *
 * @param[in]       handle      : Connection handle
 * @param[in]       label       : Message label
 * @param[in]       ctype       : avrc message type (see @ref wiced_bt_avrc_ctype_e)
 * @param[in]       p_xmit_buf  : Pointer to transmit buffer
 *
 *
 * @return          Result code (see @ref AVRC_RESULT "AVRC result codes")
 *
 */

uint16_t  wiced_bt_avrc_send_browse_data(uint8_t handle, uint8_t label,  wiced_bt_avrc_ctype_t ctype, wiced_bt_avrc_xmit_buf_t *p_xmit_buf);




/**
 * Check if correct AVRC message type is specified
 *
 * @param[in]       pdu_id      : PDU ID
 * @param[in]       ctype       : avrc message type (see @ref wiced_bt_avrc_ctype_e)
 *
 * @return          true if it is valid, false otherwise
 *
 *
 */
wiced_bool_t wiced_bt_avrc_is_valid_avc_type(uint8_t pdu_id, wiced_bt_avrc_ctype_t ctype);

/**
 * Check if the given attrib value is a valid one
 *
 * @param[in]       attr      : Player attribute ID
 *
 * Returns          returns true if it is valid
 *
 */
wiced_bool_t wiced_bt_avrc_is_valid_player_attr(uint8_t attr);

/**
 *
 * This function gets the control MTU
 *
 * Returns          returns AVRC Control MTU
 *
 */
uint16_t wiced_bt_avrc_get_ctrl_mtu(void);

/**
 *
 * This function gets the data MTU
 *
 * Returns          returns AVRC DATA MTU
 *
 */
uint16_t wiced_bt_avrc_get_data_mtu(void);

/******* avrc response utility/parse functions **********/
/**
 *
 * This function validates player attribute id and  value.
 *
 * @param[in]       attrib      : player attribute id(see @ref AVRC_PLAYER_SETTINGS "AVRC player settings ids" )
 * @param[in]       value       :  player attribute vallue (see @ref AVRC_PLAYER_SETTINGS_VALS "possible values of the Player Application Settings")
 *
 * Returns          Returns TRUE or FALSE
 *
 */

wiced_bool_t avrc_is_valid_player_attrib_value(uint8_t attrib, uint8_t value);

/**
 *
 * This function parse the avrc attribute
 *
 * @param[in]       p_attr_stream : received response stream offset-ed by amount read
 * @param[in]       stream_len    : valid length of buffer pointed by \p p_attr_stream
 * @param[out]      p_attr        :  pointer to the @ref wiced_bt_avrc_attr_entry_t parsed into
 *
 * Returns          Returns number of bytes read from the buffer
 *
 */
int avrc_read_attr_entry_from_stream(uint8_t *p_attr_stream, uint16_t stream_len, wiced_bt_avrc_attr_entry_t *p_attr);

 /**
 *
 * This function parse the string with charset_id received in buffer.
 *
 * @param[in]       p_name_stream   : received response stream offset-ed by amount read
 * @param[in]       stream_len      : valid length of buffer pointed by \p p_name_stream
 * @param[out]      p_name          :  pointer to the @ref wiced_bt_avrc_full_name_t parsed into
 *
 * Returns          Returns number of bytes read from the buffer
 *
 */
int avrc_read_full_name_from_stream(uint8_t *p_name_stream, uint16_t stream_len, wiced_bt_avrc_full_name_t *p_name);

/**
*
* This function parse  the response for browsable item player/folder/media
*
* @param[in]       p_item_stream    : received response stream offset-ed by amount read
* @param[in]       stream_len       : valid length of buffer pointed by \p p_item_stream
* @param[out]      p_rsp            :  pointer to the @ref wiced_bt_avrc_item_t parsed into
*
* Returns          Returns number of bytes read from the buffer
*
*/

int avrc_read_browse_item_from_stream(uint8_t *p_item_stream, uint16_t stream_len, wiced_bt_avrc_item_t *p_rsp);

/**
 *
 * This function parse the string received in buffer.
 *
 * @param[in]       p_name_stream  : received response stream offset-ed by amount read
 * @param[in]       stream_len     : valid length of buffer pointed by \p p_name_stream
 * @param[out]      p_name         :  pointer to the @ref wiced_bt_avrc_name_t parsed into
 *
 * Returns          Returns number of bytes read from the buffer
 *

 */

int avrc_read_name_from_stream(uint8_t *p_name_stream, uint16_t stream_len, wiced_bt_avrc_name_t *p_name);


/**
 *
 * This function set the data receive buffer in stack.
 *
 * @param[in]       handle              : AVRC  connection handle
 * @param[in]       p_drb               : pointer to the buffer
 * @param[out]      payload_len         :  valid length of buffer pointed by \p p_drb
 * @param[out]      p_unreg_cb          :  Callback function to release the  DRB pointed by \p p_drb
 *
 * Returns          Returns TRUE if success otherwise, FALSE
 *

 */

wiced_bool_t wiced_bt_avrc_set_browse_drb(uint8_t handle, tDRB *p_drb, uint16_t payload_len, wiced_bt_l2cap_drb_release_cb *p_unreg_cb);

/**@} wicedbt */
/* @endcond*/

#ifdef __cplusplus
}
#endif
