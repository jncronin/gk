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
 *  Defines the TLS Interface.
 *
 *  This file provides functions for secure communication over the IP network.
 *
 */

/**
 * \defgroup group_cy_tls TLS API
 * \brief The TLS API provides functions for secure communication over the IP network.
 * \addtogroup group_cy_tls
 * \{
 * \defgroup group_cy_tls_enums Enumerations
 * \defgroup group_cy_tls_typedefs Typedefs
 * \defgroup group_cy_tls_structures Structures
 * \defgroup group_cy_tls_functions Functions
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef INCLUDED_CY_TLS_INTERFACE_H_
#define INCLUDED_CY_TLS_INTERFACE_H_

#include "cy_result.h"
#include "cy_result_mw.h"
#include <stdbool.h>
#include <stddef.h>

/******************************************************
 *                      Constants
 ******************************************************/

/******************************************************
 *                      Enums
 ******************************************************/
/**
 * \addtogroup group_cy_tls_enums
 * \{
 */

/**
 * Endpoint type for TLS handshake.
 */
typedef enum
{
    CY_TLS_ENDPOINT_CLIENT = 0, /** Endpoint is a client. */
    CY_TLS_ENDPOINT_SERVER = 1, /** Endpoint is a server. */
} cy_tls_endpoint_type_t;

/**
 * Message digest type for configuring TLS certificate profile.
 */
typedef enum {
    CY_TLS_MD_SHA1              /** The SHA-1 message digest. */
} cy_tls_md_type_t;

/**
 * Minimum RSA key length in bits.
 */
typedef enum {
    CY_TLS_RSA_MIN_KEY_LEN_1024 = 1024,
    CY_TLS_RSA_MIN_KEY_LEN_2048 = 2048,
    CY_TLS_RSA_MIN_KEY_LEN_3072 = 3072,
    CY_TLS_RSA_MIN_KEY_LEN_4096 = 4096,
} cy_tls_rsa_min_key_len_t;
/** \} group_cy_tls_enums */

/******************************************************
 *                      Typedefs
 ******************************************************/

/**
 * \addtogroup group_cy_tls_typedefs
 * \{
 */

/**
 * TLS context type.
 */
typedef void * cy_tls_context_t;

/**
 * Callback function used by the underlying TLS stack for sending the TLS handshake messages and encrypted data over the network.
 *
 * @param[in]  context     User context provided at the time of callback registration.
 * @param[in]  buffer      Buffer of the data to send.
 * @param[in]  length      Length of the buffer.
 * @param[out] bytes_sent  Number of bytes successfully sent over the network.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns the number of bytes sent.
 */
typedef cy_rslt_t ( * cy_network_send_t )( void *context, const unsigned char *buffer, uint32_t length, uint32_t *bytes_sent );

/**
 * Callback function used by the underlying TLS stack for reading TLS handshake messages or the encrypted data from the network.
 *
 * @param[in]  context        User context provided at the time of callback registration.
 * @param[out] buffer         Buffer into which the received data will be placed.
 * @param[in]  length         Size of the buffer.
 * @param[out] bytes_received Number of bytes received.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns the number of bytes received.
 */
typedef cy_rslt_t ( * cy_network_recv_t )( void *context, unsigned char *buffer, uint32_t length, uint32_t *bytes_received );

/** \} group_cy_tls_typedefs */

/**
 * \addtogroup group_cy_tls_structures
 * \{
 */
/**
 * Parameter structure for initializing the TLS interface.
 */
typedef struct cy_tls_params
{
    const char *      rootca_certificate;            /**< RootCA certificate in PEM format. It should be a 'null'-terminated string.*/
    uint32_t          rootca_certificate_length;     /**< RootCA certificate length, excluding the 'null' terminator. */
    const void *      tls_identity;                  /**< Pointer to memory containing the certificate and private key in the underlying TLS stack format. */
    int               auth_mode;                     /**< TLS authentication mode. */
    unsigned char     mfl_code;                      /**< TLS max fragment length code. */
    const char**      alpn_list;                     /**< Application-Layer Protocol Negotiation (ALPN) protocol list to be passed in TLS ALPN extension. */
    char*             hostname;                      /**< Server hostname used with the Server Name Indication (SNI) extension. */
    cy_network_recv_t network_recv;                  /**< Pointer to a caller-defined network receive function. */
    cy_network_send_t network_send;                  /**< Pointer to a caller-defined network send function. */
    void *            context;                       /**< User context. */
#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT
    bool              load_rootca_from_ram;          /**< Flag for setting the RootCA certificate location. */
    bool              load_device_cert_key_from_ram; /**< Flag for setting the device cert & key location. */
#endif
} cy_tls_params_t;

/** \} group_cy_tls_structures */

/**
 * \addtogroup group_cy_tls_functions
 * \{
 * All the API functions except \ref cy_tls_init \ref cy_tls_deinit \ref cy_tls_load_global_root_ca_certificates
 * and \ref cy_tls_release_global_root_ca_certificates are thread-safe.
 *
 * All the API functions are blocking API functions.
 */
/******************************************************
 *                      Function Prototypes
 ******************************************************/
/**
 * Does general allocation and initialization of resources needed for the library.
 * This API function must be called before using any other context-based TLS API functions.
 *
 * \note
 *  1. Helper APIs \ref cy_tls_load_global_root_ca_certificates, \ref cy_tls_release_global_root_ca_certificates,
 *     \ref cy_tls_create_identity, and \ref cy_tls_delete_identity can be called without calling \ref cy_tls_init.
 *
 *  2. \ref cy_tls_init and \ref cy_tls_deinit API functions are not thread-safe.
 *     The caller must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure.
 */
cy_rslt_t cy_tls_init( void );

/**
 * Releases the resources allocated in the \ref cy_tls_init function.
 * \note \ref cy_tls_init and \ref cy_tls_deinit API functions are not thread-safe.
 *        The caller must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 * @return     CY_RSLT_SUCCESS on success; an error code on failure.
 */
cy_rslt_t cy_tls_deinit( void );

/** Initializes the global trusted RootCA certificates used for verifying certificates received during TLS handshake.
 *  This function parses the RootCA certificate chain and converts it to the underlying TLS stack format.
 *  It also stores the converted RootCA in its internal memory. This function overrides previously loaded RootCA certificates.
 *
 *  1. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is disabled:
 *     a. RootCA attached to the context will take first preference.
 *     b. RootCA set as global will take the last preference.
 *  2. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is enabled:
 *     a. By default, RootCA certificates which is provisioned to secure element will be used.
 *     b. If socket option CY_SOCKET_SO_ROOTCA_CERTIFICATE_LOCATION is set with value CY_SOCKET_ROOTCA_RAM then
 *        - RootCA attached to the context will take first preference.
 *        - RootCA set as global will take the last preference.
 *
 *  \note \ref cy_tls_load_global_root_ca_certificates and \ref cy_tls_release_global_root_ca_certificates API functions are not thread-safe.
 *        The caller must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 * @param[in] trusted_ca_certificates  A chain of x509 certificates in PEM or DER format. It should be a null-terminated string.
 *                                     This chain of certificates comprise the public keys of the signing authorities.
 *                                     During the handshake, these public keys are used to verify the authenticity of the peer.
 * @param[in] cert_length              Length of the trusted RootCA certificates excluding the 'null' terminator. The buffer
 *                                     pointed by trusted_ca_certificates is treated as a byte stream.
 *
 * @return cy_rslt_t      CY_RESULT_SUCCESS on success; an error code on failure.
 *                        Important error codes related to this API function are: \n
 *                        CY_RSLT_MODULE_TLS_BADARG \n
 *                        CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE \n
 *                        CY_RSLT_MODULE_TLS_PARSE_CERTIFICATE
 */
cy_rslt_t cy_tls_load_global_root_ca_certificates( const char* trusted_ca_certificates, const uint32_t cert_length );

/** Releases the resources allocated by the \ref cy_tls_load_global_root_ca_certificates API function.
 *
 *  \note \ref cy_tls_load_global_root_ca_certificates and \ref cy_tls_release_global_root_ca_certificates API functions are not thread-safe.
 *        The caller must ensure that these two API functions are not invoked simultaneously from different threads.
 *
 *
 * @return cy_rslt_t    CY_RESULT_SUCCESS on success; an error code on failure.
 */
cy_rslt_t cy_tls_release_global_root_ca_certificates( void );

/**
 * Creates an identity structure from the supplied certificate and private key.
 *
 * 1. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is disabled:
 *    a. TLS identity created using \ref cy_tls_create_identity and set to context using socket option
 *       \ref CY_SOCKET_SO_TLS_IDENTITY will be used for device certificate & key
 * 2. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is enabled:
 *    a. By default, device certificates & private keys which are provisioned to secure element will be used.
 *    b. If socket option CY_SOCKET_SO_DEVICE_CERT_KEY_LOCATION is set with value CY_SOCKET_DEVICE_CERT_KEY_RAM then TLS
 *       identity created with \ref cy_tls_create_identity API and set to context using socket option \ref CY_SOCKET_SO_TLS_IDENTITY
 *       will be used.
 *
 * @param[in] certificate_data       x509 certificate in PEM format. It should be a null-terminated string.
 * @param[in] certificate_len        Length of the certificate excluding the 'null' terminator.
 * @param[in] private_key            Private key in PEM format. It should be a null-terminated string.
 * @param[in] private_key_len        Length of the private key excluding the 'null' terminator.
 * @param[out] tls_identity          Pointer to a memory location containing the certificate and key in the underlying TLS stack format.
 *
 * @return    CY_RSLT_SUCCESS on success; an error code on failure.
 *            Important error codes related to this API function are: \n
 *            CY_RSLT_MODULE_TLS_BADARG \n
 *            CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE \n
 *            CY_RSLT_MODULE_TLS_PARSE_CERTIFICATE \n
 *            CY_RSLT_MODULE_TLS_PARSE_KEY
 */
cy_rslt_t cy_tls_create_identity( const char *certificate_data, const uint32_t certificate_len, const char *private_key, uint32_t private_key_len, void **tls_identity );

/**
 * Releases resources allocated by the \ref cy_tls_create_identity API function.
 *
 * @param[in] tls_identity Pointer to a memory location containing the certificate and key in the underlying TLS stack format.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure.
 *         Important error code related to this API function is: \n
 *         CY_RSLT_MODULE_TLS_BADARG
 */
cy_rslt_t cy_tls_delete_identity( void *tls_identity );

/**
 * Creates a TLS context structure from the input parameters.
 * It allocates a TLS context structure and stores the RootCA, TLS identity,
 * send/receive callback functions, server name to be used in the SNI extension,
 * protocol list to be added to the ALPN extension, and user context.
 * TLS parameters provided by the user are used in later cy_tls API function calls.
 * The memory holding the parameters should not be freed until completely done with using cy_tls API functions.
 *
 * @param[out] context Context handle returned by the TLS layer.
 * @param[in]  params  TLS parameters specified by the caller such as the server certificate.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure.
 *         Important error codes related to this API function are: \n
 *         CY_RSLT_MODULE_TLS_BADARG \n
 *         CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE
 */
cy_rslt_t cy_tls_create_context( cy_tls_context_t *context, cy_tls_params_t *params );

/**
 * Performs a TLS handshake and connects to the server.
 *
 * RootCA certificate will be used for peer certificate verification as below:
 * 1. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is disabled:
 *    a. By default, global RootCA certificate loaded will be used for verification.
 *    b. If context specific RootCA is set using CY_SOCKET_SO_TRUSTED_ROOTCA_CERTIFICATE socket option then
 *       context specific RootCA certificate will be used for verification.
 * 2. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is enabled:
 *    a. By default, RootCA certificates which is provisioned to secure element will be used.
 *    b. If socket option CY_SOCKET_SO_ROOTCA_CERTIFICATE_LOCATION is set with value CY_SOCKET_ROOTCA_RAM then
 *       - RootCA attached to the context will take first preference.
 *       - RootCA set as global will take the last preference.
 *
 * Device certificate & keys will be loaded as below:
 * 1. If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is disabled:
 *    a. If device certificate and keys are loaded by application/library using \ref cy_tls_create_identity API and
 *       TLS identity set using socket option CY_SOCKET_SO_TLS_IDENTITY will be used for device certificate and keys.
 * 2.If CY_SECURE_SOCKETS_PKCS_SUPPORT flag is enabled:
 *    a. By default, device certificates and keys which are provisioned to secure element will be used.
 *    b. If socket option CY_SOCKET_SO_DEVICE_CERT_KEY_LOCATION is set with value CY_SOCKET_DEVICE_CERT_KEY_RAM then
 *       identity set to the TLS context will be used.
 *
 * @param[in]  context  Context handle for the TLS Layer created using \ref cy_tls_create_context.
 * @param[in]  endpoint Endpoint type for the TLS handshake.
 * @param[in]  timeout  Maximum amount of time to wait in milliseconds to complete TLS connection.
 * @return CY_RSLT_SUCCESS on success; an error code on failure.
 *         Important error code related to this API function is: \n
 *         CY_RSLT_MODULE_TLS_ERROR
 */
cy_rslt_t cy_tls_connect( cy_tls_context_t context, cy_tls_endpoint_type_t endpoint, uint32_t timeout );

/**
 * Encrypts the given data and sends it over a secure connection.
 *
 * @param[in]  context     Context handle for TLS Layer created using \ref cy_tls_create_context.
 * @param[in]  data        Byte array of data to be encrypted and then sent to the network.
 * @param[in]  length      Length in bytes of the write buffer.
 * @param[in]  timeout     Maximum amount of time to wait in milliseconds to complete send operation.
 * @param[out] bytes_sent  Number of bytes sent.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure. On success, it also returns the number of bytes sent.
 *         Important error codes related to this API function are: \n
 *         CY_RSLT_MODULE_TLS_BADARG \n
 *         CY_RSLT_MODULE_TLS_ERROR
 */
cy_rslt_t cy_tls_send( cy_tls_context_t context, const unsigned char *data, uint32_t length, uint32_t timeout, uint32_t *bytes_sent );

/**
 * Reads the encrypted data from the network, decrypts the data, and then stores it in the given buffer.
 *
 * @param[in]  context         Context handle for the TLS Layer created using \ref cy_tls_create_context.
 * @param[out] buffer          Byte array to store the decrypted data received from the network.
 * @param[in]  length          Length in bytes of the read buffer.
 * @param[in]  timeout         Maximum amount of time to wait in milliseconds to complete receive operation.
 * @param[out] bytes_received  Number of bytes received.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure. On Success, it also returns the number of bytes received.
 *         Important error codes related to this API function are: \n
 *         CY_RSLT_MODULE_TLS_BADARG \n
 *         CY_RSLT_MODULE_TLS_ERROR
 */
cy_rslt_t cy_tls_recv( cy_tls_context_t context, unsigned char *buffer, uint32_t length, uint32_t timeout, uint32_t *bytes_received );

/**
 * Releases the resources allocated for the TLS connection.
 *
 * @param[in] context Context handle returned by the TLS Layer created using \ref cy_tls_create_context.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure.
 *         Important error code related to this API function is: \n
 *         CY_RSLT_MODULE_TLS_BADARG
 */
cy_rslt_t cy_tls_delete_context( cy_tls_context_t context );

/**
 * Configures a custom certificate profile using the message digest and RSA min key length.
 *
 * @param[in] mds_type      Message digest type.
 * @param[in] rsa_bit_len   Minimum RSA key length in bits.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure.
 *         Important error code related to this API function is: \n
 *         CY_RSLT_MODULE_TLS_BADARG
 */
cy_rslt_t cy_tls_config_cert_profile_param(cy_tls_md_type_t mds_type, cy_tls_rsa_min_key_len_t rsa_bit_len);

/**
 * Checks if given buffer is in valid certificate format.
 *
 * @param[in] certificate_data       x509 certificate in PEM format. It should be a null-terminated string.
 * @param[in] certificate_len        Length of the certificate excluding the 'null' terminator.
 *
 * @return CY_RSLT_SUCCESS on success; an error code on failure.
 */
cy_rslt_t cy_tls_is_certificate_valid_x509(const char *certificate_data,
                                           const uint32_t certificate_len);

/**
 * This function is the entropy source function. It generates true random number
 * using HW TRNG engine. mbedtls random number module calls this function
 * to get the entropy from HW TRGN engine.
 *
 * @param[in]  obj:              cyhal RNG object
 * @param[out] output:           output buffer holding the random number
 * @param[in]  length:           Requested random number length
 * @param[out] output_length:    Actual generated random number length
 * 
 * @return int zero on success, negative value on failure
 */
int cy_tls_mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);

/** \} group_cy_tls_functions */

#ifdef __cplusplus
} /*extern "C" */
#endif
#endif /* ifndef INCLUDED_CY_TLS_INTERFACE_H_ */

/** \} group_cy_tls */
