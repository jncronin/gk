/*
 * Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
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
 *  Defines the Secure Sockets Interface.
 *  
 *  This file provides functions to communicate over the IP network.
 *  The interface is broadly based on the POSIX sockets API function, but provides a subset and tailored to RTOS needs.
 *  The following additional support has been added on top of the standard POSIX sockets functionality:
 *  - CY_SOCKET_IPPROTO_TLS protocol type added for the socket_create API function to support secure connections with TLS.
 *  - Options added to \ref cy_socket_setsockopt API function to:
 *    1. Configure TLS-specific parameters such as RootCA, certificate/key pair, server name for Server Name Indication (SNI) TLS extension, and Application-Layer Protocol Negotiation (ALPN) protocol list.
 *    2. Support incoming connect request (for server only), receive, and disconnect callbacks.
 *
 */

/**
 * \defgroup group_secure_sockets Secure Sockets API
 * \brief The Secure Sockets API provides functions to communicate over the IP network. The interface is broadly based on the POSIX socket APIs, but implements a subset and is tailored to RTOS needs.
 * \addtogroup group_secure_sockets
 * \{
 * \defgroup group_secure_sockets_mscs Message Sequence Charts
 * \defgroup group_secure_sockets_macros Macros
 * \defgroup group_secure_sockets_typedefs Typedefs
 * \defgroup group_secure_sockets_enums Enumerated types
 * \defgroup group_secure_sockets_structures Structures
 * \defgroup group_secure_sockets_functions Functions
 */

/**
*
* \addtogroup group_secure_sockets_mscs
* \{
*
********************************************************************************
* \section section_secure_connect Secure Connect
********************************************************************************
*
* \image html uml_secure_connect.png

********************************************************************************
* \section section_server_async Server Connect Async
********************************************************************************
*
* \image html uml_secure_server_async_connect.png
*
********************************************************************************
* \section section_rw_read Write and Read Async
********************************************************************************
*
* \image html uml_secure_write_and_read_async.png
*
* \}
*
*/

#ifndef INCLUDED_CY_SECURE_SOCKETS_INTERFACE_H_
#define INCLUDED_CY_SECURE_SOCKETS_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cy_result.h"
#include "cy_secure_sockets_error.h"
#include "cy_secure_sockets_constants.h"

/******************************************************
 *                      Constants
 ******************************************************/

/**
 * \addtogroup group_secure_sockets_enums
 * \{
 */
/******************************************************
 *                      Enums
 ******************************************************/
/**
 * IP Version
 */
typedef enum
{
    CY_SOCKET_IP_VER_V4 = 4, /**< IPv4 protocol */
    CY_SOCKET_IP_VER_V6 = 6  /**< IPv6 protocol */
} cy_socket_ip_version_t;

/**
 * Options for socket option \ref CY_SOCKET_SO_TLS_AUTH_MODE
 */
typedef enum
{
    CY_SOCKET_TLS_VERIFY_NONE = 0,     /**< Peer certificate is not checked (default authentication mode for server sockets). */
    CY_SOCKET_TLS_VERIFY_OPTIONAL = 1, /**< Peer certificate is checked, but the handshake continues even if verification fails. */
    CY_SOCKET_TLS_VERIFY_REQUIRED = 2  /**< Peer must present a valid certificate; handshake is aborted if verification failed (default authentication mode for client sockets). */
} cy_socket_tls_auth_mode_t;

/**
 * Options for socket option \ref CY_SOCKET_SO_BINDTODEVICE
 */
typedef enum
{
    CY_SOCKET_STA_INTERFACE  = 0,   /**< STA or Client Interface    */
    CY_SOCKET_AP_INTERFACE   = 1,   /**< softAP Interface           */
    CY_SOCKET_ETH0_INTERFACE = 2,   /**< Ethernet Interface index 0 */
    CY_SOCKET_ETH1_INTERFACE = 3    /**< Ethernet Interface index 1 */
} cy_socket_interface_t;

/** \} group_secure_sockets_enums */

/**
 * \addtogroup group_secure_sockets_typedefs
 * \{
 */
/******************************************************
 *                      Typedefs
 ******************************************************/
/**
 * Socket handle.
 * \note The socket handle is created and returned to the caller of the \ref cy_socket_create function.
 */
typedef void * cy_socket_t;

/**
 * Socket callback functions type used to set connect, receive, and disconnect callbacks
 * using the \ref cy_socket_setsockopt function.
 */
typedef cy_rslt_t (*cy_socket_callback_t)(cy_socket_t socket_handle, void *arg);

/** \} group_secure_sockets_typedefs */

/**
 * \addtogroup group_secure_sockets_structures
 * \{
 */

/**
 * IP Address Structure
 */
typedef struct cy_socket_ip_address
{
    cy_socket_ip_version_t version; /**< IP version \ref cy_socket_ip_version_t */

    union
    {
        uint32_t v4;    /**< IPv4 address in network byte order. */
        uint32_t v6[4]; /**< IPv6 address in network byte order. */
    } ip; /**< IP address bytes. */
} cy_socket_ip_address_t;

/**
 * Socket Address
 */
typedef struct cy_socket_sockaddr
{
    uint16_t                  port;       /**< Port Number in host byte order. */
    cy_socket_ip_address_t    ip_address; /**< IP Address. */
} cy_socket_sockaddr_t;

/**
 * Option value type for \ref CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK, \ref CY_SOCKET_SO_RECEIVE_CALLBACK,
 * and \ref CY_SOCKET_SO_DISCONNECT_CALLBACK socket options.
 */
typedef struct cy_socket_opt_callback
{
    cy_socket_callback_t callback; /**< Pointer to a caller-defined callback function. */
    void * arg;                    /**< Caller-defined context to be used with the callback function. */
} cy_socket_opt_callback_t;

/**
 * Option value type for \ref CY_SOCKET_SO_JOIN_MULTICAST_GROUP, and \ref CY_SOCKET_SO_LEAVE_MULTICAST_GROUP socket options.
 */
typedef struct cy_socket_ip_mreq {
    cy_socket_ip_address_t multi_addr; /**< IP multicast address of the group. */
    cy_socket_ip_address_t if_addr;    /**< Local IP address of the interface. */
} cy_socket_ip_mreq_t;

/** \} group_secure_sockets_structures */

/**
 * \addtogroup group_secure_sockets_functions
 * \{
 * All API functions except \ref cy_socket_init and \ref cy_socket_deinit are thread-safe.
 *
 * All API functions are blocking API functions.
 *
 * Secure Sockets Library creates a worker thread for processing events from the network stack.
 * The priority of the thread is CY_RTOS_PRIORITY_ABOVENORMAL. The macro CY_RTOS_PRIORITY_ABOVENORMAL
 * is defined in abstraction-rtos/include/COMPONENT_FREERTOS/cyabs_rtos_impl.h.
 */
/******************************************************
 *                      Function Prototypes
 ******************************************************/

/**
 * Does general allocation and initialization of resources needed for the library.
 * This API function must be called before using any other socket API.
 *
 * \note \ref cy_socket_init and \ref cy_socket_deinit API functions are not thread-safe. The caller
 *       must ensure that these two API functions are not invoked simultaneously from different threads.
 * @return     CY_RSLT_SUCCESS on success; an error code on failure.
 */
cy_rslt_t cy_socket_init( void );

/**
 * Releases the resources allocated in \ref cy_socket_init function. Prior to calling this API function,
 * all created sockets must be disconnected and deleted.
 *
 * \note \ref cy_socket_init and \ref cy_socket_deinit API functions are not thread-safe. The caller
 *       must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure.
 *             Important error code related to this API function is: \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOT_INITIALIZED
 */
cy_rslt_t cy_socket_deinit( void );

/**
 * Creates a new socket.
 *
 * \note
 * 1. \ref cy_socket_create() function creates a dual-stack socket if domain passed is \ref CY_SOCKET_DOMAIN_AF_INET6.
 * 2. Secure Sockets Library configures default send and receive timeout values to 10 seconds for a newly created socket.
 *    These default values can be overridden using the \ref cy_socket_setsockopt API. Adjust the default timeout values based on the network speed or use case.
 *    For example, to change the send timeout, use the \ref CY_SOCKET_SO_SNDTIMEO socket option; similarly, for receive timeout, use the \ref CY_SOCKET_SO_RCVTIMEO socket option.
 *
 * 3. Secure Socket Library sets default TLS authentication mode for TLS client sockets to \ref CY_SOCKET_TLS_VERIFY_REQUIRED. For TLS server sockets,
 *    it sets the default TLS authentication mode to CY_SOCKET_TLS_VERIFY_NONE. To override the default authentication mode,
 *    use \ref cy_socket_setsockopt with the \ref CY_SOCKET_SO_TLS_AUTH_MODE socket option.
 * 4. To use a socket for multicast operations over SoftAP interface, user must bind the socket to the SoftAP interface using \ref CY_SOCKET_SO_BINDTODEVICE socket option after creating the socket.
 *
 * Valid type/protocol combinations are:
 *   - \ref CY_SOCKET_TYPE_STREAM with \ref CY_SOCKET_IPPROTO_TCP or \ref CY_SOCKET_IPPROTO_TLS
 *   - \ref CY_SOCKET_TYPE_DGRAM  with \ref CY_SOCKET_IPPROTO_UDP
 *
 * @param[in]  domain   Protocol family to be used by the socket.
 *                      Refer \ref CY_SOCKET_DOMAIN_AF_INET and \ref CY_SOCKET_DOMAIN_AF_INET6.
 * @param[in]  type     Protocol type to be used by the socket.
 *                      Refer \ref CY_SOCKET_TYPE_DGRAM and \ref CY_SOCKET_TYPE_STREAM.
 * @param[in]  protocol Transport protocol to be used by the socket.
 *                      Refer \ref CY_SOCKET_IPPROTO_TCP, \ref CY_SOCKET_IPPROTO_UDP, and \ref CY_SOCKET_IPPROTO_TLS.
 * @param[out] handle   Socket handle; contents of this handle are specific to the socket layer implementation.
 * @return     CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns the socket handle.
 *             Important error codes related to this API function are: \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST
 */
cy_rslt_t cy_socket_create(int domain, int type, int protocol, cy_socket_t *handle);

/**
 * Connects a TCP/TLS socket to the specified server IP address and port. This API function is a blocking call.
 *
 * For secure (TLS) sockets, before calling this API function, the following TLS configuration can be set:
 * 1. RootCA using the \ref cy_tls_load_global_root_ca_certificates or \ref cy_socket_setsockopt API function with \ref CY_SOCKET_SO_TRUSTED_ROOTCA_CERTIFICATE.
 * 2. Certificate/key pair with \ref cy_tls_create_identity and \ref cy_socket_setsockopt with \ref CY_SOCKET_SO_TLS_IDENTITY.
 * 3. For TLS client sockets, the default authentication mode set is \ref CY_SOCKET_TLS_VERIFY_REQUIRED. To override the default authentication mode,
 *    use \ref cy_socket_setsockopt with the \ref CY_SOCKET_SO_TLS_AUTH_MODE socket option.
 * 4. The default mbed TLS configuration provided by the *Wi-Fi Middleware Core Library* disables the validity period verification of the certificates.
 *    To perform this verification, enable MBEDTLS_HAVE_TIME_DATE in the [mbedtls_user_config.h] (https://github.com/cypresssemiconductorco/wifi-mw-core/blob/master/configs/mbedtls_user_config.h) file. Ensure that the system time is set prior to the \ref cy_socket_connect() function call.
 *    To set the system time, get the time from the NTP server and set the system's RTC time using cyhal_rtc_init(), cyhal_rtc_write() and cy_set_rtc_instance() functions. See the [Time Support Details](https://github.com/cypresssemiconductorco/clib-support/blob/master/README.md#time-support-details)
 *    for reference. See the \ref snip12 to get the time from NTP server.
 *
 * \note This is applicable if Secure Sockets Library is built for lwIP network stack. If this function returns \ref CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED error, the socket handle cannot be reused to establish the connection. The caller should invoke \ref cy_socket_delete API function to delete the current socket handle,
 * and create a new socket handle using \ref cy_socket_create API function to re-establish the connection. This is required, due to the limitation of lwIP stack.
 *
 * @param[in] handle         Socket handle returned by the \ref cy_socket_create API function.
 * @param[in] address        Pointer to the \ref cy_socket_sockaddr_t structure that contains the address to connect the socket to. Refer \ref cy_socket_ip_address_t for IP address endienness.
 * @param[in] address_length Length of the \ref cy_socket_sockaddr_t structure.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TLS_ERROR \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM
 */
cy_rslt_t cy_socket_connect(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t address_length);

/**
 * Disconnects a TCP/TLS socket's remote connection.
 * Timeout is not supported by all network stacks. If the underlying network stack does not support
 * the timeout option, this function returns after a clean disconnect. lwIP does not support the timeout option.
 *
 * \note Ensure that this API function is also called when the socket send/receive API fails with the error \ref CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED.
 *
 * @param[in] handle         Socket handle returned by either the \ref cy_socket_create API function for client sockets,
 *                           or by the \ref cy_socket_accept API function for accepted sockets.
 *
 * @param[in] timeout        Maximum amount of time to wait in milliseconds for a clean disconnect.
 *                           When the timeout is zero, the function returns after a clean disconnect or when the operation results in an error.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED
 */
cy_rslt_t cy_socket_disconnect(cy_socket_t handle, uint32_t timeout);

/**
 * Binds the socket to the given socket address.
 *
 * @param[in] handle         Socket handle returned by the \ref cy_socket_create API function.
 * @param[in] address        Address to be bound to the socket. Refer \ref cy_socket_ip_address_t for IP address endienness.
 * @param[in] address_length Length of the \ref cy_socket_sockaddr_t structure.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_PROTOCOL_NOT_SUPPORTED
 */
cy_rslt_t cy_socket_bind(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t address_length);

/**
 * Listens for TCP/TLS socket connections and limits the queue of incoming connections.
 *
 * If the socket has been configured with a connection request callback, the registered callback will be invoked
 * when the new client connection request is received. Invoke the \ref cy_socket_accept API function from the callback function
 * to accept the client connection.
 *
 * @param[in] handle        Socket handle returned by the \ref cy_socket_create API function.
 * @param[in] backlog       Maximum pending connections allowed.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR
 */
cy_rslt_t cy_socket_listen(cy_socket_t handle, int backlog);

/**
 * Accepts a new TCP/TLS connection on a socket.
 *
 * This is a blocking API function that returns when there is an incoming connection request from a client.
 *
 * However, when the \ref CY_SOCKET_SO_RCVTIMEO socket option is set on the listening socket (input socket handle param),
 * this API function returns with a \ref CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT error if there is no
 * connection request from a client within the timeout period.
 *
 * \ref CY_SOCKET_SO_RCVTIMEO can be set using the \ref cy_socket_setsockopt API function.
 *
 *
 * @param[in]  handle          Socket handle that has been created with \ref cy_socket_create,
 *                             bound to a local address with \ref cy_socket_bind,
 *                             and is listening for connections after a call to \ref cy_socket_listen.
 *                             This is the server-side socket that is reused to establish
 *                             connections across clients.
 * @param[out] address         Address of the peer socket in the cy_socket_sockaddr_t structure. Refer \ref cy_socket_ip_address_t for IP address endienness.
 * @param[out] address_length  Contains the actual size of the peer socket address.
 * @param[out] socket          Socket handle for the accepted connection with a client. This is
 *                             the socket that should be used for further communication over a
 *                             new client connection.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure.
 *             Important error codes related to this API function are: \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_TLS_ERROR \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOT_LISTENING
 */
cy_rslt_t cy_socket_accept(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t *address_length, cy_socket_t *socket);

/**
 * Sends data over a connected TCP/TLS socket.
 *
 * \note Ensure that the \ref cy_socket_disconnect API function is called when this API function fails with the CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED error.
 * lwIP doesn't allow the socket to be reused after disconnection. Therefore, the caller should call \ref cy_socket_disconnect and \ref cy_socket_delete API functions
 * to delete the closed socket. This is applicable if Secure Sockets Library is built for lwIP network stack.
 *
 * @param[in]  handle        Socket handle returned by either the \ref cy_socket_create API function for client sockets,
 *                           or by the \ref cy_socket_accept API function for accepted sockets.
 * @param[in]  buffer        Buffer containing the data to be sent.
 * @param[in]  length        Length of the data to be sent.
 * @param[in]  flags         Flags to indicate the send options:
 *                           \ref CY_SOCKET_FLAGS_NONE and \ref CY_SOCKET_FLAGS_MORE
 * @param[out] bytes_sent    Number of bytes sent.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns the number of bytes sent.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TLS_ERROR \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK
 */
cy_rslt_t cy_socket_send(cy_socket_t handle, const void *buffer, uint32_t length, int flags, uint32_t *bytes_sent);

/**
 * Sends a UDP datagram over a specified socket.
 *
 * \note If an ARP entry for the specified destination address is not present in the ARP cache, this function sends an ARP request and waits for
 *       \ref ARP_WAIT_TIME_IN_MSEC time for MAC address resolution. If the MAC address is not resolved within the timeout, this function
 *       returns the error code \ref CY_RSLT_MODULE_SECURE_SOCKETS_ARP_TIMEOUT. Upon receiving this error, the caller can retry this API again after a brief delay.
 *
 * @param[in]  handle         Socket handle returned by the \ref cy_socket_create API function for UDP sockets.
 * @param[in]  buffer         Buffer containing the data to be sent.
 * @param[in]  length         Length of the data to be sent.
 * @param[in]  flags          Flags to indicate send options. Currently, this argument is not used; this is reserved for the future.
 * @param[in]  dest_addr      Pointer to the \ref cy_socket_sockaddr_t structure that contains the destination
 *                            address to send the data to. Refer \ref cy_socket_ip_address_t for IP address endienness.
 * @param[in]  address_length Length of the \ref cy_socket_sockaddr_t structure.
 * @param[out] bytes_sent     Number of bytes sent.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns the number of bytes sent.
 *            Important error code related to this API function is: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_ARP_TIMEOUT \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_ERROR_ROUTING
 */
cy_rslt_t cy_socket_sendto(cy_socket_t handle, const void *buffer, uint32_t length, int flags, const cy_socket_sockaddr_t *dest_addr, uint32_t address_length, uint32_t *bytes_sent);

/**
 * Receives the data from a connected TCP/TLS socket.
 *
 * \note Ensure that the \ref cy_socket_disconnect API function is called when this API function fails with the CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED error.
 * lwIP doesn't allow the socket to be reused after disconnection. Therefore, the caller should call \ref cy_socket_disconnect and \ref cy_socket_delete API functions
 * to delete the closed socket. This is applicable if Secure Sockets Library is built for lwIP network stack.
 *
 * @param[in]  handle         Socket handle returned by either the \ref cy_socket_create API function for client sockets,
 *                            or by \ref cy_socket_accept API function for accepted sockets.
 * @param[out] buffer         Buffer into which received data will be placed.
 * @param[in]  length         Size of the data buffer.
 * @param[in]  flags          Not currently used. Should be set to \ref CY_SOCKET_FLAGS_NONE.
 * @param[out] bytes_received Number of bytes received.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns number of bytes received.
 *             Important error codes related to this API function are: \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_TLS_ERROR \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK
 */
cy_rslt_t cy_socket_recv(cy_socket_t handle, void *buffer, uint32_t length, int flags, uint32_t *bytes_received);

/**
 * Receives a UDP datagram for the specified socket.
 *
 * \note
 * 1. If the datagram is larger than the supplied buffer, excess bytes in the datagram is discarded.
 * 2. To receive data from a specific source address, the caller should set \ref CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER flag in the 'flags' parameter, and assign
 *    the source address through the 'src_addr' parameter. If \ref CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER flag is set and src_addr is NULL, this function
 *    returns with the error code \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG.
 *
 * @param[in]      handle             Socket handle returned by either the \ref cy_socket_create API function for client sockets,
 *                                    or by \ref cy_socket_accept API function for accepted sockets.
 * @param[out]     buffer             Buffer into which the received data will be placed.
 * @param[in]      length             Size of the data buffer.
 * @param[in]      flags              Flags to control the datagram to be received.
 *                                    Refer \ref CY_SOCKET_FLAGS_RECVFROM_NONE and \ref CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER.
 * @param[in, out] src_addr           Pointer to the \ref cy_socket_sockaddr_t structure to store the sender's address.
 *                                    If passed as NULL, the sender's address is not returned to the caller.
 *                                    If the \ref CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER flag is set, it contains the source address from which the datagram to be received.
 *                                    Refer \ref cy_socket_ip_address_t for IP address endienness.
 * @param[in]      src_addr_length    Pointer containing source address length. Currently, this argument is not used; this is reserved for the future.
 * @param[out]     bytes_received     Number of bytes received.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure. On success, it returns the number of bytes received. If src_addr is not NULL it also returns sender address.
 *             Important error code related to this API function is: \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *             \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG
 */
cy_rslt_t cy_socket_recvfrom(cy_socket_t handle, void *buffer, uint32_t length, int flags, cy_socket_sockaddr_t *src_addr, uint32_t *src_addr_length, uint32_t *bytes_received);

/**
 * Sets a particular socket option.
 * This API function can be called multiple times for the same socket to set various socket options.
 *
 * @param[in] handle         Handle of the socket to set the option for.
 * @param[in] level          Level at which the option resides:\n
 *                           \ref CY_SOCKET_SOL_SOCKET \n
 *                           \ref CY_SOCKET_SOL_TCP \n
 *                           \ref CY_SOCKET_SOL_TLS
 * @param[in] optname        Socket option to be set: \n
 *                           \ref CY_SOCKET_SO_RCVTIMEO \n
 *                           \ref CY_SOCKET_SO_SNDTIMEO \n
 *                           \ref CY_SOCKET_SO_NONBLOCK \n
 *                           \ref CY_SOCKET_SO_TCP_USER_TIMEOUT \n
 *                           \ref CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK \n
 *                           \ref CY_SOCKET_SO_RECEIVE_CALLBACK \n
 *                           \ref CY_SOCKET_SO_DISCONNECT_CALLBACK \n
 *                           \ref CY_SOCKET_SO_TRUSTED_ROOTCA_CERTIFICATE \n
 *                           \ref CY_SOCKET_SO_TLS_IDENTITY \n
 *                           \ref CY_SOCKET_SO_SERVER_NAME_INDICATION \n
 *                           \ref CY_SOCKET_SO_ALPN_PROTOCOLS \n
 *                           \ref CY_SOCKET_SO_TLS_AUTH_MODE \n
 *                           \ref CY_SOCKET_SO_TLS_MFL \n
 *                           \ref CY_SOCKET_SO_JOIN_MULTICAST_GROUP \n
 *                           \ref CY_SOCKET_SO_LEAVE_MULTICAST_GROUP \n
 *                           \ref CY_SOCKET_SO_IP_MULTICAST_TTL \n
 *                           \ref CY_SOCKET_SO_BROADCAST \n
 *                           \ref CY_SOCKET_SO_IP_TOS
 * @param[in] optval         A buffer containing the value of the option to set.
 * @param[in] optlen         The length of the buffer pointed to by optval.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_ADDRESS_IN_USE \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_MAX_MEMBERSHIP_ERROR \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_MULTICAST_ADDRESS_NOT_REGISTERED \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED
 */
cy_rslt_t cy_socket_setsockopt(cy_socket_t handle, int level, int optname, const void *optval, uint32_t optlen);

/**
 * Gets the value of a particular socket option.
 *
 * @param[in] handle         Handle of the socket to get the option value for.
 * @param[in] level          Level at which the option resides:\n
 *                           \ref CY_SOCKET_SOL_SOCKET \n
 *                           \ref CY_SOCKET_SOL_TCP \n
 *                           \ref CY_SOCKET_SOL_TLS
 * @param[in] optname        Socket options: \n
 *                           \ref CY_SOCKET_SO_RCVTIMEO \n
 *                           \ref CY_SOCKET_SO_SNDTIMEO \n
 *                           \ref CY_SOCKET_SO_NONBLOCK \n
 *                           \ref CY_SOCKET_SO_NWRITE \n
 *                           \ref CY_SOCKET_SO_TCP_USER_TIMEOUT \n
 *                           \ref CY_SOCKET_SO_SERVER_NAME_INDICATION \n
 **                          \ref CY_SOCKET_SO_TLS_AUTH_MODE \n
 **                          \ref CY_SOCKET_SO_IP_TOS
 * @param[out] optval        Buffer containing the value of the option to get.
 * @param[in, out] optlen    Length of the option value. It is a value-result argument;
 *                           the caller provides the size of the buffer pointed to by optval,
 *                           and is modified by this function on return to indicate
 *                           the actual size of the value returned.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_PROTOCOL_NOT_SUPPORTED \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED
 */
cy_rslt_t cy_socket_getsockopt(cy_socket_t handle, int level, int optname, void *optval, uint32_t *optlen);

/**
 * Resolves a host name using Domain Name Service.
 *
 * @param[in]  hostname        Hostname to resolve. It should be a null-terminated string containing ASCII characters.
 * @param[in]  ip_ver          IP version type \ref cy_socket_ip_version_t for which the hostname has to be resolved.
 * @param[out] addr            IP address of the specified host. Refer \ref cy_socket_ip_address_t for IP address endienness.
 *
 * @return    On success it returns CY_RSLT_SUCCESS and the IP address of the specified host. Returns an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_HOST_NOT_FOUND
 */
cy_rslt_t cy_socket_gethostbyname(const char *hostname, cy_socket_ip_version_t ip_ver, cy_socket_ip_address_t *addr);

/**
 * Checks whether data is available on the socket.
 *
 * @param[in]      handle        Socket handle returned by either the \ref cy_socket_create API function for client sockets,
 *                               or by \ref cy_socket_accept API function for accepted sockets.
 * @param[in, out] rwflags       On input, the flags indicate whether the socket needs to be polled for read/write/read-write operation.
 *                               On return, the flags are updated to indicate the status of the socket readiness for a read/write/read-write operation.
 * @param[in]      timeout       Maximum amount of time in milliseconds to wait before returning. If timeout is zero, the function
 *                               returns immediately. If timeout is \ref CY_SOCKET_NEVER_TIMEOUT, the function waits indefinitely.
 *
 * @return    On success, it returns CY_RSLT_SUCCESS. The CY_SOCKET_POLL_READ flag is set
 *            in the rwflags parameter if data is available for read. The CY_SOCKET_POLL_WRITE flag is set
 *            if the socket is ready for write operations. Returns an error code on failure.
 *            Important error codes related to this API function are: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED
  */
cy_rslt_t cy_socket_poll(cy_socket_t handle, uint32_t *rwflags, uint32_t timeout);

/**
 * Shuts down the send and/or receive operation on the given TCP socket.
 *
 * \note This API is not applicable for UDP.
 *
 * @param[in] handle        Socket handle.
 * @param[in] how           Socket shutdown modes. Supported modes: \ref CY_SOCKET_SHUT_RD,
 *                          \ref CY_SOCKET_SHUT_WR, and \ref CY_SOCKET_SHUT_RDWR.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error code related to this API function is: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_BADARG
 */
cy_rslt_t cy_socket_shutdown(cy_socket_t handle, int how);

/**
 * Releases the resources allocated for the socket by the \ref cy_socket_create function.
 *
 * @param[in] handle        Socket handle returned by the \ref cy_socket_create API function.
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error code related to this API function is: \n
 *            \ref CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET
 */
cy_rslt_t cy_socket_delete(cy_socket_t handle);

/** \} group_secure_sockets_functions */
#ifdef __cplusplus
} /*extern "C" */
#endif
#endif /* ifndef INCLUDED_CY_SECURE_SOCKETS_INTERFACE_H_ */

/** \} group_secure_sockets */
