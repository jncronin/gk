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
* @file cy_wps_common.h
* @brief Cypress WPS common file
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef COMPONENT_CM0P
#define CY_WPS_HOST_IS_ALIGNED
#endif /* COMPONENT_CM0P */

#include "cy_wps_structures.h"
#include "cyabs_rtos.h"
#include "cy_wps_memory.h"
#include "whd_types.h"
#include "cy_eapol.h"
#include "cy_log.h"
#include "wps_helper_utility.h"
//#include "cy_wcm_debug.h"
/******************************************************
 *                      Macros
 ******************************************************/

#define WPS_ASSERT(x)



#ifdef CY_WPS_HOST_IS_ALIGNED

#define CY_WPS_HOST_READ_16(ptr)              ((uint16_t)(((uint8_t*)ptr)[0] + (((uint8_t*)ptr)[1] << 8)))
#define CY_WPS_HOST_READ_16_BE(ptr)           ((uint16_t)(((uint8_t*)ptr)[1] + (((uint8_t*)ptr)[0] << 8)))
#define CY_WPS_HOST_READ_32(ptr)              ((uint32_t)(((uint8_t*)ptr)[0] + ((((uint8_t*)ptr)[1] << 8)) + (((uint8_t*)ptr)[2] << 16) + (((uint8_t*)ptr)[3] << 24)))
#define CY_WPS_HOST_READ_32_BE(ptr)           ((uint32_t)(((uint8_t*)ptr)[3] + ((((uint8_t*)ptr)[2] << 8)) + (((uint8_t*)ptr)[1] << 16) + (((uint8_t*)ptr)[0] << 24)))
#define CY_WPS_HOST_WRITE_16(ptr, value)      do { ((uint8_t*)ptr)[0] = (uint8_t)value; ((uint8_t*)ptr)[1]=(uint8_t)(value>>8); } while(0)
#define CY_WPS_HOST_WRITE_16_BE(ptr, value)   do { ((uint8_t*)ptr)[1] = (uint8_t)value; ((uint8_t*)ptr)[0]=(uint8_t)(value>>8); } while(0)
#define CY_WPS_HOST_WRITE_32(ptr, value)      do { ((uint8_t*)ptr)[0] = (uint8_t)value; ((uint8_t*)ptr)[1]=(uint8_t)(value>>8); ((uint8_t*)ptr)[2]=(uint8_t)(value>>16); ((uint8_t*)ptr)[3]=(uint8_t)(value>>24); } while(0)
#define CY_WPS_HOST_WRITE_32_BE(ptr, value)   do { ((uint8_t*)ptr)[3] = (uint8_t)value; ((uint8_t*)ptr)[2]=(uint8_t)(value>>8); ((uint8_t*)ptr)[1]=(uint8_t)(value>>16); ((uint8_t*)ptr)[0]=(uint8_t)(value>>24); } while(0)

#else

#define CY_WPS_HOST_READ_16(ptr)            ((uint16_t*)ptr)[0]
#define CY_WPS_HOST_READ_32(ptr)            ((uint32_t*)ptr)[0]
#define CY_WPS_HOST_WRITE_16(ptr, value)    ((uint16_t*)ptr)[0] = value
#define CY_WPS_HOST_WRITE_16_BE(ptr, value) ((uint16_t*)ptr)[0] = htobe16(value)
#define CY_WPS_HOST_WRITE_32(ptr, value)    ((uint32_t*)ptr)[0] = value
#define CY_WPS_HOST_WRITE_32_BE(ptr, value) ((uint32_t*)ptr)[0] = htobe32(value)

/* Prevents errors about strict aliasing */
static inline ALWAYS_INLINE uint16_t CY_WPS_HOST_READ_16_BE(uint8_t* ptr_in)
{
    uint16_t* ptr = (uint16_t*)ptr_in;
    uint16_t  v   = *ptr;
    return (uint16_t)(((v&0x00FF) << 8) | ((v&0xFF00)>>8));
}

static inline ALWAYS_INLINE uint32_t CY_WPS_HOST_READ_32_BE(uint8_t* ptr_in)
{
    uint32_t* ptr = (uint32_t*)ptr_in;
    uint32_t  v   = *ptr;
    return (uint32_t)(((v&0x000000FF) << 24) | ((v&0x0000FF00) << 8) | ((v&0x00FF0000) >> 8) | ((v&0xFF000000) >> 24));
}

#endif

/******************************************************
 *                    Constants
 ******************************************************/

/* Universally Unique IDentifier for device */
#define WPS_TEMPLATE_UUID        "\x77\x5b\x66\x80\xbf\xde\x11\xd3\x8d\x2f"

/* Maximum number of APs to be added after scan into the list */
#define AP_LIST_SIZE             10

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/
/* WPS Message types */
typedef enum
{
    CY_WPS_MESSAGE_TYPE_START    = 0x01,
    CY_WPS_MESSAGE_TYPE_ACK      = 0x02,
    CY_WPS_MESSAGE_TYPE_NACK     = 0x03,
    CY_WPS_MESSAGE_TYPE_MSG      = 0x04,
    CY_WPS_MESSAGE_TYPE_DONE     = 0x05,
    CY_WPS_MESSAGE_TYPE_FRAG_ACK = 0x06,
} cy_wps_message_type_t;

/* Device Type categories for primary and secondary device types */
typedef enum
{
    CY_WPS_DEVICE_TYPE_CAT_COMPUTER      = 1,
    CY_WPS_DEVICE_TYPE_CAT_INPUT_DEVICE  = 2,
    CY_WPS_DEVICE_TYPE_CAT_PRINTER       = 3,
    CY_WPS_DEVICE_TYPE_CAT_CAMERA        = 4,
    CY_WPS_DEVICE_TYPE_CAT_STORAGE       = 5,
    CY_WPS_DEVICE_TYPE_CAT_NW_INFRA      = 6,
    CY_WPS_DEVICE_TYPE_CAT_DISPLAYS      = 7,
    CY_WPS_DEVICE_TYPE_CAT_MM_DEVICES    = 8,
    CY_WPS_DEVICE_TYPE_CAT_GAME_DEVICES  = 9,
    CY_WPS_DEVICE_TYPE_CAT_TELEPHONE     = 10,
    CY_WPS_DEVICE_TYPE_CAT_AUDIO_DEVICES = 11, /* WSC 2.0 */
} cy_wps_device_category_t;

typedef enum
{
    CY_WPS_PRIMARY_DEVICE_COMPUTER               = 1,
    CY_WPS_PRIMARY_DEVICE_INPUT                  = 2,
    CY_WPS_PRIMARY_DEVICE_PRINT_SCAN_FAX_COPY    = 3,
    CY_WPS_PRIMARY_DEVICE_CAMERA                 = 4,
    CY_WPS_PRIMARY_DEVICE_STORAGE                = 5,
    CY_WPS_PRIMARY_DEVICE_NETWORK_INFRASTRUCTURE = 6,
    CY_WPS_PRIMARY_DEVICE_DISPLAY                = 7,
    CY_WPS_PRIMARY_DEVICE_MULTIMEDIA             = 8,
    CY_WPS_PRIMARY_DEVICE_GAMING                 = 9,
    CY_WPS_PRIMARY_DEVICE_TELEPHONE              = 10,
    CY_WPS_PRIMARY_DEVICE_AUDIO                  = 11,
    CY_WPS_PRIMARY_DEVICE_OTHER                  = 0xFF,
} cy_wps_primary_device_type_category_t;

typedef enum
{
    CY_WPS_USBA                  = 0x0001,
    CY_WPS_ETHERNET              = 0x0002,
    CY_WPS_LABEL                 = 0x0004,
    CY_WPS_DISPLAY               = 0x0008,
    CY_WPS_EXTERNAL_NFC_TOKEN    = 0x0010,
    CY_WPS_INTEGRATED_NFC_TOKEN  = 0x0020,
    CY_WPS_NFC_INTERFACE         = 0x0040,
    CY_WPS_PUSH_BUTTON           = 0x0080,
    CY_WPS_KEYPAD                = 0x0100,
    CY_WPS_VIRTUAL_PUSH_BUTTON   = 0x0280,
    CY_WPS_PHYSICAL_PUSH_BUTTON  = 0x0480,
    CY_WPS_VIRTUAL_DISPLAY_PIN   = 0x2008,
    CY_WPS_PHYSICAL_DISPLAY_PIN  = 0x4008
} cy_wps_configuration_method_t;

typedef enum
{
    CY_WPS_ENROLLEE_INFO_ONLY      = 0x00,
    CY_WPS_ENROLLEE_OPEN_8021X     = 0x01,
    CY_WPS_REGISTRAR               = 0x02,
    CY_WPS_WLAN_MANAGER_REGISTRAR  = 0x03
} cy_wps_request_type_t;

/* Device password ID */
typedef enum
{
    CY_WPS_DEFAULT_DEVICEPWDID       = 0x0000,
    CY_WPS_USER_SPEC_DEVICEPWDID     = 0x0001,
    CY_WPS_MACHINE_SPEC_DEVICEPWDID  = 0x0002,
    CY_WPS_REKEY_DEVICEPWDID         = 0x0003,
    CY_WPS_PUSH_BTN_DEVICEPWDID      = 0x0004,
    CY_WPS_DEVICEPWDID_REG_SPEC      = 0x0005,
} cy_wps_device_password_id_t;

/* WPS encryption types */
typedef enum
{
    CY_WPS_MIXED_ENCRYPTION = 0x000c,
    CY_WPS_AES_ENCRYPTION   = 0x0008,
    CY_WPS_TKIP_ENCRYPTION  = 0x0004, /* Deprecated in WSC 2.0 */
    CY_WPS_WEP_ENCRYPTION   = 0x0002, /* Deprecated in WSC 2.0 */
    CY_WPS_NO_ENCRYPTION    = 0x0001,
    CY_WPS_NO_UNDEFINED     = 0x0000,
} cy_wps_encryption_type_t;


/* WPS authentication types */
typedef enum
{
    CY_WPS_OPEN_AUTHENTICATION               = 0x0001,
    CY_WPS_WPA_PSK_AUTHENTICATION            = 0x0002, /* Deprecated in version 2.0 */
    CY_WPS_SHARED_AUTHENTICATION             = 0x0004, /* Deprecated in version 2.0 */
    CY_WPS_WPA_ENTERPRISE_AUTHENTICATION     = 0x0008, /* Deprecated in version 2.0 */
    CY_WPS_WPA2_ENTERPRISE_AUTHENTICATION    = 0x0010,
    CY_WPS_WPA2_PSK_AUTHENTICATION           = 0x0020,
    CY_WPS_WPA2_WPA_PSK_MIXED_AUTHENTICATION = 0x0022,
} cy_wps_authentication_type_t;

/* WPS authentication types */
typedef enum
{
    CY_WPS_TLV_VERSION             = (1 << 0),
    CY_WPS_TLV_ENROLLEE_NONCE      = (1 << 1),
    CY_WPS_TLV_E_HASH1             = (1 << 2),
    CY_WPS_TLV_E_HASH2             = (1 << 3),
    CY_WPS_TLV_ENCRYPTION_SETTINGS = (1 << 4),
    CY_WPS_TLV_AUTHENTICATOR       = (1 << 5),
    CY_WPS_TLV_REGISTRAR_NONCE     = (1 << 6),
    CY_WPS_TLV_AUTH_TYPE_FLAGS     = (1 << 7),
    CY_WPS_TLV_ENCR_TYPE_FLAGS     = (1 << 8),
    CY_WPS_TLV_UUID_R              = (1 << 9),
    CY_WPS_TLV_PUBLIC_KEY          = (1 << 10),
    CY_WPS_TLV_MSG_TYPE            = (1 << 11),
    CY_WPS_TLV_X509_CERT           = (1 << 12),
    CY_WPS_TLV_VENDOR_EXT          = (1 << 13),
    CY_WPS_TLV_R_HASH1             = (1 << 14),
    CY_WPS_TLV_R_HASH2             = (1 << 15),
    CY_WPS_TLV_SSID                = (1 << 16),
    CY_WPS_TLV_AUTH_TYPE           = (1 << 17),
    CY_WPS_TLV_ENCR_TYPE           = (1 << 18),
    CY_WPS_TLV_NW_KEY              = (1 << 19),
    CY_WPS_TLV_MAC_ADDR            = (1 << 20),
    CY_WPS_TLV_E_SNONCE1           = (1 << 21),
    CY_WPS_TLV_E_SNONCE2           = (1 << 22),
    CY_WPS_TLV_R_SNONCE1           = (1 << 23),
    CY_WPS_TLV_R_SNONCE2           = (1 << 24),
    CY_WPS_TLV_CREDENTIAL          = (1 << 25),
} cy_wps_tlv_mask_value_t;

typedef enum
{
    CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE       = (1 <<  1), // 0x002
    CY_WPS_CRYPTO_MATERIAL_ENROLLEE_HASH1       = (1 <<  2), // 0x004
    CY_WPS_CRYPTO_MATERIAL_ENROLLEE_HASH2       = (1 <<  3), // 0x008
    CY_WPS_CRYPTO_MATERIAL_ENROLLEE_PUBLIC_KEY  = (1 <<  4), // 0x010
    CY_WPS_CRYPTO_MATERIAL_ENROLLEE_MAC_ADDRESS = (1 <<  5), // 0x020
    CY_WPS_CRYPTO_MATERIAL_REGISTRAR_NONCE      = (1 <<  6), // 0x040
    CY_WPS_CRYPTO_MATERIAL_REGISTRAR_HASH1      = (1 <<  7), // 0x080
    CY_WPS_CRYPTO_MATERIAL_REGISTRAR_HASH2      = (1 <<  8), // 0x100
    CY_WPS_CRYPTO_MATERIAL_REGISTRAR_PUBLIC_KEY = (1 <<  9), // 0x200
    CY_WPS_CRYPTO_MATERIAL_AUTH_KEY             = (1 << 10), // 0x400
    CY_WPS_CRYPTO_MATERIAL_KEY_WRAP_KEY         = (1 << 11), // 0x800
} cy_wps_crypto_material_value_t;

typedef enum
{
    CY_WPS_EAP_START         = 0,    /* (EAP start ) */
    CY_WPS_EAP_IDENTITY      = 1,    /* (EAP identity request, EAP identity response) */
    CY_WPS_WSC_START         = 2,    /* (WSC start) */
    CY_WPS_ENROLLEE_DISCOVER = 3,    /* Special state machine just for Enrollees*/
} cy_wps_eap_state_machine_stage_t;

typedef enum
{
    CY_WPS_SENDING_PUBLIC_KEYS,
    CY_WPS_SENDING_SECRET_HASHES,
    CY_WPS_SENDING_SECRET_NONCE1,
    CY_WPS_SENDING_SECRET_NONCE2,
    CY_WPS_SENDING_CREDENTIALS = CY_WPS_SENDING_SECRET_NONCE2,
} cy_wps_state_machine_stage_t;

typedef uint8_t* (*cy_wps_packet_generator_t)(cy_wps_agent_t* workspace, uint8_t* iter);

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    uint8_t                valid_message_type;
    uint8_t                outgoing_message_type;
    uint32_t               tlv_mask;
    uint32_t               encrypted_tlv_mask;
    cy_wps_packet_generator_t packet_generator;
} cy_wps_state_machine_state_t;

typedef struct
{
    cy_thread_t           thread;
    void*                 thread_stack;
    cy_queue_t            event_queue;
    cy_event_message_t    event_buffer[10];
    cy_time_t             timer_reference;
    uint32_t              timer_timeout;
    whd_interface_t       interface;
} cy_host_workspace_t;

typedef struct
{
    uint8_t  ssid_length;
    uint8_t  ssid[32];
    uint16_t encryption_type;
    uint16_t authentication_type;
    uint8_t  network_key_length;
    uint8_t  network_key[64];
} cy_wps_internal_credential_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

extern void         cy_wps_send_eapol_packet       ( cy_packet_t packet, cy_wps_agent_t* workspace, cy_eapol_packet_type_t type, whd_mac_t* their_mac_address, uint16_t content_size );
extern cy_rslt_t    cy_wps_send_basic_packet       ( cy_wps_agent_t* workspace, uint8_t type, uint16_t optional_config_error );
extern void         cy_wps_enrollee_init           ( cy_wps_agent_t* workspace );
extern void         cy_wps_enrollee_start          ( cy_wps_agent_t* workspace, whd_interface_t interface );
extern void         cy_wps_enrollee_reset          ( cy_wps_agent_t* workspace, whd_interface_t interface );
extern void         cy_wps_registrar_init          ( cy_wps_agent_t* workspace );
extern void         cy_wps_registrar_start         ( cy_wps_agent_t* workspace );
extern void         cy_wps_registrar_reset         ( cy_wps_agent_t* workspace );
extern cy_rslt_t    cy_wps_pbc_overlap_check       ( const whd_mac_t* data );
extern void         cy_wps_clear_pbc_overlap_array ( void );
extern void         cy_wps_record_last_pbc_enrollee( const whd_mac_t* mac );
extern void         cy_wps_update_pbc_overlap_array( cy_wps_agent_t* workspace, const whd_mac_t* mac );
extern void         cy_wps_pbc_overlap_array_notify( const whd_mac_t* mac );
extern cy_rslt_t    cy_wps_process_event(cy_wps_agent_t* workspace, cy_event_message_t* event);
extern void         cy_wps_init_workspace(cy_wps_agent_t* workspace);
extern void         cy_wps_deinit_workspace(cy_wps_agent_t* workspace);

extern cy_rslt_t    cy_wps_process_event( cy_wps_agent_t* workspace, cy_event_message_t* event );
extern void         cy_wps_init_workspace       ( cy_wps_agent_t* workspace );
extern void         cy_wps_deinit_workspace     ( cy_wps_agent_t* workspace );
extern void         cy_wps_reset_workspace      ( cy_wps_agent_t* workspace, whd_interface_t interface );
extern void         cy_wps_scan_result_handler  ( whd_scan_result_t* result, void* user_data );
extern void         cy_wps_prepare_workspace_crypto   ( cy_wps_agent_t* workspace );
extern cy_rslt_t    cy_wps_advertise_registrar( cy_wps_agent_t* workspace, uint8_t selected_registrar );
extern void         cy_wps_register_result_callback( cy_wps_agent_t* workspace, void (*wps_result_callback)(cy_rslt_t*) );

void                cy_wps_register_internal_result_callback( cy_wps_agent_t* workspace, void (*wps_internal_result_callback)(cy_rslt_t*) );


/* Association functions */
extern cy_rslt_t    cy_wps_host_join( void* workspace, cy_wps_ap_t* ap, whd_interface_t interface );
extern cy_rslt_t    cy_wps_host_leave( whd_interface_t interface );

/* Timing functions */
extern void         cy_wps_host_start_timer( void* workspace, uint32_t timeout );
extern void         cy_wps_host_stop_timer ( void* workspace );

/* IE management functions */
extern void         cy_wps_host_add_vendor_ie   ( uint32_t interface, void* data, uint16_t data_length, uint32_t packet_mask );
extern void         cy_wps_host_remove_vendor_ie( uint32_t interface, void* data, uint16_t data_length, uint32_t packet_mask );

/* Scanning functions */
extern void         cy_wps_host_scan                 ( cy_wps_agent_t* workspace, cy_wps_scan_handler_t result_handler, whd_interface_t interface );
extern cy_wps_ap_t* cy_wps_host_store_ap             ( void* workspace, whd_scan_result_t* scan_result, cy_wps_uuid_t* uuid );
extern cy_wps_ap_t* cy_wps_host_retrieve_ap          ( void* workspace );
extern uint16_t     cy_wps_host_get_ap_list_size     ( void* workspace);
extern cy_rslt_t    cy_wps_enrollee_pbc_overlap_check( cy_wps_agent_t* workspace );

/* Credential management functions */
extern void         cy_wps_host_store_credential   ( void* workspace, cy_wps_internal_credential_t* credential );
extern void         cy_wps_host_retrieve_credential( void* workspace, cy_wps_internal_credential_t* credential );

/* Authorized MACs API */
extern void         cy_wps_host_get_authorized_macs( void* workspace, whd_mac_t** mac_list, uint8_t* mac_list_length );

uint32_t            cy_hton32(uint32_t intlong);
uint16_t            cy_hton16(uint16_t intshort);

/* Helper functions for WPS */
cy_rslt_t           cy_host_random_bytes( void* buffer, size_t buffer_length, size_t* output_length );
void                cy_host_start_timer( void* workspace, uint32_t timeout );

#ifdef __cplusplus
} /*extern "C" */
#endif
