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
* @file cy_wps_structures.h
* @brief Cypress WPS structures
*/
#pragma once
#include <stdint.h>
#include "cy_wps_crypto.h"
#include "tlv.h"
#include "cy_wps_constants.h"
#include "whd.h"
#include "cy_eapol.h"
#include "cy_wps_result.h"
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/
#define CY_WPS_SSID_LENGTH             32
#define CY_WPS_PASSPHRASE_LENGTH       63
/******************************************************
 *                   Enumerations
 ******************************************************/

typedef enum
{
    CY_EVENT_NO_EVENT,
    CY_EVENT_TIMER_TIMEOUT,
    CY_EVENT_ABORT_REQUESTED,
    CY_EVENT_EAPOL_PACKET_RECEIVED,
    CY_EVENT_RECEIVED_IDENTITY_REQUEST,
    CY_EVENT_COMPLETE,
    CY_SUPPLICANT_EVENT_PACKET_TO_SEND,
    CY_WPS_EVENT_DISCOVER_COMPLETE,
    CY_WPS_EVENT_ENROLLEE_ASSOCIATED,
    CY_WPS_EVENT_RECEIVED_IDENTITY,
    CY_WPS_EVENT_RECEIVED_WPS_START,
    CY_WPS_EVENT_RECEIVED_EAPOL_START,
    CY_WPS_EVENT_PBC_OVERLAP_NOTIFY_USER,
} cy_wps_event_t;

typedef enum
{
    CY_WPS_PBC_MODE = 0,
    CY_WPS_PIN_MODE = 1,
    CY_WPS_NFC_MODE = 2,
} cy_wps_mode_t;

typedef enum
{
    CY_WPS_INTERNAL_DEVICE_COMPUTER               = 1,
    CY_WPS_INTERNAL_DEVICE_INPUT                  = 2,
    CY_WPS_INTERNAL_DEVICE_PRINT_SCAN_FAX_COPY    = 3,
    CY_WPS_INTERNAL_DEVICE_CAMERA                 = 4,
    CY_WPS_INTERNAL_DEVICE_STORAGE                = 5,
    CY_WPS_INTERNAL_DEVICE_NETWORK_INFRASTRUCTURE = 6,
    CY_WPS_INTERNAL_DEVICE_DISPLAY                = 7,
    CY_WPS_INTERNAL_DEVICE_MULTIMEDIA             = 8,
    CY_WPS_INTERNAL_DEVICE_GAMING                 = 9,
    CY_WPS_INTERNAL_DEVICE_TELEPHONE              = 10,
    CY_WPS_INTERNAL_DEVICE_AUDIO                  = 11,
    CY_WPS_INTERNAL_DEVICE_OTHER                  = 0xFF,
} cy_wps_internal_device_category_t;

typedef enum
{
    CY_WPS_ENROLLEE_AGENT  = 0,
    CY_WPS_REGISTRAR_AGENT = 1,
} cy_wps_agent_type_t;

/* High level states
 * INITIALISING ( scan, join )
 * EAP_HANDSHAKE ( go through EAP state machine )
 * WPS_HANDSHAKE ( go through WPS state machine )
 */
typedef enum
{
    CY_WPS_INITIALISING,
    CY_WPS_IN_WPS_HANDSHAKE,
    CY_WPS_CLOSING_EAP,
} cy_wps_main_stage_t;


/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef uint8_t   wps_bool_t;
typedef void*     cy_packet_t;

/******************************************************
 *                Packed Structures
 ******************************************************/
typedef union
{
    cy_packet_t           packet;
    uint32_t              value;
} cy_supplicant_event_message_data_t;

typedef struct
{
    cy_wps_event_t                    event_type;
    cy_supplicant_event_message_data_t data;
} cy_event_message_t;

typedef struct
{
    uint32_t len;
    uint32_t num[48];
} cy_wps_NN_t;

typedef void (*cy_wps_scan_handler_t)(whd_scan_result_t* result, void* user_data);

typedef struct
{
    cy_wps_internal_device_category_t device_category;
    uint16_t                          sub_category;
    char*                             device_name;
    char*                             manufacturer;
    char*                             model_name;
    char*                             model_number;
    char*                             serial_number;
    uint32_t                          config_methods;
    uint32_t                          os_version;
    uint16_t                          authentication_type_flags;
    uint16_t                          encryption_type_flags;
    uint8_t                           add_config_methods_to_probe_resp;
} cy_wps_device_detail_t;

typedef struct
{
    uint8_t          ssid[CY_WPS_SSID_LENGTH + 1];
    whd_security_t   security;
    uint8_t          passphrase[CY_WPS_PASSPHRASE_LENGTH + 1];
} cy_wps_credential_t;

/* DSS Parameter Set */
typedef struct
{
    uint8_t type;
    uint8_t length;
    uint8_t current_channel;
} cy_dsss_parameter_set_ie_t;

/* In dongle firmware this is defined as ht_add_ie_t in 802.11.h. It has similar structure but different field names due to history.
 * Wireshark reports this element as the HT information element. */
typedef struct
{
    uint8_t       type;
    uint8_t       length;
    uint8_t       primary_channel;
    uint8_t       ht_operation_subset_1;
    uint16_t      ht_operation_subset_2;
    uint16_t      ht_operation_subset_3;
    uint8_t       rx_supported_mcs_set[16];
} cy_ht_operation_ie_t;

#pragma pack(1)

typedef struct
{
    uint8_t nonce[SIZE_128_BITS];
} cy_wps_nonce_t;

typedef struct
{
    uint8_t octet[SIZE_256_BITS];
} cy_wps_hash_t;

typedef struct
{
    uint8_t octect[SIZE_128_BITS]; /* Half of a hash */
} cy_wps_psk_t;

typedef struct
{
    uint8_t octet[16];
} cy_wps_uuid_t;

typedef struct
{
    uint8_t octet[SIZE_256_BITS];
} cy_auth_key_t;

typedef struct
{
    uint8_t octet[SIZE_128_BITS];
} cy_wps_iv_t;

typedef struct
{
    uint8_t octet[SIZE_128_BITS];
} cy_key_wrap_key_t;

typedef struct
{
    uint8_t octet[SIZE_256_BITS];
} cy_emsk_t;

typedef struct
{
    uint8_t key[SIZE_1536_BITS];
} cy_public_key_t;

typedef struct
{
    uint8_t octet[SIZE_ETHERNET_ADDRESS];
} cy_ethernet_address_t;

typedef struct
{
    cy_wps_nonce_t            enrollee_nonce;
    cy_ethernet_address_t     enrollee_mac;
    cy_wps_nonce_t            registrar_nonce;
} cy_kdk_input_t;

typedef struct
{
    cy_auth_key_t     auth_key;
    cy_key_wrap_key_t key_wrap_key;
    cy_emsk_t         emsk;
} cy_wps_session_keys_t;

typedef union
{
    cy_wps_session_keys_t keys;
    uint8_t               data[WPS_PADDED_AES_ROUND_UP( sizeof(cy_wps_session_keys_t), SIZE_256_BITS )];
} cy_wps_session_key_derivation_output_t;

typedef struct
{
    uint8_t iv[SIZE_128_BITS];
    uint8_t data[1];
} cy_wps_encryption_data_t;

typedef struct
{
    uint8_t vendor_id[3];
    uint8_t data[1];
} cy_general_vendor_ext_t;

typedef struct
{
    uint8_t      vendor_id[3];
    tlv8_uint8_t subid_version2;
} cy_vendor_ext_t;

typedef struct
{
    uint8_t      vendor_id[3];
    tlv8_uint8_t subid_version2;
    tlv8_uint8_t request_to_enroll;
} cy_wps_m1_vendor_ext_t;

typedef struct
{
    cy_wps_nonce_t  secret;  /* This will be es1/rs1 or es2/rs2 */
    cy_wps_psk_t    psk;     /* This will be  psk1   or  psk2 */
    cy_public_key_t enrollee_public_key;
    cy_public_key_t registrar_public_key;
} cy_wps_hash_input_t;

typedef struct
{
    uint16_t category;
    uint32_t oui;
    uint16_t sub_category;
} cy_wps_primary_device_type_t;

typedef struct
{
    cy_ether_header_t        ethernet;
    cy_eapol_header_t        eapol;
    cy_eap_header_t          eap;
    cy_eap_expanded_header_t eap_expanded;
    uint8_t                  data[1];
} cy_wps_msg_packet_t;

typedef struct
{
    cy_ether_header_t        ethernet;
    cy_eapol_header_t        eapol;
    cy_eap_header_t          eap;
    cy_eap_expanded_header_t eap_expanded;
} cy_wps_msg_packet_header_t;

#pragma pack()


/******************************************************
 *                Unpacked Structures
 ******************************************************/
/*
 * This structure contains data that is unique for both agent but contained by both.
 * Each agent starts with their public_key and nonce and progressively generates the secret nonces and hashes while also
 * learning the other agent's public key, nonces and hashes.
 *
 * psk : Generated from the password
 */
typedef struct
{
    cy_public_key_t public_key;
    cy_wps_nonce_t  nonce;
    cy_wps_nonce_t  secret_nonce[2];
    cy_wps_hash_t   secret_hash[2];
    whd_mac_t       mac_address;
    uint8_t         supported_version;
    uint16_t        authTypeFlags;
    uint16_t        encrTypeFlags;
} cy_wps_agent_data_t;


typedef struct
{
    whd_scan_result_t scan_result;
    cy_wps_uuid_t     uuid;
} cy_wps_ap_t;

typedef struct _wps_agent_t cy_wps_agent_t;

typedef cy_rslt_t (*cy_wps_event_handler_t)(cy_wps_agent_t* workspace, cy_event_message_t* message);

struct _wps_agent_t
{
    /* Persistent variables between WPS attempts:
     * - device_details
     * - wps_mode
     * - agent_type
     * - wps_host_workspace
     * - password
     * - interface
     */
    const cy_wps_device_detail_t*   device_details;
    cy_wps_mode_t                   wps_mode;
    cy_wps_agent_type_t             agent_type;
    void*                           wps_host_workspace;
    whd_interface_t                 interface;

    /* Variables that need to be refactored */
    uint8_t                         connTypeFlags;
    uint8_t                         rfBand;
    uint8_t                         scState;
    uint16_t                        association_state;
    uint32_t                        osVersion;
    cy_wps_uuid_t                   uuid;
    cy_wps_primary_device_type_t    primary_device;

    /* WPS cryptography data for both WPS participants */
    cy_wps_agent_data_t             my_data;
    cy_wps_agent_data_t             their_data;

    /* Pointers to allow a single structure type be used for both enrollee and registrar */
    cy_wps_agent_data_t*            registrar_data;
    cy_wps_agent_data_t*            enrollee_data;

    /* Natural Number versions of keys used for quick calculations */
    cy_wps_NN_t                     my_private_key;

    /* Password and derived PSK */
    const char*                     password;
    uint16_t                        device_password_id;
    cy_wps_psk_t                    psk[2];

    cy_rslt_t                       wps_result;

    /* Session keys */
    cy_auth_key_t                   auth_key;
    cy_key_wrap_key_t               key_wrap_key;
    cy_emsk_t                       emsk;

    /* Progressive HMAC workspace */
    cy_sha2_hmac_context            hmac;

    /* State machine stages */
    cy_wps_main_stage_t             current_main_stage;
    uint8_t                         current_sub_stage; /* Either a value from wps_eap_state_machine_stage_t or wps_state_machine_stage_t */
    uint8_t                         retry_counter;

    uint32_t                        available_crypto_material;

    /* The ID of the last received packet we should use when replying */
    uint8_t                         last_received_id;

    /* Fragment packet processing flag */
    uint8_t                         processing_fragmented_packet;
    uint8_t*                        fragmented_packet;
    uint16_t                        fragmented_packet_length;
    uint16_t                        fragmented_packet_length_max;

    /* Event handler for all events that occur during the INITIALIZING and IN_EAP_HANDSHAKE stages */
    cy_wps_event_handler_t          event_handler;

    /* Pointer to the owner of the WPS agent, for example a P2P group owner workspace */
    void*                           wps_agent_owner;

    /* IE elements for both Enrollee and Registrar */
    union
    {
        struct
        {
            cy_ie_t probe_request;
            cy_ie_t association_request;
        } enrollee;
        struct
        {
            cy_ie_t probe_response;
            cy_ie_t association_response;
            cy_ie_t beacon;
        } registrar;
        cy_ie_t common[3];
    } ie;

    uint8_t                         in_reverse_registrar_mode;

    uint32_t                        start_time;

    /*
     * Enrollee only variables
     */
    /* Current target AP */
    cy_wps_ap_t*                    ap;
    uint32_t                        directed_wps_max_attempts;
    uint8_t                         ap_join_attempts;
    uint8_t                         identity_request_received_count;
    whd_band_list_t                 band_list;

    /* Copy of M1 to be used for hashing when we receive M2 */
    uint8_t*                        m1_copy;
    uint16_t                        m1_copy_length;

    /* P2P related variables */
    uint8_t                         is_p2p_enrollee;
    uint8_t                         is_p2p_registrar;

    void                            (*cy_wps_result_callback)         (cy_rslt_t*); /*!< WPS result callback for applications */
    void                            (*cy_wps_internal_result_callback)(cy_rslt_t*); /*!< WPS internal result callback for use by P2P and other BESL features */
};

#ifdef __cplusplus
} /*extern "C" */
#endif
