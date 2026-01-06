/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
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

#ifndef CY_NEWORK_MW_CORE_ERROR_H_
#define CY_NEWORK_MW_CORE_ERROR_H_

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "cy_result.h"
#include "cy_result_mw.h"

/**
 * @defgroup generic_netxduo_whd_port_defines CY generic netxduo WHD glue results/error codes
 * @ingroup group_utils_enums
 * WiFi middleware core APIs return results of type cy_rslt_t and comprise of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result Format
 *
   \verbatim
              Module base                      Type    Library specific error code
      +--------------------------------------+------+------------------------------+
      |CY_RSLT_MODULE_NETXDUO_WHD_PORT_BASE  | 0x2  |           Error Code         |
      +--------------------------------------+------+------------------------------+
                14-bits                       2-bits            16-bits

   Refer to the macro section of this document for library specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h located in <core_lib/include>
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of the CY_RSLT_MODULE_MIDDLEWARE_BASE
 *              The details of the offset and the middleware base are defined in cy_result_mw.h, that is part of [Github connectivity-utilities] (https://github.com/Infineon/connectivity-utilities)
 *              For instance, NetXDuo WHD PORT (CY_NETXDUO_WHD_PORT) uses CY_RSLT_MODULE_NETXDUO_WHD_PORT_BASE as the module base
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING or CY_RSLT_TYPE_INFO. AWS library error codes are of type CY_RSLT_TYPE_ERROR
 *
 * Library specific error code: These error codes are library specific and defined in macro section
 *
 * Helper macros used for creating the library specific result are provided as part of cy_result.h
 * \{
 */
/** Generic CY netxduo WHD port base error code */
#define CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE                  CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_NETXDUO_WHD_PORT_BASE, 0)

/** CY NETXDUO error */
#define CY_RSLT_NETWORK_INTERFACE_EXISTS                   (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 1)  /**< Denotes interface already exists   */
#define CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE             (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 2)  /**< Denotes adding interface failed    */
#define CY_RSLT_NETWORK_ERROR_STARTING_DHCP                (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 3)  /**< Denotes failure to start DHCP      */
#define CY_RSLT_NETWORK_INTERFACE_DOES_NOT_EXIST           (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 4)  /**< Denotes interface does not exist   */
#define CY_RSLT_NETWORK_BAD_ARG                            (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 6)  /**< Denotes BAD arg                    */
#define CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP       (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 14) /**< Denotes failure to start internal DHCP server */
#define CY_RSLT_NETWORK_INTERFACE_NETWORK_NOT_UP           (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 15) /**< Denotes network is not up for the given interface */
#define CY_RSLT_NETWORK_ERROR_REMOVING_INTERFACE           (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 16) /**< Denotes error while removing interface */
#define CY_RSLT_NETWORK_ERROR_STARTING_DNS                 (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 17) /**< Denotes failure to start DNS client */
#define CY_RSLT_NETWORK_ERROR_ADDING_DNS_SERVER            (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 18) /**< Denotes error adding DNS server address to DNS client */
#define CY_RSLT_NETWORK_ERROR_NO_PACKET                    (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 19) /**< Denotes error allocating packet */
#define CY_RSLT_NETWORK_NO_INTERFACE_ADDRESS               (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 20) /**< Denotes error no IP address for the interface */
#define CY_RSLT_NETWORK_NOT_SUPPORTED                      (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 21) /**< Denotes error feature not enabled */
#define CY_RSLT_NETWORK_GW_ADDRESS_NOT_FOUND               (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 22) /**< Denotes error IP address of the gateway not found */
#define CY_RSLT_NETWORK_ARP_REQUEST_FAILURE                (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 23) /**< Denotes error IP ARP request failure */
#define CY_RSLT_NETWORK_WAIT_TIMEOUT                       (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 24) /**< Denotes error timeout */
#define CY_RSLT_NETWORK_PING_FAILURE                       (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 25) /**< Denotes ping failure */
#define CY_RSLT_NETWORK_ERROR_TRNG                         (CY_RSLT_NETXDUO_WHD_PORT_ERR_BASE + 26) /**< Denotes that the random number generation failed*/
/**
 * \}
 */

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* CY_NEWORK_MW_CORE_ERROR_H_ */
