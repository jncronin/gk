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
 *  Constants for secure sockets library
 */

#ifndef INCLUDED_CY_SECURE_SOCKETS_CONSTANTS_H_
#define INCLUDED_CY_SECURE_SOCKETS_CONSTANTS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup group_secure_sockets_macros
 * \{
 */
/******************************************************
 *                      Constants
 ******************************************************/

/**
 * Assigned to a \ref cy_socket_t variable when the socket handle is not valid.
 */
#define CY_SOCKET_INVALID_HANDLE        ( ( cy_socket_t ) ~0U )

/**
 * Default socket receive timeout value, in milliseconds.
 */
#define DEFAULT_RECV_TIMEOUT_IN_MSEC    10000

/**
 * Default socket send timeout value, in milliseconds.
 */
#define DEFAULT_SEND_TIMEOUT_IN_MSEC    10000

/**
 *  Maximum time in milliseconds the \ref cy_socket_sendto function waits for MAC address-to-IP address resolution.
 */
#define ARP_WAIT_TIME_IN_MSEC           30000

/*
 * Options for the domain parameter of the \ref cy_socket_create() function. Values match that of POSIX sockets.
 */
#define CY_SOCKET_DOMAIN_AF_INET        ( 2 )   /**< Domain option for \ref cy_socket_create() - IPv4 internet protocols.*/
#define CY_SOCKET_DOMAIN_AF_INET6       ( 10 )  /**< Domain option for \ref cy_socket_create() - IPv6 internet protocols.*/

/*
 * Options for the type parameter of the \ref cy_socket_create() function.
 */
#define CY_SOCKET_TYPE_DGRAM     ( 2 )   /**< Type parameter for \ref cy_socket_create() - Datagram. */
#define CY_SOCKET_TYPE_STREAM    ( 1 )   /**< Type parameter for \ref cy_socket_create() - Byte-stream. */

/*
 * Options for the protocol parameter of the \ref cy_socket_create() function.
 */
#define CY_SOCKET_IPPROTO_TCP    ( 1 )   /**< Protocol option for \ref cy_socket_create() - TCP. */
#define CY_SOCKET_IPPROTO_UDP    ( 2 )   /**< Protocol option for \ref cy_socket_create() - UDP. */
#define CY_SOCKET_IPPROTO_TLS    ( 3 )   /**< Protocol option for \ref cy_socket_create() - TLS. */

/*
 * Options for the level parameter in \ref cy_socket_setsockopt() and cy_socket_getsockopt().
 */
#define CY_SOCKET_SOL_SOCKET     ( 1 )   /**< Level option for \ref cy_socket_setsockopt() - Socket-level option. */
#define CY_SOCKET_SOL_TCP        ( 2 )   /**< Level option for \ref cy_socket_setsockopt() - TCP protocol-level option. */
#define CY_SOCKET_SOL_TLS        ( 3 )   /**< Level option for \ref cy_socket_setsockopt() - TLS protocol-level option. */
#define CY_SOCKET_SOL_IP         ( 4 )   /**< Level option for \ref cy_socket_setsockopt() - IP protocol-level option. */
/*
 * Options for optname in \ref cy_socket_setsockopt() and cy_socket_getsockopt().
 */

/**
 * Set the receive timeout in milliseconds.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the timeout value in uint32_t type.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 *
 * \note Configuring the receive timeout value on a server socket impacts the \ref cy_socket_accept() API function.
 *       If the client does not send a connect request within the timeout, \ref cy_socket_accept() returns the \ref CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT error.
 */
#define CY_SOCKET_SO_RCVTIMEO    ( 0 )

/**
 * Set the send timeout in milliseconds.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding timeout value in uint32_t type.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_SNDTIMEO    ( 1 )

/**
 * Set the blocking status of socket API function calls.
 * This option is currently not supported; will be supported in a future release.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the blocking status in uint8_t type.
 *                   Value "1" indicates blocking status to non-blocking.
 *                   Value "0" indicates blocking status to blocking.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_NONBLOCK    ( 2 )

/**
 * Enable/disable the TCP keepalive mechanism on the socket.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the keepalive configuration in int type.
 *                   Value "1" indicates enable TCP keepalive.
 *                   Value "0" indicates disable TCP keepalive.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE     ( 3 )

/**
 * Set the interval in milliseconds between TCP keepalive probes.
 * Keepalives are sent only when the feature is enabled
 * with the \ref CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE socket option.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding value of the keepalive interval in uint32_t type.
 *   * Level: \ref CY_SOCKET_SOL_TCP
 */
#define CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL   ( 4 )

/**
 * Set the maximum number of TCP keepalive probes to be sent before
 * giving up and killing the connection if no response is obtained
 * from the other end. Keepalives are sent only when the feature is
 * enabled with the \ref CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE socket option.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the value of maximum keepalive probe count in uint32_t type.
 *   * Level: \ref CY_SOCKET_SOL_TCP
 */
#define CY_SOCKET_SO_TCP_KEEPALIVE_COUNT      ( 5 )

/**
 * Set the duration for which the connection needs to be idle (in milliseconds) before
 * TCP begins sending out keepalive probes. Keepalive probes are sent only when the
 * feature is enabled with \ref CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE socket option.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the value of keepalive idle time in uint32_t type.
 *   * Level: \ref CY_SOCKET_SOL_TCP
 */
#define CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME  ( 6 )

/**
 * Set the callback to be called upon an incoming client connection request.
 * This option is supported only for TCP server sockets.
 * The callback function registered with this option runs in the secure sockets worker thread context.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to \ref cy_socket_opt_callback_t.
 *                   Passing the NULL value de-registers the registered callback.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK ( 7 )

/**
 * Set the callback to be called when the socket has received data.
 * The callback function registered with this option runs in the secure sockets worker thread context.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * \note The registered receive callback function is invoked when a network packet is received from the network stack;
 *       therefore, this callback is invoked per network packet. If the data in the packet is consumed partially, the secure sockets library
 *       doesn't provide another callback for the remaining data in that packet. Therefore, it is recommended to read the entire network packet
 *       by providing a buffer of MTU size (1460 bytes) to the \ref cy_socket_recv function.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to \ref cy_socket_opt_callback_t.
 *                   Passing the NULL value de-registers the registered callback.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_RECEIVE_CALLBACK         ( 8 )

/**
 * Set the callback to be called when the socket is disconnected.
 * This option is supported only for TCP sockets.
 * The callback function registered with this option runs in the secure sockets worker thread context.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * \note This callback is invoked whenever the peer disconnects. In the callback, \ref cy_socket_disconnect should be invoked.
 *       This callback will not be invoked during self-initiated disconnections, i.e., when \ref cy_socket_disconnect is called by self.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to \ref cy_socket_opt_callback_t.
 *                   Passing the NULL value de-registers the registered callback.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */

#define CY_SOCKET_SO_DISCONNECT_CALLBACK      ( 9 )

/**
 * Get the number of bytes pending to be sent by the protocol.
 * This option is used only with \ref cy_socket_getsockopt.
 * This option is currently not supported; will be supported in a future release.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to the uint32_t type into which the API function fills the number of bytes.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_NWRITE           ( 10 )

/**
 * Set the user timeout value that controls the duration transmitted data may remain unacknowledged before a connection is forcefully closed.
 * This option is currently not supported; will be supported in a future release.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the timeout value in uint32_t type.
 *   * Level: \ref CY_SOCKET_SOL_TCP
 */
#define CY_SOCKET_SO_TCP_USER_TIMEOUT ( 11 )

/**
 * Set a RootCA certificate specific to the socket, in PEM format.
 * By default, the RootCA certificates loaded with \ref cy_tls_load_global_root_ca_certificates
 * are used to validate the peer's certificate. If a specific RootCA needs to be used for a socket,
 * this socket option should be used to configure a connection-specific RootCA.
 *
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to a buffer (char array) holding the certificate data.
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_TRUSTED_ROOTCA_CERTIFICATE ( 12 )

/**
 * Set the TLS identity.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer (type void) returned by the \ref cy_tls_create_identity function.
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_TLS_IDENTITY               ( 13 )

/**
 * Set the hostname to be used for the TLS server name indication (SNI) extension of TLS ClientHello.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to the stream of bytes (char array) holding the hostname.
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_SERVER_NAME_INDICATION     ( 14 )

/**
 * Set the application protocol list to be included in TLS ClientHello.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to the stream of bytes (char array) with protocol names separated by comma.
 *                   e.g., "h2-16,h2-15,h2-14,h2,spdy/3.1,http/1.1"
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_ALPN_PROTOCOLS ( 15 )

/**
 * Set the TLS authenticate mode.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the TLS authentication mode value of type \ref cy_socket_tls_auth_mode_t.
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_TLS_AUTH_MODE ( 16 )

/**
 * Set the TLS maximum fragment length.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the maximum fragment length value in uint32_t type.
 *                   Allowed values are 512, 1024, 2048, 4096 per https://tools.ietf.org/html/rfc6066#section-4.
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_TLS_MFL       ( 17 )

/**
 * Join an IPv4 (or) IPv6 multicast group.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * \note
 *  1. This implementation supports up to 10 multicast groups in total, and across the sockets. This is due to the underlying Wi-Fi host driver (WHD) limitation.
 *  2. The maximum number of IPv4 and IPv6 multicast groups to be supported can be configured using the configuration options provided by the network stack.
 *     However, if the total count of both IPv4 and IPv6 multicast groups is more than 10, any attempts to join a new multicast group will fail.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the multicast group address and local IP address values in \ref cy_socket_ip_mreq_t structure type.
 *   * Level: \ref CY_SOCKET_SOL_IP
 */
#define CY_SOCKET_SO_JOIN_MULTICAST_GROUP   ( 18 )

/**
 * Leave an IPv4 (or) IPv6 multicast group.
 * This option is not supported in \ref cy_socket_getsockopt.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the multicast group address and local IP address values in \ref cy_socket_ip_mreq_t structure type.
 *   * Level: \ref CY_SOCKET_SOL_IP
 */
#define CY_SOCKET_SO_LEAVE_MULTICAST_GROUP  ( 19 )

/**
 * Set the time-to-live value of outgoing multicast packets for this socket.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the time-to-live value in uint8_t type.
 *                   The value of this socket option should be in the range 0 <= value <= 255.
 *                   For IPv4, this value is measured in seconds; for IPv6, it is measured in hop limit.
 *   * Level: \ref CY_SOCKET_SOL_IP
 */
#define CY_SOCKET_SO_IP_MULTICAST_TTL ( 20 )

/**
 * Enable/disable sending and receiving broadcast messages on the socket.
 *
 * \note Use this socket option only when broadcast filter is enabled in the network stack configuration.
 *       If disabled, broadcast messages cannot be controlled
 *       with this socket option.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the uint8_t value.
 *                   Value "1" enables broadcast messages.
 *                   Value "0" disables broadcast messages.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_BROADCAST       ( 21 )

/**
 * Set the network interface to be used for sending and receiving data on the socket.
 * This socket option can be called more than once for a socket to change the interface it's bound to.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the interface type value in \ref cy_socket_interface_t type.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_BINDTODEVICE    ( 22 )

/**
 * Set the Type-Of-Service (TOS) field that is sent with every IP packet originating from this socket.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the type of service value in uint8_t type.
 *                   The value of this socket option should be in the range 0 <= value <= 255.
 *   * Level: \ref CY_SOCKET_SOL_IP
 */
#define CY_SOCKET_SO_IP_TOS ( 23 )

/**
 * Get the number of bytes immediately available for reading.
 *
 * Arguments related to this optname:
 *   * Option value: Pointer to the uint32_t type into which the API function fills the number of bytes available for reading.
 *   * Level: \ref CY_SOCKET_SOL_SOCKET
 */
#define CY_SOCKET_SO_BYTES_AVAILABLE    ( 24 )

/**
 * Enable/disable TCP no delay socket option.
 * If this option is enabled, Nagle's algorithm is disabled. If disabled, data packets are sent as soon as possible, even if there is only a small amount of data. If enabled, data is buffered until there is a sufficient amount to send out.
 *
 *
 * Arguments related to this optname:
 *   * Option value: Pointer holding the uint8_t value.
 *                   Value "0" enables Nagle's algorithm.
 *                   Value "1" disables Nagle's algorithm.
 *   * Level: \ref CY_SOCKET_SOL_TCP
 */
#define CY_SOCKET_SO_TCP_NODELAY        ( 25 )

#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT

/**
 * Use RootCA certificate which is provisioned
 */
#define CY_SOCKET_ROOTCA_SECURE_STORAGE  ( 0x0 )

/**
 * Use RootCA certificate which is provided from application
 */
#define CY_SOCKET_ROOTCA_RAM             ( 0x1 )

/**
 * Set the RootCA certificate location
 *
 *  If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is enabled, RootCA will be read from the secure element by default.
 *  To override default behaviour, set this socket option.
 *
 *  Arguments related to this optname:
 *   * Option value: Pointer holding the uint8_t value.
 *                   Value \ref CY_SOCKET_ROOTCA_SECURE_STORAGE reads rootCA certificate from secure element.
 *                   Value \ref CY_SOCKET_ROOTCA_RAM reads rootCA certificate from RAM loaded through TLS API
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_ROOTCA_CERTIFICATE_LOCATION       (26)

/**
 * Use device certificate and keys which are provisioned.
 */
#define CY_SOCKET_DEVICE_CERT_KEY_SECURE_STORAGE       ( 0x0 )

/**
 * Use device certificate and keys which are provided from application.
 */
#define CY_SOCKET_DEVICE_CERT_KEY_RAM                  ( 0x1 )

/**
 * Set the device certificate and key location.
 *
 *  If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is enabled, the device certificate and keys will be read from the secure element by default.
 *  To override default behaviour, set this socket option.
 *
 *  Arguments related to this optname:
 *   * Option value: Pointer holding the uint8_t value.
 *                   Value \ref CY_SOCKET_DEVICE_CERT_KEY_SECURE_STORAGE reads the device certificate and key from the secure element.
 *                   Value \ref CY_SOCKET_DEVICE_CERT_KEY_RAM reads the device certificate and key from RAM loaded through the TLS API.
 *   * Level: \ref CY_SOCKET_SOL_TLS
 */
#define CY_SOCKET_SO_DEVICE_CERT_KEY_LOCATION           (27)

#endif

/*
 * \ref cy_socket_send() input flags. One or more flags can be combined.
 */
#define CY_SOCKET_FLAGS_NONE       ( 0x0 ) /**< \ref cy_socket_send() input flags - No flag. */
#define CY_SOCKET_FLAGS_MORE       ( 0x1 ) /**< \ref cy_socket_send() input flags - The caller indicates that there is additional data to be sent. This flag is applicable only for TCP connections. Caller will not set this flag for the last data chunk to be sent. */

/*
 * \ref cy_socket_recvfrom() input flags. One or more flags can be combined.
 */
#define CY_SOCKET_FLAGS_RECVFROM_NONE          ( 0x0 ) /**< \ref cy_socket_recvfrom() input flags - No flag. */
#define CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER    ( 0x1 ) /**< \ref cy_socket_recvfrom() input flags - Used for filtering of input packets. Packets are received only from the user-specified source address. */

/*
 * \ref cy_socket_poll() input flags. One or more flags can be combined.
 */
#define CY_SOCKET_POLL_READ  ( 1 ) /**< \ref cy_socket_poll() input flags - Check for pending data.  */
#define CY_SOCKET_POLL_WRITE ( 2 ) /**< \ref cy_socket_poll() input flags - Check whether write is possible. */

/*
 * Options for "how" parameter of \ref cy_socket_shutdown() function.
 */
#define CY_SOCKET_SHUT_RD    ( 0x1 )                                   /**< Option for the "how" parameter of \ref cy_socket_shutdown() - Disables further receive operations. */
#define CY_SOCKET_SHUT_WR    ( 0x2 )                                   /**< Option for the "how" parameter of \ref cy_socket_shutdown() - Disables further send operations. */
#define CY_SOCKET_SHUT_RDWR  ( CY_SOCKET_SHUT_RD | CY_SOCKET_SHUT_WR ) /**< Option for the "how" parameter of \ref cy_socket_shutdown() - Disables further send and receive. operations. */


/**
 * Never timeout.
 */
#define CY_SOCKET_NEVER_TIMEOUT    ( 0xFFFFFFFFU )

/** \} group_secure_sockets_macros */

#ifdef __cplusplus
} /*extern "C" */
#endif
#endif /* ifndef INCLUDED_CY_SECURE_SOCKETS_CONSTANTS_H_ */
