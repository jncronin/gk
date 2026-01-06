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

/**
* @file cy_wps_result.h
* @brief Cypress WPS results
*/
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/
#define CY_RSLT_MODULE_WPS_BASE                         CY_RSLT_MODULE_MIDDLEWARE_BASE + 400

/** Generic WPS error code start */
#define CY_RSLT_MODULE_WPS_ERR_CODE_START              (0)
/** Generic WPS base error code */
#define CY_RSLT_WPS_ERR_BASE                           CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_WPS_BASE, CY_RSLT_MODULE_WPS_ERR_CODE_START)

#define CY_RSLT_WPS_TIMEOUT                                        ( CY_RSLT_WPS_ERR_BASE + 1)   /** WPS timeout */
#define CY_RSLT_WPS_ERROR                                          ( CY_RSLT_WPS_ERR_BASE + 2)   /** WPS error */
#define CY_RSLT_WPS_BADARG                                         ( CY_RSLT_WPS_ERR_BASE + 3)   /** WPS bad argument */
#define CY_RSLT_WPS_UNPROCESSED                                    ( CY_RSLT_WPS_ERR_BASE + 4)   /** WPS unprocessed */
#define CY_RSLT_WPS_IN_PROGRESS                                    ( CY_RSLT_WPS_ERR_BASE + 5)   /** WPS In Progress */
#define CY_RSLT_WPS_COMPLETE                                       ( CY_RSLT_WPS_ERR_BASE + 6)   /** WPS Complete */
#define CY_RSLT_WPS_PBC_OVERLAP                                    ( CY_RSLT_WPS_ERR_BASE + 7)   /** WPS PBC overlap */
#define CY_RSLT_WPS_ABORTED                                        ( CY_RSLT_WPS_ERR_BASE + 8)   /** WPS aborted */
#define CY_RSLT_WPS_NOT_STARTED                                    ( CY_RSLT_WPS_ERR_BASE + 9)   /** WPS not started */
#define CY_RSLT_WPS_REGISTRAR_NOT_READY                            ( CY_RSLT_WPS_ERR_BASE + 10)  /** WPS registrar not ready */
#define CY_RSLT_WPS_ATTEMPTED_EXTERNAL_REGISTRAR_DISCOVERY         ( CY_RSLT_WPS_ERR_BASE + 11)  /** Attempted external registrar discovery */
#define CY_RSLT_WPS_ERROR_JOIN_FAILED                              ( CY_RSLT_WPS_ERR_BASE + 12)  /** Join failed */
#define CY_RSLT_WPS_ERROR_ENCRYPTED_TLV_HMAC_FAIL                  ( CY_RSLT_WPS_ERR_BASE + 13)  /** TLV HMAC failed */
#define CY_RSLT_WPS_ERROR_SECRET_NONCE_MISMATCH                    ( CY_RSLT_WPS_ERR_BASE + 14)  /** Secret nonce not matching */
#define CY_RSLT_WPS_ERROR_MESSAGE_HMAC_FAIL                        ( CY_RSLT_WPS_ERR_BASE + 15)  /** HMAC failed */
#define CY_RSLT_WPS_ERROR_MESSAGE_TLV_MASK_MISMATCH                ( CY_RSLT_WPS_ERR_BASE + 16)  /** TLV mask mismatch */
#define CY_RSLT_WPS_ERROR_ENCRYPTION_TYPE_ERROR                    ( CY_RSLT_WPS_ERR_BASE + 17)  /** Encryption error  */
#define CY_RSLT_WPS_ERROR_AUTHENTICATION_TYPE_ERROR                ( CY_RSLT_WPS_ERR_BASE + 18)  /** Authentication type error */
#define CY_RSLT_WPS_ERROR_REGISTRAR_NONCE_MISMATCH                 ( CY_RSLT_WPS_ERR_BASE + 19)  /** Registrar nonce mismatch */
#define CY_RSLT_WPS_ERROR_ENROLLEE_NONCE_MISMATCH                  ( CY_RSLT_WPS_ERR_BASE + 20)  /** Enrollee nonce mismatch */
#define CY_RSLT_WPS_ERROR_VERSION_MISMATCH                         ( CY_RSLT_WPS_ERR_BASE + 21)  /** Version mismatch */
#define CY_RSLT_WPS_ERROR_CRYPTO                                   ( CY_RSLT_WPS_ERR_BASE + 22)  /** Crypto error */
#define CY_RSLT_WPS_ERROR_MESSAGE_MISSING_TLV                      ( CY_RSLT_WPS_ERR_BASE + 23)  /** Missing TLV */
#define CY_RSLT_WPS_ERROR_INCORRECT_MESSAGE                        ( CY_RSLT_WPS_ERR_BASE + 24)  /** Incorrect message */
#define CY_RSLT_WPS_ERROR_NO_RESPONSE                              ( CY_RSLT_WPS_ERR_BASE + 25)  /** No response */
#define CY_RSLT_WPS_ERROR_RECEIVED_EAP_FAIL                        ( CY_RSLT_WPS_ERR_BASE + 26)  /** EAP received failed */
#define CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS                 ( CY_RSLT_WPS_ERR_BASE + 27)  /** Error in WEP credentials received */
#define CY_RSLT_WPS_ERROR_OUT_OF_MEMORY                            ( CY_RSLT_WPS_ERR_BASE + 28)  /** Out of memory */
#define CY_RSLT_WPS_ERROR_QUEUE_PUSH                               ( CY_RSLT_WPS_ERR_BASE + 29)  /** Queue push failed */
#define CY_RSLT_WPS_ERROR_SCAN_START_FAIL                          ( CY_RSLT_WPS_ERR_BASE + 30)  /** Scan start failed */
#define CY_RSLT_WPS_ERROR_WPS_STACK_MALLOC_FAIL                    ( CY_RSLT_WPS_ERR_BASE + 31)  /** WPS stack malloc failed */
#define CY_RSLT_WPS_ERROR_CREATING_EAPOL_PACKET                    ( CY_RSLT_WPS_ERR_BASE + 32)  /** Error creating EAPOL packet */
#define CY_RSLT_WPS_UNKNOWN_EVENT                                  ( CY_RSLT_WPS_ERR_BASE + 33)  /** Unknown event */
#define CY_RSLT_WPS_OUT_OF_HEAP_SPACE                              ( CY_RSLT_WPS_ERR_BASE + 34)  /** Out of heap */
#define CY_RSLT_WPS_CERT_PARSE_FAIL                                ( CY_RSLT_WPS_ERR_BASE + 35)  /** certificate parsing failed */
#define CY_RSLT_WPS_KEY_PARSE_FAIL                                 ( CY_RSLT_WPS_ERR_BASE + 36)  /** Key parsing failed */
#define CY_RSLT_WPS_ERROR_DEVICE_LIST_FIND                         ( CY_RSLT_WPS_ERR_BASE + 37)  /** Error in finding device list */
#define CY_RSLT_WPS_ERROR_NO_P2P_TLV                               ( CY_RSLT_WPS_ERR_BASE + 38)  /** No P2P TLV */
#define CY_RSLT_WPS_ERROR_ALREADY_STARTED                          ( CY_RSLT_WPS_ERR_BASE + 39)  /** WPS already started */
#define CY_RSLT_WPS_ERROR_HANDLER_ALREADY_REGISTERED               ( CY_RSLT_WPS_ERR_BASE + 40)  /** Handler already registered */
#define CY_RSLT_WPS_BUFFER_ALLOC_FAIL                              ( CY_RSLT_WPS_ERR_BASE + 41)  /** Buffer alloc failed */
#define CY_RSLT_WPS_OTHER_ENROLLEE                                 ( CY_RSLT_WPS_ERR_BASE + 42)  /** Other enrollee */
#define CY_RSLT_WPS_ERROR_RECEIVED_INVALID_CREDENTIALS             ( CY_RSLT_WPS_ERR_BASE + 43)  /** Received invalid credentials */
#define CY_RSLT_WPS_ERROR_HMAC_CHECK_FAIL                          ( CY_RSLT_WPS_ERR_BASE + 44)  /** HMAC check failed */
#define CY_RSLT_WPS_ERROR_UNABLE_TO_SET_WLAN_SECURITY              ( CY_RSLT_WPS_ERR_BASE + 45)  /** Unable to set WLAN security */
#define CY_RSLT_WPS_ERROR_RUNT_WPS_PACKET                          ( CY_RSLT_WPS_ERR_BASE + 46)  /** Runt packet */

#define AVP_LENGTH_MASK 0x00FFFFFF
/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                 Packed Structures
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif
