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
* @file cy_eapol.h
* @brief EAPOL handling for receiving EAPOL data from WHD
*/
#pragma once

#include "whd_types.h"
#include "whd.h"
#include "whd_buffer_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
#define CY_EAP_MTU_SIZE                 ( 1020 )
#define CY_ETHERNET_ADDRESS_LENGTH      ( 6 )
/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef enum
{
    CY_EAP_CODE_REQUEST  = 1,
    CY_EAP_CODE_RESPONSE = 2,
    CY_EAP_CODE_SUCCESS  = 3,
    CY_EAP_CODE_FAILURE  = 4
} cy_eap_code_t;

/*
 * EAP Request and Response data begins with one octet Type. Success and
 * Failure do not have additional data.
 */
typedef enum {
    CY_EAP_TYPE_NONE         = 0,
    CY_EAP_TYPE_IDENTITY     = 1   /* RFC 3748 */,
    CY_EAP_TYPE_NOTIFICATION = 2   /* RFC 3748 */,
    CY_EAP_TYPE_NAK          = 3   /* Response only, RFC 3748 */,
    CY_EAP_TYPE_MD5          = 4,  /* RFC 3748 */
    CY_EAP_TYPE_OTP          = 5   /* RFC 3748 */,
    CY_EAP_TYPE_GTC          = 6,  /* RFC 3748 */
    CY_EAP_TYPE_TLS          = 13  /* RFC 2716 */,
    CY_EAP_TYPE_LEAP         = 17  /* Cisco proprietary */,
    CY_EAP_TYPE_SIM          = 18  /* draft-haverinen-pppext-eap-sim-12.txt */,
    CY_EAP_TYPE_TTLS         = 21  /* draft-ietf-pppext-eap-ttls-02.txt */,
    CY_EAP_TYPE_AKA          = 23  /* draft-arkko-pppext-eap-aka-12.txt */,
    CY_EAP_TYPE_PEAP         = 25  /* draft-josefsson-pppext-eap-tls-eap-06.txt */,
    CY_EAP_TYPE_MSCHAPV2     = 26  /* draft-kamath-pppext-eap-mschapv2-00.txt */,
    CY_EAP_TYPE_TLV          = 33  /* draft-josefsson-pppext-eap-tls-eap-07.txt */,
    CY_EAP_TYPE_FAST         = 43  /* draft-cam-winget-eap-fast-00.txt */,
    CY_EAP_TYPE_PAX          = 46, /* draft-clancy-eap-pax-04.txt */
    CY_EAP_TYPE_EXPANDED_NAK = 253 /* RFC 3748 */,
    CY_EAP_TYPE_WPS          = 254 /* Wireless Simple Config */,
    CY_EAP_TYPE_PSK          = 255 /* EXPERIMENTAL - type not yet allocated draft-bersani-eap-psk-09 */
} cy_eap_type_t;

/**
 * EAPOL types
 */

typedef enum
{
    CY_EAP_PACKET                   = 0,
    CY_EAPOL_START                  = 1,
    CY_EAPOL_LOGOFF                 = 2,
    CY_EAPOL_KEY                    = 3,
    CY_EAPOL_ENCAPSULATED_ASF_ALERT = 4
} cy_eapol_packet_type_t;


/******************************************************
 *                    Structures
 ******************************************************/

#pragma pack(1)

typedef struct
{
    uint8_t* data;
    uint16_t length;
    uint32_t packet_mask;
} cy_ie_t;

typedef struct
{
    uint8_t   ether_dhost[CY_ETHERNET_ADDRESS_LENGTH];
    uint8_t   ether_shost[CY_ETHERNET_ADDRESS_LENGTH];
    uint16_t  ether_type;
} cy_ether_header_t;

typedef struct
{
    uint8_t  version;
    uint8_t  type;
    uint16_t length;
} cy_eapol_header_t;

typedef struct
{
    cy_ether_header_t  ethernet;
    cy_eapol_header_t  eapol;
} cy_eapol_packet_header_t;

typedef struct
{
    cy_ether_header_t  ethernet;
    cy_eapol_header_t  eapol;
    uint8_t         data[1];
} cy_eapol_packet_t;

typedef struct
{
    uint8_t  code;
    uint8_t  id;
    uint16_t length;
    uint8_t  type;
} cy_eap_header_t;

typedef struct
{
    cy_ether_header_t  ethernet;
    cy_eapol_header_t  eapol;
    cy_eap_header_t    eap;
    uint8_t         data[1];
} cy_eap_packet_t;

typedef struct
{
    uint8_t  vendor_id[3];
    uint32_t vendor_type;
    uint8_t  op_code;
    uint8_t  flags;
} cy_eap_expanded_header_t;

typedef struct
{
    uint8_t  flags;
} cy_eap_tls_header_t;

typedef struct
{
    cy_ether_header_t        ethernet;
    cy_eapol_header_t        eapol;
    cy_eap_header_t          eap;
    cy_eap_tls_header_t      eap_tls;
    uint8_t               data[1]; // Data starts with a length of TLS data field or TLS data depending on the flags field
} cy_eap_tls_packet_t;

#pragma pack()

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
extern uint8_t*     cy_eapol_get_eapol_data( whd_buffer_t packet, whd_interface_t interface );
extern uint16_t     cy_get_eapol_packet_size( whd_buffer_t packet, whd_interface_t interface );

#ifdef __cplusplus
} /*extern "C" */
#endif
