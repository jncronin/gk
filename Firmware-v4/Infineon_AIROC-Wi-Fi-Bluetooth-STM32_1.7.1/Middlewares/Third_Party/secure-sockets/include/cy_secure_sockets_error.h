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
 *  Defines the Secure Sockets Interface Error codes.
 *
 */

#ifndef INCLUDED_CY_SECURE_SOCKETS_ERROR_H_
#define INCLUDED_CY_SECURE_SOCKETS_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cy_result.h"
#include "cy_result_mw.h"


/**
 * \defgroup group_secure_sockets_results Secure Sockets results/error codes
 * @ingroup group_secure_sockets_macros
 *
 * Secure Sockets Library APIs return results of type cy_rslt_t and consist of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result Format
 *
   \verbatim
              Module base                   Type    Library-specific error code
      +-----------------------------------+------+------------------------------+
      |CY_RSLT_MODULE_SECURE_SOCKETS_BASE | 0x2  |           Error Code         |
      +-----------------------------------+------+------------------------------+
                14 bits                    2 bits            16 bits

   See the macro section of this document for library-specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h located in <core_lib/include>
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of the CY_RSLT_MODULE_MIDDLEWARE_BASE.
 *              The details of the offset and the middleware base are defined in cy_result_mw.h, which is part of the [GitHub connectivity-utilities] (https://github.com/cypresssemiconductorco/connectivity-utilities) repo.
 *              For example, the Secure Sockets Library uses CY_RSLT_MODULE_SECURE_SOCKETS_BASE as the module base.
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING, or CY_RSLT_TYPE_INFO. AWS library error codes are of type CY_RSLT_TYPE_ERROR.
 *
 * Library-specific error code: These error codes are library-specific and defined in the macro section.
 *
 * Helper macros used for creating the library-specific result are provided as part of cy_result.h.
 * \{
 */

/** Secure Sockets error code base. */
#define CY_RSLT_SECURE_SOCKETS_ERR_BASE    CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_SECURE_SOCKETS_BASE, 0)

/** Generic TCP/IP error. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR                            ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 1 )
/** Invalid argument. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_BADARG                                 ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 2 )
/** Out of resources. This error is returned if the system goes out of memory,
 * or network stack resources such as network buffers or the number of
 * connections used exceeds the configured value. To debug this error, check network stack
 * configurations based on the API that is returning this error. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM                                  ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 3 )
/** Socket not connected. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED                          ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 4 )
/** Socket closed. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED                                 ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 5 )
/** Socket already connected. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED                      ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 6 )
/** Protocol not supported. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_PROTOCOL_NOT_SUPPORTED                 ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 7 )
/**
 * Socket option not supported error.
 * Secure Socket Layer returns this error code if the feature is not enabled in lwipopts.h.
 */
#define CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED                   ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 8 )
/** Invalid option. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION                         ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 9 )
/** Socket not listening. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_NOT_LISTENING                          ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 10 )
/** Operation timed out. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT                                ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 11 )
/** Operation in progress. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_IN_PROGRESS                            ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 12 )
/** Host not found. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_HOST_NOT_FOUND                         ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 13 )
/** Generic TLS error. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_TLS_ERROR                              ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 14 )
/** Invalid socket. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET                         ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 15 )
/** API not supported. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED                          ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 16 )
/** Library not initialized. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_NOT_INITIALIZED                        ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 17 )
/** Network interface does not exist. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST                   ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 18 )
/** ARP resolution timed out. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_ARP_TIMEOUT                            ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 19 )
/** Both IPv4 and IPv6 network stack configuration are disabled. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION             ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 20 )
/** Socket address already in use. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_ADDRESS_IN_USE                         ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 21 )
/** Maximum number of multicast addresses are already created. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_MAX_MEMBERSHIP_ERROR                   ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 22 )
/** Multicast address not registered. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_MULTICAST_ADDRESS_NOT_REGISTERED       ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 23 )
/** Routing problem. */
#define CY_RSLT_MODULE_SECURE_SOCKETS_ERROR_ROUTING                          ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 24 )
/** PKCS generic error */
#define CY_RSLT_MODULE_SECURE_SOCKETS_PKCS_ERROR                             ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 25 )
/** Send buffer is full while sending / Receive buffer is empty while reading */
#define CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK                             ( CY_RSLT_SECURE_SOCKETS_ERR_BASE + 26 )
/** \} group_secure_sockets_macros */
#ifdef __cplusplus
} /*extern "C" */
#endif
#endif /* ifndef INCLUDED_CY_SECURE_SOCKETS_ERROR_H_ */
