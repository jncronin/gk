/*
 * Copyright 2019-2024, Cypress Semiconductor Corporation (an Infineon company) or
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
#pragma once

#include <stdint.h>
#include "cy_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @addtogroup group_utils_enums
 *
 * Cypress middleware APIs return results of type cy_rslt_t and comprise of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result Format
 *
   \verbatim
              Module base             Type    Library specific error code
      +------------------------------+------+------------------------------+
      |CY_RSLT_MODULE_MIDDLEWARE_BASE| 0x2  |           Error Code         |
      +------------------------------+------+------------------------------+
                14-bits               2-bits            16-bits

   Refer to the macro section of this document for library specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h located in <core_lib/include>
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of the CY_RSLT_MODULE_MIDDLEWARE_BASE
 *              The details of the offset and the middleware base are defined below
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING or CY_RSLT_TYPE_INFO. AWS library error codes are of type CY_RSLT_TYPE_ERROR
 *
 * Library specific error code: These error codes are library specific and defined in macro section of the respetcive libraries
 *
 * Helper macros used for creating the library specific result are provided as part of cy_result.h
 *
 * Each middleware module has been reserved with 128 error codes
 *
 *
 *
 * @{
 */
 /* !!! ALWAYS ADD MODULE BASES AT THE END. DO NOT INSERT NEW MODULES IN BETWEEN EXISTING MODULES !!!
 * The expectation is that all middleware modules shall define their base out of this file so
 * that the error code space can be reserved and allotted efficiently
 */

/** MDNS module base */
#define CY_RSLT_MODULE_MDNS_BASE                         CY_RSLT_MODULE_MIDDLEWARE_BASE
/** AWS IoT module base */
#define CY_RSLT_MODULE_AWS_BASE                          CY_RSLT_MODULE_MIDDLEWARE_BASE + 1
/** JSON parser module base */
#define CY_RSLT_MODULE_JSON_BASE                         CY_RSLT_MODULE_MIDDLEWARE_BASE + 2
/** Linked list module base */
#define CY_RSLT_MODULE_LINKED_LIST_BASE                  CY_RSLT_MODULE_MIDDLEWARE_BASE + 3
/** command console module base */
#define CY_RSLT_MODULE_COMMAND_CONSOLE_BASE              CY_RSLT_MODULE_MIDDLEWARE_BASE + 4
/** HTTP server module base */
#define CY_RSLT_MODULE_HTTP_SERVER                       CY_RSLT_MODULE_MIDDLEWARE_BASE + 5
/** Enterprise Security base */
#define CY_RSLT_MODULE_ENTERPRISE_SECURITY_BASE          CY_RSLT_MODULE_MIDDLEWARE_BASE + 6
/** TCP/IP module base */
#define CY_RSLT_MODULE_TCPIP_BASE                        CY_RSLT_MODULE_MIDDLEWARE_BASE + 7
/** Generic middleware module base */
#define CY_RSLT_MODULE_MW_BASE                           CY_RSLT_MODULE_MIDDLEWARE_BASE + 8
/** TLS module base */
#define CY_RSLT_MODULE_TLS_BASE                          CY_RSLT_MODULE_MIDDLEWARE_BASE + 9
/** Secure Sockets module base */
#define CY_RSLT_MODULE_SECURE_SOCKETS_BASE               CY_RSLT_MODULE_MIDDLEWARE_BASE + 10
/** WiFi Connection Manager (WCM) module base */
#define CY_RSLT_MODULE_WCM_BASE                          CY_RSLT_MODULE_MIDDLEWARE_BASE + 11
/** lwIP WHD port module base */
#define CY_RSLT_MODULE_LWIP_WHD_PORT_BASE                CY_RSLT_MODULE_MIDDLEWARE_BASE + 12
/** Over The Air Update Module base (OTA) */
#define CY_RSLT_MODULE_OTA_UPDATE_BASE                   CY_RSLT_MODULE_MIDDLEWARE_BASE + 13
/** HTTP Client module base */
#define CY_RSLT_MODULE_HTTP_CLIENT                       CY_RSLT_MODULE_MIDDLEWARE_BASE + 14
/** Buffer pool module base */
#define CY_RSLT_MODULE_BUFFER_POOL_BASE                  CY_RSLT_MODULE_MIDDLEWARE_BASE + 15
/** lwIP network interface integration module base */
#define CY_RSLT_MODULE_NETWORK_PORT_BASE                 CY_RSLT_MODULE_MIDDLEWARE_BASE + 16
/** Ethernet Connection Manager (ECM) module base */
#define CY_RSLT_MODULE_ECM_BASE                          CY_RSLT_MODULE_MIDDLEWARE_BASE + 17
/** NetXDuo WHD port module base */
#define CY_RSLT_MODULE_NETXDUO_WHD_PORT_BASE             CY_RSLT_MODULE_MIDDLEWARE_BASE + 18
/** Virtual connectivity Manager module base */
#define CY_RSLT_MODULE_VCM_BASE                          CY_RSLT_MODULE_MIDDLEWARE_BASE + 19

/**
 * @}
 */

/**
 * @defgroup generic_mw_defines generic middleware results/error codes
 * @ingroup group_utils_enums
 * @{
 * List of all the generic result/error codes defined by the connectivity-utilities library.
 */
/** Generic middleware error code start */
#define CY_RSLT_MODULE_MW_ERR_CODE_START       (0)
/** Generic middleware base error code */
#define CY_RSLT_MW_ERR_BASE                   CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MW_BASE, CY_RSLT_MODULE_MW_ERR_CODE_START)
/** Generic middleware error */
#define CY_RSLT_MW_ERROR                      ( CY_RSLT_MW_ERR_BASE + 1 )
/** Generic middleware timeout */
#define CY_RSLT_MW_TIMEOUT                    ( CY_RSLT_MW_ERR_BASE + 2 )
/** Generic middleware bad argument */        
#define CY_RSLT_MW_BADARG                     ( CY_RSLT_MW_ERR_BASE + 3 )
/** Generic middleware out of heap memory */        
#define CY_RSLT_MW_OUT_OF_HEAP_SPACE          ( CY_RSLT_MW_ERR_BASE + 4 )
/** Generic middleware pending operation */        
#define CY_RSLT_MW_PENDNG                     ( CY_RSLT_MW_ERR_BASE + 5 )
/** Generic middleware unsupported method */        
#define CY_RSLT_MW_UNSUPPORTED                ( CY_RSLT_MW_ERR_BASE + 6 )
/** Generic middleware buffer unavailable */        
#define CY_RSLT_MW_BUFFER_UNAVAIL_TEMPORARILY ( CY_RSLT_MW_ERR_BASE + 7 )

/**
 * @}
 */

/** 
 * @defgroup tcpip_mw_defines TCP/IP socket results/error codes
 * @ingroup group_utils_enums
 * @{
 * List of all the socket result/error codes defined by the connectivity-utilities library.
 */
/** TCP/IP error code start */
#define CY_RSLT_MODULE_TCPIP_ERR_CODE_START       (0)
/** TCPIP/IP base error code */
#define CY_RSLT_TCPIP_ERR_BASE                CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_TCPIP_BASE, CY_RSLT_MODULE_TCPIP_ERR_CODE_START)
/** TCP/IP generic error */
#define CY_RSLT_TCPIP_ERROR                   ( CY_RSLT_TCPIP_ERR_BASE + 1 )
/** TCP/IP timeout */
#define CY_RSLT_TCPIP_TIMEOUT                 ( CY_RSLT_TCPIP_ERR_BASE + 2 )
/** TCP/IP out of memory */
#define CY_RSLT_TCPIP_ERROR_NO_MEMORY         ( CY_RSLT_TCPIP_ERR_BASE + 3 )
/** TCP/IP error opening socket */
#define CY_RSLT_TCPIP_ERROR_SOCKET_OPEN       ( CY_RSLT_TCPIP_ERR_BASE + 4 )
/** TCP/IP error binding socket */
#define CY_RSLT_TCPIP_ERROR_SOCKET_BIND       ( CY_RSLT_TCPIP_ERR_BASE + 5 )
/** TCP/IP error listening to socket */
#define CY_RSLT_TCPIP_ERROR_SOCKET_LISTEN     ( CY_RSLT_TCPIP_ERR_BASE + 6 )
/** TCP/IP error accepting socket */
#define CY_RSLT_TCPIP_ERROR_SOCKET_ACCEPT     ( CY_RSLT_TCPIP_ERR_BASE + 7 )
/** TCP/IP error with TLS operation */
#define CY_RSLT_TCPIP_ERROR_TLS_OPERATION     ( CY_RSLT_TCPIP_ERR_BASE + 8 )
/** TCP/IP max sockets bound */
#define CY_RSLT_TCPIP_ERROR_NO_MORE_SOCKET    ( CY_RSLT_TCPIP_ERR_BASE + 9 )
/** TCP/IP error sending data */
#define CY_RSLT_TCPIP_ERROR_SEND              ( CY_RSLT_TCPIP_ERR_BASE + 10)
/** TCP/IP error receiving data */
#define CY_RSLT_TCPIP_ERROR_RECEIVE           ( CY_RSLT_TCPIP_ERR_BASE + 11)
/** TCP/IP error in setting socket options */
#define CY_RSLT_TCPIP_ERROR_SOCKET_OPTIONS    ( CY_RSLT_TCPIP_ERR_BASE + 12 )
/** TCP/IP error bad argument */
#define CY_RSLT_TCPIP_ERROR_BAD_ARG           ( CY_RSLT_TCPIP_ERR_BASE + 13 )
/** TCP/IP error socket closed */
#define CY_RSLT_TCPIP_ERROR_SOCKET_CLOSED     ( CY_RSLT_TCPIP_ERR_BASE + 14 )

/**
 * @}
 */

/** 
 * @defgroup tls_mw_defines TLS results/error codes
 * @ingroup group_utils_enums
 * @{
 * List of all the TLS result/error codes defined by the connectivity-utilities library.
 */
/** TLS error code start */
#define CY_RSLT_MODULE_TLS_ERR_CODE_START        (0)
/** TLS base error code */
#define CY_RSLT_TLS_ERR_BASE                     CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_TLS_BASE, CY_RSLT_MODULE_TLS_ERR_CODE_START)
/** TLS generic error */
#define CY_RSLT_MODULE_TLS_ERROR                 ( CY_RSLT_TLS_ERR_BASE + 1 )
/** TLS timeout error */
#define CY_RSLT_MODULE_TLS_TIMEOUT               ( CY_RSLT_TLS_ERR_BASE + 2 )
/** TLS bad argument */
#define CY_RSLT_MODULE_TLS_BADARG                ( CY_RSLT_TLS_ERR_BASE + 3 )
/** TLS out of memory */
#define CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE     ( CY_RSLT_TLS_ERR_BASE + 4 )
/** TLS bad input */
#define CY_RSLT_MODULE_TLS_BAD_INPUT_DATA        ( CY_RSLT_TLS_ERR_BASE + 5 )
/** TLS error parsing private key */
#define CY_RSLT_MODULE_TLS_PARSE_KEY             ( CY_RSLT_TLS_ERR_BASE + 6 )
/** TLS error parsing certificate */
#define CY_RSLT_MODULE_TLS_PARSE_CERTIFICATE     ( CY_RSLT_TLS_ERR_BASE + 7 )
/** TLS unsupported method */
#define CY_RSLT_MODULE_TLS_UNSUPPORTED           ( CY_RSLT_TLS_ERR_BASE + 8 )
/** TLS handshake failure */
#define CY_RSLT_MODULE_TLS_HANDSHAKE_FAILURE     ( CY_RSLT_TLS_ERR_BASE + 9 )
/** TLS socket connection closed by peer */
#define CY_RSLT_MODULE_TLS_CONNECTION_CLOSED     ( CY_RSLT_TLS_ERR_BASE + 10 )
/** TLS socket not connected */
#define CY_RSLT_MODULE_TLS_SOCKET_NOT_CONNECTED  ( CY_RSLT_TLS_ERR_BASE + 11 )
/** PKCS11 generic error */
#define CY_RSLT_MODULE_TLS_PKCS_ERROR            ( CY_RSLT_TLS_ERR_BASE + 12 )

/**
 * @}
 */

#ifdef __cplusplus
} /*extern "C" */
#endif        

