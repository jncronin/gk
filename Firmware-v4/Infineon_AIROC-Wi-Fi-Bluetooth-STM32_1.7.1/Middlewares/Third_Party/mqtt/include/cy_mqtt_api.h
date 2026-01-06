/*
 * Copyright 2023, Cypress Semiconductor Corporation (an Infineon company) or
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
 * @file cy_mqtt_api.h
 * @brief Header file MQTT wrapper API's functions.
 */

#ifndef CY_MQTT_API_H_
#define CY_MQTT_API_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cy_result_mw.h"
#include "cy_nw_helper.h"
#include "cy_log.h"

/* MQTT API headers. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "transport_interface.h"

/* AWS iot Port layer headers. */
#include "clock.h"
#include "cy_retry_utils.h"
#include "cy_tcpip_port_secure_sockets.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * \defgroup group_c_api C APIs
 * \defgroup mqtt_struct Structures and Enumerations
 * @ingroup group_c_api
 * \defgroup mqtt_defines Macros
 * @ingroup group_c_api
 * \defgroup mqtt_api_functions Functions
 * @ingroup group_c_api
 */

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
/**
 * @addtogroup mqtt_defines
 *
 * MQTT library preprocessor directives such as results and error codes
 *
 * Cypress middleware APIs return results of type cy_rslt_t and consist of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result Format
 *
   \verbatim
              Module base         Type    Library-specific error code
      +-------------------------+------+------------------------------+
      |          0x0200         | 0x2  |           Error Code         |
      +-------------------------+------+------------------------------+
                14 bits          2 bits            16 bits

   See the macro section of this document for library-specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h.
 * In Mbed OS, the PSoC 6 MCU target platform is located in <mbed-os/targets/TARGET_Cypress/TARGET_PSOC6/psoc6csp/core_lib/include>.
 * In ModusToolbox environment,  PSoC 6 MCU target platform is located in <core_lib/include>.
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of CY_RSLT_MODULE_MIDDLEWARE_BASE.
 *              Details of the offset and the middleware base are defined in cy_result_mw.h, which is part of [Github connectivity-utilities] (https://github.com/Infineon/connectivity-utilities).
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING, or CY_RSLT_TYPE_INFO. MQTT library error codes are of type CY_RSLT_TYPE_ERROR which is 0x2.
 *
 * Library-specific error codes: These error codes are library-specific and defined in the macro section.
 *
 * Helper macros used for creating library-specific results are provided as part of cy_result.h.
 *
 * \note
 *     The functions defined in this library can return errors other than the error codes listed in this section.
 *     Please decode the error using the information provided above. Based on the module base value, refer the header file of corresponding module to identify the error.
 *
 *  @{
 */

/** MQTT error code base. */
#define CY_RSLT_MQTT_ERR_BASE                                      CY_RSLT_CREATE( CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_AWS_BASE, 0 )
/** Generic MQTT API error. */
#define CY_RSLT_MODULE_MQTT_ERROR                                  ( CY_RSLT_MQTT_ERR_BASE + 1 )
/** Invalid argument. */
#define CY_RSLT_MODULE_MQTT_BADARG                                 ( CY_RSLT_MQTT_ERR_BASE + 2 )
/** Out of memory. */
#define CY_RSLT_MODULE_MQTT_NOMEM                                  ( CY_RSLT_MQTT_ERR_BASE + 3 )
/** MQTT library Init failure. */
#define CY_RSLT_MODULE_MQTT_INIT_FAIL                              ( CY_RSLT_MQTT_ERR_BASE + 4 )
/** MQTT library Deinit failure. */
#define CY_RSLT_MODULE_MQTT_DEINIT_FAIL                            ( CY_RSLT_MQTT_ERR_BASE + 5 )
/** MQTT library Create failure. */
#define CY_RSLT_MODULE_MQTT_CREATE_FAIL                            ( CY_RSLT_MQTT_ERR_BASE + 6 )
/** MQTT library Delete failure. */
#define CY_RSLT_MODULE_MQTT_DELETE_FAIL                            ( CY_RSLT_MQTT_ERR_BASE + 7 )
/** Invalid MQTT handle. */
#define CY_RSLT_MODULE_MQTT_INVALID_HANDLE                         ( CY_RSLT_MQTT_ERR_BASE + 8 )
/** MQTT library Connect failure. */
#define CY_RSLT_MODULE_MQTT_CONNECT_FAIL                           ( CY_RSLT_MQTT_ERR_BASE + 9 )
/** MQTT library Disconnect failure. */
#define CY_RSLT_MODULE_MQTT_DISCONNECT_FAIL                        ( CY_RSLT_MQTT_ERR_BASE + 10 )
/** MQTT library Publish failure. */
#define CY_RSLT_MODULE_MQTT_PUBLISH_FAIL                           ( CY_RSLT_MQTT_ERR_BASE + 11 )
/** MQTT library Subscribe failure. */
#define CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL                         ( CY_RSLT_MQTT_ERR_BASE + 12 )
/** MQTT library Unsubscribe failure. */
#define CY_RSLT_MODULE_MQTT_UNSUBSCRIBE_FAIL                       ( CY_RSLT_MQTT_ERR_BASE + 13 )
/** MQTT client not connected. */
#define CY_RSLT_MODULE_MQTT_NOT_CONNECTED                          ( CY_RSLT_MQTT_ERR_BASE + 14 )
/** MQTT client connection closed. */
#define CY_RSLT_MODULE_MQTT_CLOSED                                 ( CY_RSLT_MQTT_ERR_BASE + 15 )
/** MQTT client already connected. */
#define CY_RSLT_MODULE_MQTT_ALREADY_CONNECTED                      ( CY_RSLT_MQTT_ERR_BASE + 16 )
/** Protocol not supported. */
#define CY_RSLT_MODULE_MQTT_PROTOCOL_NOT_SUPPORTED                 ( CY_RSLT_MQTT_ERR_BASE + 17 )
/** Invalid credentials. */
#define CY_RSLT_MODULE_MQTT_INVALID_CREDENTIALS                    ( CY_RSLT_MQTT_ERR_BASE + 18 )
/** TLS handshake failed. */
#define CY_RSLT_MODULE_MQTT_HANDSHAKE_FAILED                       ( CY_RSLT_MQTT_ERR_BASE + 19 )
/** MQTT library handle not found. */
#define CY_RSLT_MODULE_MQTT_HANDLE_NOT_FOUND                       ( CY_RSLT_MQTT_ERR_BASE + 20 )
/** MQTT virtual API failure due to VCM error. */
#define CY_RSLT_MODULE_MQTT_VCM_ERROR                              ( CY_RSLT_MQTT_ERR_BASE + 21 )
/** MQTT library not initialized. */
#define CY_RSLT_MODULE_MQTT_NOT_INITIALIZED                        ( CY_RSLT_MQTT_ERR_BASE + 22 )

/**
 * MQTT event type for subscribed message receive event.
 * Macro is added for library backward compatibility.
 * \note This macro is defined only for backward compatibility. Application should not modify this macro.
 */
#define CY_MQTT_EVENT_TYPE_PUBLISH_RECEIVE       CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE

/**
 * Minimum network buffer size in bytes, for sending and receiving an MQTT packet.
 *
 * \note
 *    This is the default value configured in the library. This value can be modified to suit the application use case requirements.
 *
 */
#define CY_MQTT_MIN_NETWORK_BUFFER_SIZE          ( 256U )

/**
 * Maximum wait time in milliseconds to receive acknowledgment packet from the MQTT broker for QoS1/QoS2 Publish/Subscribe message.
 * MQTT library function returns to the caller immediately, if the MQTT acknowledgment message is received before the timeout/wait time. Else, the function returns the failure status at the end of wait time.
 * \note
 *    This is the default value configured in the library. This value can be modified depending on the network condition.
 *
 */
#ifndef CY_MQTT_ACK_RECEIVE_TIMEOUT_MS
#define CY_MQTT_ACK_RECEIVE_TIMEOUT_MS           ( 3000U )
#endif

/**
 * MQTT message send timeout in milliseconds.
 * MQTT library function returns to the caller immediately, if the MQTT message is sent before the timeout/wait time. Else, the function returns the failure status at the end of wait time.
 * \note
 *    This is the default value configured in the library. This value can be modified depending on the network condition.
 *
 */
#ifndef CY_MQTT_MESSAGE_SEND_TIMEOUT_MS
#define CY_MQTT_MESSAGE_SEND_TIMEOUT_MS          ( 3000U )
#endif

/**
 * MQTT message receive timeout for the subscribed topic, represented in milliseconds.
 * MQTT network receive function waits for this timeout value, in case if partial data is received from the network socket. If the complete data is received before this timeout value, the network receive functions returns to the caller immediately after reading the requested data from the socket.
 * \note
 *    This is the default value configured in the library. This value can be modified depending on the network condition.
 *
 */
#ifndef CY_MQTT_MESSAGE_RECEIVE_TIMEOUT_MS
#define CY_MQTT_MESSAGE_RECEIVE_TIMEOUT_MS       ( 500U )
#endif

/**
 * Maximum number of retry for MQTT publish/subscribe/unsubcribe message send.
 *
 * \note
 *    This is the default value configured in the library. This value can be modified by defining macro in application makefile.
 *
 */
#ifndef CY_MQTT_MAX_RETRY_VALUE
#define CY_MQTT_MAX_RETRY_VALUE                  ( 3U )
#endif

/**
 * Maximum number of MQTT instances supported.
 */
#define CY_MQTT_MAX_HANDLE                       ( 2U )

/**
 * Configure value of maximum number of outgoing publishes maintained in MQTT library
 * until an ack is received from the broker.
 */
#ifndef CY_MQTT_MAX_OUTGOING_PUBLISHES
#define CY_MQTT_MAX_OUTGOING_PUBLISHES           ( 1U )
#endif
/**
 * Configure value of maximum number of outgoing subscription topics maintained in MQTT library
 * until an ack is received from the broker.
 */
#ifndef CY_MQTT_MAX_OUTGOING_SUBSCRIBES
#define CY_MQTT_MAX_OUTGOING_SUBSCRIBES          ( 5U )
#endif

/**
 * Maximum length of descriptor supported.
 */
#define CY_MQTT_DESCP_MAX_LEN                    ( 20 )

/**
 * Stack size for MQTT Event processing thread.
 */
#ifndef CY_MQTT_EVENT_THREAD_STACK_SIZE
    #ifdef ENABLE_MQTT_LOGS
        /* Additional 3kb of stack is added for enabling the prints */
        #define CY_MQTT_EVENT_THREAD_STACK_SIZE     ( (1024 * 3) + (1024 * 3) )
    #else
        #define CY_MQTT_EVENT_THREAD_STACK_SIZE     ( 1024 * 3 )
    #endif
#endif
/**
 * @}
 */

/**
 * @addtogroup mqtt_struct
 *
 * mqtt library data structures and type definitions
 *
 * @{
 */

/******************************************************
 *                   Enumerations
 ******************************************************/
/**
 * MQTT QoS levels.
 */
typedef enum cy_mqtt_qos
{
    CY_MQTT_QOS0        = 0,  /**< Delivery at most once. */
    CY_MQTT_QOS1        = 1,  /**< Delivery at least once. */
    CY_MQTT_QOS2        = 2,  /**< Delivery exactly once. */
    CY_MQTT_QOS_INVALID = -1  /**< Invalid QoS. */
} cy_mqtt_qos_t;

/**
 * MQTT event types.
 */
typedef enum cy_mqtt_event_type
{
    CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE = 0, /**< Message from the subscribed topic. */
    CY_MQTT_EVENT_TYPE_DISCONNECT                   = 1  /**< Disconnected from MQTT broker. */
} cy_mqtt_event_type_t;

/**
 * MQTT disconnect type.
 */
typedef enum cy_mqtt_disconn_type
{
    CY_MQTT_DISCONN_TYPE_BROKER_DOWN   = 0,  /**< Keep-alive not received from broker, possibly broker is down  */
    CY_MQTT_DISCONN_TYPE_NETWORK_DOWN  = 1,  /**< Network is disconnected */
    CY_MQTT_DISCONN_TYPE_BAD_RESPONSE  = 2,  /**< Bad response from MQTT broker. possibly received MQTT packet with invalid packet type ID */
    CY_MQTT_DISCONN_TYPE_SND_RCV_FAIL  = 3   /**< MQTT packet send or receive operation failed due to network latency (or) send/receive related timeouts */
} cy_mqtt_disconn_type_t;

/**
 * @}
 */

/******************************************************
 *                 Type Definitions
 ******************************************************/
/**
 * @var cy_mqtt_t
 * Handle to MQTT instance
 */
typedef void * cy_mqtt_t;

/******************************************************
 *                    Structures
 ******************************************************/
 /**
 * @addtogroup mqtt_struct
 *
 * mqtt library data structures and type definitions
 *
 * @{
 */

/**
 * MQTT subscribe information structure.
 */
typedef struct cy_mqtt_subscribe_info
{
    cy_mqtt_qos_t  qos;           /**< Requested quality of Service for the subscription. */
    const char     *topic;        /**< Topic filter to subscribe to. */
    uint16_t       topic_len;     /**< Length of subscription topic filter. */
    cy_mqtt_qos_t  allocated_qos; /**< QoS allocated by the broker for the subscription. \ref CY_MQTT_QOS_INVALID indicates subscription failure. */
} cy_mqtt_subscribe_info_t;

/**
 * MQTT publish information structure.
 * MQTT messages received on the subscribed topic is also represented using this structure.
 */
typedef struct cy_mqtt_publish_info
{
    cy_mqtt_qos_t  qos;          /**< Quality of Service for message. */
    bool           retain;       /**< Whether this is a retained message. */
    bool           dup;          /**< Whether this is a duplicate publish message. */
    const char     *topic;       /**< Topic name on which the message is published. */
    uint16_t       topic_len;    /**< Length of topic name. */
    const char     *payload;     /**< Message payload. */
    size_t         payload_len;  /**< Message payload length. */
} cy_mqtt_publish_info_t;

/**
 * MQTT broker information structure.
 */
typedef struct cy_mqtt_broker_info
{
    const char                   *hostname;      /**< Server host name. Must be Non NULL-terminated. This memory needs to be maintained until MQTT object is deleted.*/
    uint16_t                     hostname_len;   /**< Length of the server host name. */
    uint16_t                     port;           /**< Server port in host-order. */
} cy_mqtt_broker_info_t;

/**
 * MQTT connect information structure.
 */
typedef struct cy_mqtt_connect_info
{
    const char                   *client_id;     /**< MQTT client identifier. Must be unique per client. This memory needs to be maintained until MQTT object is deleted.*/
    uint16_t                     client_id_len;  /**< Length of the client identifier. */
    const char                   *username;      /**< MQTT user name. Set to NULL if not used. This memory needs to be maintained until MQTT object is deleted.*/
    uint16_t                     username_len;   /**< Length of MQTT user name. Set to 0 if not used. */
    const char                   *password;      /**< MQTT password. Set to NULL if not used. This memory needs to be maintained until MQTT object is deleted.*/
    uint16_t                     password_len;   /**< Length of MQTT password. Set to 0 if not used. */
    bool                         clean_session;  /**< Whether to establish a new, clean session or resume a previous session.*/
    uint16_t                     keep_alive_sec; /**< MQTT keep alive period.*/
    cy_mqtt_publish_info_t       *will_info;     /**< MQTT will message. This will info can be NULL. */
 } cy_mqtt_connect_info_t;

 /**
  * MQTT received message information structure.
  * \note This type is defined for improving readability and for library backward compatibility.
  */
 typedef cy_mqtt_publish_info_t cy_mqtt_received_msg_info_t;

/**
 * Received MQTT publish message information structure.
 */
typedef struct cy_mqtt_message
{
    uint16_t                    packet_id;         /**< Packet ID of the MQTT message. */
    cy_mqtt_received_msg_info_t received_message;  /**< Received MQTT message from the subscribed topic. */
} cy_mqtt_message_t;

/**
 * MQTT event information structure.
 */
typedef struct cy_mqtt_event
{
    cy_mqtt_event_type_t type;             /**< Event type */
    union
    {
        cy_mqtt_disconn_type_t   reason;   /**< Disconnection reason for event type \ref CY_MQTT_EVENT_TYPE_DISCONNECT */
        cy_mqtt_message_t        pub_msg;  /**< Received MQTT message for event type \ref CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE */
    } data;                                /**< Event data */
} cy_mqtt_event_t;

/**
 * MQTT unsubscribe information structure.
 */
typedef cy_mqtt_subscribe_info_t cy_mqtt_unsubscribe_info_t;


/**
 * @}
 */

/******************************************************
 *               Function Declarations
 ******************************************************/

/**
 *
 * @addtogroup mqtt_api_functions
 *
 * C APIs provided by the MQTT library.
 * All library API functions except cy_mqtt_init and cy_mqtt_deinit are thread safe.
 *
 * @{
 */

/**
 * MQTT event callback functions type used to process the disconnect event and incoming MQTT publish packets
 * received from the MQTT broker.
 *
 * \note
 *    MQTT library functions should not be invoked from this callback function.
 *
 * @param mqtt_handle [in]     : MQTT handle.
 * @param event [in]           : MQTT event information. For event values refer \ref cy_mqtt_event_t .
 * @param user_data [in]       : Pointer to user data provided during \ref cy_mqtt_create.
 *
 * @return                     : void
 */
typedef void ( *cy_mqtt_callback_t )( cy_mqtt_t mqtt_handle, cy_mqtt_event_t event, void *user_data );

/**
 * Performs network sockets initialization required for the MQTT library.
 * <b>It must be called once (and only once) before calling any other function in this library.</b>
 *
 * This API is supported in multi-core environment and must be invoked on the secondary core application
 * before calling any other virtual MQTT APIs.
 *
 * \note \ref cy_mqtt_init and \ref cy_mqtt_deinit API functions are not thread-safe. The caller
 *       must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_init( void );

/**
 * Creates an MQTT instance and initializes its members based on the supplied arguments.
 * Initializes the core AWS MQTT library and its components.
 * The handle to the MQTT instance is returned via the handle pointer supplied by the user on success.
 * This handle is used for creating MQTT connection with MQTT broker.
 * Same handle is used for MQTT Publish, Subscribe, and Unsubscribing from MQTT topic.
 * Note: To receive notification of events like availability of data for subscribed topic, or
 *       network disconnection, use \ref cy_mqtt_register_event_callback.
 *
 * @param buffer [in]         : Network buffer for send and receive.
 *                              Application needs to allocate memory for network buffer and should not be freed until MQTT object is deleted.
 *                              The minimum buffer size is defined by \ref CY_MQTT_MIN_NETWORK_BUFFER_SIZE. Please make sure the buffer allocated is larger than \ref CY_MQTT_MIN_NETWORK_BUFFER_SIZE.
 *                              This buffer is used for sending and receiving MQTT packets over the network. Hence, the buffer size should be sufficiently large enough to hold the MQTT packet header and payload.
 *                              For example: To send/receive the MQTT payload of 4 kb, it's recommended to allocate 4.5 kb or more buffer.
 * @param buff_len [in]       : Network buffer length in bytes.
 * @param security [in]       : Credentials for TLS connection.
 *                              Application needs to allocate memory for keys, certs, and sni/user names should not be freed until MQTT object is deleted.
 * @param broker_info [in]    : MQTT broker information. Refer \ref cy_mqtt_broker_info_t for details.
 * @param descriptor          : A string that describes the MQTT handle that is being created in order to uniquely identify it.
 * @param mqtt_handle [out]   : Pointer to store the MQTT handle allocated by this function on successful return.
 *
 * @return cy_rslt_t          : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_create( uint8_t *buffer, uint32_t buff_len,
                          cy_awsport_ssl_credentials_t *security,
                          cy_mqtt_broker_info_t *broker_info,
                          char *descriptor,
                          cy_mqtt_t *mqtt_handle );

/**
 * Connects to the given MQTT broker using a secured/non-secured TCP connection and establishes MQTT client session with the broker.
 *
 * @param mqtt_handle [in]   : MQTT handle created using \ref cy_mqtt_create.
 * @param connect_info [in]  : MQTT connection parameters. Refer \ref cy_mqtt_connect_info_t for details.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_connect( cy_mqtt_t mqtt_handle, cy_mqtt_connect_info_t *connect_info );

/**
 * Gets the MQTT handle associated with a user defined descriptor passed in \ref cy_mqtt_create.
 *
 * This API is supported in multi-core environment and can be invoked from the secondary core application.
 *
 * @param mqtt_handle [out]  : Pointer to store the MQTT handle that corresponds to the descriptor.
 * @param descriptor [in]    : NULL terminated string that uniquely identifies the MQTT handle.
                               Note: The maximum permissible length of the string is 20 characters (excluding the null character).
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_get_handle( cy_mqtt_t *mqtt_handle, char *descriptor );

/**
 * Publishes the MQTT message on given MQTT topic.
 *
 * This API is supported in multi-core environment and can be invoked from the secondary core application.
 *
 * @param mqtt_handle [in]   : MQTT handle created using \ref cy_mqtt_create.
 * @param pub_msg [in]       : MQTT publish message information. Refer \ref cy_mqtt_publish_info_t for details.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_publish( cy_mqtt_t mqtt_handle, cy_mqtt_publish_info_t *pub_msg );

/**
 * Subscribes for MQTT message on the given MQTT topic or list of topics.
 *
 * This API is supported in multi-core environment and can be invoked from the secondary core application.
 *
 * \note
 *       1. Single subscription request with multiple topics, this function returns success if at least one of the subscription is successful.
 *          If subscription fails for any of the topic in the list, the failure is indicated through 'allocated_qos' set to \ref CY_MQTT_QOS_INVALID.
 *          Refer \ref cy_mqtt_subscribe_info_t for more details. Upon return, the 'allocated_qos' indicate the QoS level allocated by the MQTT broker for the successfully subscribed topic.
 *       2. Call \ref cy_mqtt_register_event_callback before calling this API in order to be notified of
 *          data available for the subscribed topic(s).
 * @param mqtt_handle [in]   : MQTT handle created using \ref cy_mqtt_create.
 * @param sub_info [in, out] : Pointer to array of MQTT subscription information structure. Refer \ref cy_mqtt_subscribe_info_t for details.
 * @param sub_count [in]     : Number of subscription topics in the subscription array.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_subscribe( cy_mqtt_t mqtt_handle, cy_mqtt_subscribe_info_t *sub_info, uint8_t sub_count );

/**
 * Registers an event callback for the given MQTT handle.
 *
 * This API is supported in multi-core environment and can be invoked as a virtual API from the secondary core application.
 *
 * @param mqtt_handle [in]    : MQTT handle created using \ref cy_mqtt_create.
 * @param event_callback [in] : Application callback function which needs to be called on arrival of MQTT incoming publish packets and network disconnection notification from network layer.
 * @param user_data [in]      : Pointer to user data to be passed in the event callback.
 *
 * @return cy_rslt_t          : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_register_event_callback( cy_mqtt_t mqtt_handle,
                                           cy_mqtt_callback_t event_callback,
                                           void *user_data );

/**
 * Deregisters an event callback for the given MQTT handle which was previously registered using \ref cy_mqtt_register_event_callback API.
 *
 * This API is supported in multi-core environment and can be invoked as a virtual API from the secondary core application.
 *
 * @param mqtt_handle [in]    : MQTT handle created using \ref cy_mqtt_create.
 * @param event_callback [in] : Callback function which needs to be deregistered.
 *
 * @return cy_rslt_t          : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_deregister_event_callback( cy_mqtt_t mqtt_handle,
                                             cy_mqtt_callback_t event_callback);

/**
 * Unsubscribes from a given MQTT topic.
 *
 * This API is supported in multi-core environment and can be invoked as a virtual API from the secondary core application.
 *
 * @param mqtt_handle [in]   : MQTT handle created using \ref cy_mqtt_create.
 * @param unsub_info [in]    : Pointer to array of MQTT unsubscription information structure. Refer \ref cy_mqtt_unsubscribe_info_t for details.
 * @param unsub_count [in]   : Number of unsubscription topics in the unsubscription array.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_unsubscribe( cy_mqtt_t mqtt_handle, cy_mqtt_unsubscribe_info_t *unsub_info, uint8_t unsub_count );

/**
 * Initiates MQTT client disconnection from connected MQTT broker.
 *
 * @param mqtt_handle [in]   : MQTT handle created using \ref cy_mqtt_create.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_disconnect( cy_mqtt_t mqtt_handle );

/**
 * Deletes the given MQTT instance and frees the resources allocated for the instance by the \ref cy_mqtt_create function.
 * Before calling this API function, MQTT connection with broker must be disconnected. And the MQTT handle should not be used after delete.
 *
 * @param mqtt_handle [in]   : MQTT handle created using \ref cy_mqtt_create.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_delete( cy_mqtt_t mqtt_handle );

/**
 * One-time deinitialization function for network sockets implementation.
 * It should be called after destroying all network socket connections.
 *
 * This API is supported in multi-core environment and can be invoked on the secondary core application
 * to clean up the MQTT virtual-only stack implementation. It should be called when the MQTT virtual
 * APIs are no longer needed.
 *
 * \note \ref cy_mqtt_init and \ref cy_mqtt_deinit API functions are not thread-safe. The caller
 *       must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 * @return cy_rslt_t         : CY_RSLT_SUCCESS on success; error codes in @ref mqtt_defines otherwise.
 */
cy_rslt_t cy_mqtt_deinit( void );

/**
 * @}
 */

#if defined(__cplusplus)
}
#endif

#endif // CY_MQTT_API_H_
