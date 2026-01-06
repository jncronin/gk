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
 *  Weak functions for TLS
 *
 *  This file provides weak functions when MBEDTLS stack is not used.
 *
 */

#include "cy_tls.h"

#if defined(__ICCARM__)
#define TLS_WEAK_FUNCTION        __WEAK
#define TLS_ATTR_PACKED(struct)  __packed struct
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define TLS_WEAK_FUNCTION        __attribute__((weak))
#define TLS_ATTR_PACKED(struct)  struct __attribute__((packed))
#else
#define TLS_WEAK_FUNCTION        __attribute__((weak))
#define TLS_ATTR_PACKED(struct)  struct __attribute__((packed))
#endif  /* defined(__ICCARM__) */

#define UNUSED_ARG(arg)                     (void)(arg)

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_release_global_root_ca_certificates(void)
{
    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_load_global_root_ca_certificates(const char *trusted_ca_certificates, const uint32_t cert_length)
{
    UNUSED_ARG(trusted_ca_certificates);
    UNUSED_ARG(cert_length);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_init(void)
{
    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_create_identity(const char *certificate_data, const uint32_t certificate_len, const char *private_key, uint32_t private_key_len, void **tls_identity)
{
    UNUSED_ARG(certificate_data);
    UNUSED_ARG(certificate_len);
    UNUSED_ARG(private_key);
    UNUSED_ARG(private_key_len);
    UNUSED_ARG(tls_identity);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_delete_identity(void *tls_identity )
{
    UNUSED_ARG(tls_identity);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_create_context(void **context, cy_tls_params_t *params)
{
    UNUSED_ARG(context);
    UNUSED_ARG(params);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_connect(void *context, cy_tls_endpoint_type_t endpoint, uint32_t timeout)
{
    UNUSED_ARG(context);
    UNUSED_ARG(endpoint);
    UNUSED_ARG(timeout);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_send(void *context, const unsigned char *data, uint32_t length, uint32_t timeout, uint32_t *bytes_sent)
{
    UNUSED_ARG(context);
    UNUSED_ARG(data);
    UNUSED_ARG(length);
    UNUSED_ARG(timeout);
    UNUSED_ARG(bytes_sent);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_recv(void *context, unsigned char *buffer, uint32_t length, uint32_t timeout, uint32_t *bytes_received)
{
    UNUSED_ARG(context);
    UNUSED_ARG(buffer);
    UNUSED_ARG(length);
    UNUSED_ARG(timeout);
    UNUSED_ARG(bytes_received);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_delete_context(cy_tls_context_t context)
{
    UNUSED_ARG(context);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_config_cert_profile_param(cy_tls_md_type_t mds_type, cy_tls_rsa_min_key_len_t rsa_bit_len)
{
    UNUSED_ARG(mds_type);
    UNUSED_ARG(rsa_bit_len);

    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_deinit(void)
{
    return CY_RSLT_SUCCESS;
}

TLS_WEAK_FUNCTION cy_rslt_t cy_tls_is_certificate_valid_x509(const char *certificate_data,
                                                             const uint32_t certificate_len)
{
    UNUSED_ARG(certificate_data);
    UNUSED_ARG(certificate_len);

    return CY_RSLT_MODULE_TLS_PARSE_CERTIFICATE;
}
