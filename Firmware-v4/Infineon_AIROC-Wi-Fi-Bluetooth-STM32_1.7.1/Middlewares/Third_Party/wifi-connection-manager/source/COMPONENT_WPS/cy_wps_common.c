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
* @file cy_wps_common.c
* @brief WPS common file for handling M1 to M8 WPS messages
*/


#include "cy_wps_common.h"

#include <stdint.h>
#include <string.h>

#include "cy_template_wps_packets.h"
#include "cy_wps_constants.h"
#include "cy_wps_structures.h"
#include "cy_wcm_log.h"
#include "cy_wps_crypto.h"
#include "nn.h"
#include "whd_int.h"
#include "whd_buffer_api.h"
#include "whd_types.h"
#include "cyhal.h"
#ifdef COMPONENT_MBEDTLS
#include "entropy_poll.h"
#endif

/******************************************************
 *                      Macros
 ******************************************************/

#define wps_assert(x, y)
#define WPS_DH_GENERATE_KEY(a, b)    DH_generate_key(a, b)

/* Returns the opposite agent type (enrollee changes to registrar. registrar changes to enrollee) */
#define OPPOSITE_AGENT_TYPE(type)    (( workspace->agent_type + 1 ) & 1)

/******************************************************
 *                    Constants
 ******************************************************/
#define AES_BLOCK_SZ                         16
#define ETHER_TYPE_802_1X                    0x888e              /* 802.1x */

#define MAX_PERSONALIZATION_STRING_SIZE      64 /* This may need to be modified */

#define PRIVATE_KEY_NN_LENGTH        (12)       /* 12 x 32 bits = 384 bit private key */
#define PRIVATE_KEY_BYTE_LENGTH      (PRIVATE_KEY_NN_LENGTH * 4)

#define WPS_ENCRYPTION_BLOCK_SIZE    (16)
#define WPS_AUTHENTICATOR_LEN        (8)

#define WPS_LENGTH_FIELD_MASK        (0x02)
#define WPS_MORE_FRAGMENTS_MASK      (0x01)
#define WPS_MESSAGE_TYPE_M1          (0x04)
#ifndef TRUE
#define TRUE   (1)
#endif /* ifndef TRUE */

#ifndef FALSE
#define FALSE  (0)
#endif /* ifndef FALSE */

#ifdef ENABLE_WCM_LOGS
#define cy_wcm_log_msg cy_log_msg
#else
#define cy_wcm_log_msg(a,b,c,...)
#endif

static const uint8_t DH_P_VALUE[SIZE_1536_BITS] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
    0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
    0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
    0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
    0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
    0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
    0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
    0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
    0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
    0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
    0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
    0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
    0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
    0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
    0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x23, 0x73, 0x27,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#define KDK_REQUIRED_CRYPTO_MATERIAL          (CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE | CY_WPS_CRYPTO_MATERIAL_REGISTRAR_NONCE | CY_WPS_CRYPTO_MATERIAL_ENROLLEE_MAC_ADDRESS)

/******************************************************
 *                   Enumerations
 ******************************************************/

typedef enum
{
    CY_WPS_HASH1_INDEX,
    CY_WPS_HASH2_INDEX,
    CY_WPS_SNONCE1_INDEX,
    CY_WPS_SNONCE2_INDEX,
    CY_WPS_NONCE_INDEX,
    CY_WPS_UUID_INDEX,
} cy_wps_agent_specific_tlv_index_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/* Message content creation functions */
static uint8_t*     cy_wps_write_nonce                 ( cy_wps_agent_t* workspace, uint8_t* iter );
static uint8_t*     cy_wps_write_hashes                ( cy_wps_agent_t* workspace, uint8_t* iter );
static uint8_t*     cy_wps_write_secret1               ( cy_wps_agent_t* workspace, uint8_t* iter );
static uint8_t*     cy_wps_write_secret2               ( cy_wps_agent_t* workspace, uint8_t* iter );
#ifdef WCM_ENABLE_WPS_REGISTRAR
static uint8_t*     cy_wps_write_credentials           ( cy_wps_agent_t* workspace, uint8_t* iter );
#endif
static uint8_t*     cy_wps_write_common_header         ( cy_wps_agent_t* workspace, uint8_t* start_of_packet, uint8_t message_type );
static uint8_t*     cy_wps_write_vendor_extension      ( uint8_t* iter, cy_wps_agent_t* workspace );
static uint8_t*     cy_wps_start_encrypted_tlv         ( uint8_t* iter);
static uint8_t*     cy_wps_end_encrypted_tlv           ( cy_wps_agent_t* workspace, uint8_t* start_of_encrypted_tlv_header, uint8_t* end_of_data );
static void         cy_wps_send_protocol_message       ( cy_wps_agent_t* workspace, cy_packet_t* packet, uint8_t* start_of_packet, uint8_t* iter );
static void         cy_wps_encrypt_data                ( uint8_t* input, uint16_t input_length, cy_key_wrap_key_t* encr_key, uint8_t* output, cy_wps_iv_t* iv );
static cy_rslt_t    cy_wps_send_wsc_nack               ( cy_wps_agent_t* workspace, uint16_t config_error);
static cy_rslt_t    cy_wps_send_done                   ( cy_wps_agent_t* workspace );

/* Message processing functions */
static cy_rslt_t    cy_wps_process_message_content     ( cy_wps_agent_t* workspace, uint8_t* content, uint16_t content_length, uint32_t tlv_mask );
static cy_rslt_t    cy_wps_process_encrypted_tlvs      ( cy_wps_agent_t* workspace, cy_wps_encryption_data_t* encrypted_data, uint16_t encrypted_data_length, uint32_t valid_tlv_mask );
static cy_rslt_t    cy_wps_process_credential          ( cy_wps_agent_t* workspace, uint8_t* data, uint16_t data_length );
static int          cy_wps_decrypt_data                ( cy_wps_encryption_data_t* encrypted_data, uint16_t encrypted_data_length, cy_key_wrap_key_t* encr_key, uint8_t* plain_text);
static cy_rslt_t    cy_wps_validate_secret_nonce       ( cy_wps_agent_t* workspace, cy_wps_agent_data_t* agent_data, uint8_t* data, uint8_t which_nonce);
static uint8_t*     cy_get_wps_packet_data             ( cy_wps_msg_packet_t* packet);

/* Packet fragmentation functions */
static cy_rslt_t    cy_wps_process_packet_fragmentation(cy_wps_agent_t* workspace, cy_packet_t eapol_packet, uint8_t** data, uint16_t* data_size );
static void         cy_wps_free_unfragmented_packet    ( cy_wps_agent_t* workspace );
static void         cy_wps_init_unfragmented_packet    ( cy_wps_agent_t* workspace, uint16_t total_length );
static void         cy_wps_append_fragment             ( cy_wps_agent_t* workspace, void* fragment, uint16_t fragment_length );
static void         cy_wps_retrieve_unfragmented_packet( cy_wps_agent_t* workspace, cy_packet_t* packet, uint16_t* packet_length );
static uint32_t     cy_wps_send_frag_ack               ( cy_wps_agent_t* workspace );

/* Event handling functions */
static cy_rslt_t    cy_wps_common_event_handler        (cy_wps_agent_t* workspace, cy_event_message_t* message);

/* Calculation functions */
static cy_rslt_t    cy_wps_calculate_kdk               ( cy_wps_agent_t* workspace );
static cy_rslt_t    cy_wps_calculate_kdf               ( uint8_t* key, uint16_t key_length, char* personalization_string, uint8_t* output, uint16_t output_length );
static cy_rslt_t    cy_wps_calculate_hash              ( cy_wps_agent_t* workspace, cy_wps_agent_data_t* source, cy_wps_hash_t* output, uint8_t hash );
static cy_rslt_t    cy_wps_calculate_psk               ( cy_wps_agent_t* workspace );

static void         cy_wps_cleanup_workspace           ( cy_wps_agent_t* workspace );

#ifdef COMPONENT_4390X
extern cy_rslt_t cy_prng_get_random( void* buffer, uint32_t buffer_length );
#endif
/******************************************************
 *               Variable Definitions
 ******************************************************/

static uint32_t DH_G_VALUE = 2;

static const uint16_t agent_specific_tlv_id[2][6] =
{
    [CY_WPS_ENROLLEE_AGENT] =
    {
        [CY_WPS_HASH1_INDEX]   = WPS_ID_E_HASH1,
        [CY_WPS_HASH2_INDEX]   = WPS_ID_E_HASH2,
        [CY_WPS_SNONCE1_INDEX] = WPS_ID_E_SNONCE1,
        [CY_WPS_SNONCE2_INDEX] = WPS_ID_E_SNONCE2,
        [CY_WPS_NONCE_INDEX]   = WPS_ID_ENROLLEE_NONCE,
        [CY_WPS_UUID_INDEX]    = WPS_ID_UUID_E,
    },
    [CY_WPS_REGISTRAR_AGENT] =
    {
        [CY_WPS_HASH1_INDEX]   = WPS_ID_R_HASH1,
        [CY_WPS_HASH2_INDEX]   = WPS_ID_R_HASH2,
        [CY_WPS_SNONCE1_INDEX] = WPS_ID_R_SNONCE1,
        [CY_WPS_SNONCE2_INDEX] = WPS_ID_R_SNONCE2,
        [CY_WPS_NONCE_INDEX]   = WPS_ID_REGISTRAR_NONCE,
        [CY_WPS_UUID_INDEX]    = WPS_ID_UUID_R,
    }
};

const cy_wps_state_machine_state_t wps_states[2][4] =
{
    [CY_WPS_ENROLLEE_AGENT] =
    {
        // Sending M1, Receiving M2
        [CY_WPS_SENDING_PUBLIC_KEYS] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M2,
            .outgoing_message_type = WPS_ID_MESSAGE_M1,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_ENROLLEE_NONCE | CY_WPS_TLV_REGISTRAR_NONCE | CY_WPS_TLV_UUID_R | CY_WPS_TLV_PUBLIC_KEY | CY_WPS_TLV_AUTH_TYPE_FLAGS | CY_WPS_TLV_ENCR_TYPE_FLAGS | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = 0,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_nonce,
        },
        // Sending M3, Receiving M4
        [CY_WPS_SENDING_SECRET_HASHES] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M4,
            .outgoing_message_type = WPS_ID_MESSAGE_M3,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_ENROLLEE_NONCE | CY_WPS_TLV_R_HASH1 | CY_WPS_TLV_R_HASH2 | CY_WPS_TLV_ENCRYPTION_SETTINGS | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = CY_WPS_TLV_R_SNONCE1,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_hashes,
        },
        // Sending M5, Receiving M6
        [CY_WPS_SENDING_SECRET_NONCE1] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M6,
            .outgoing_message_type = WPS_ID_MESSAGE_M5,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_ENROLLEE_NONCE | CY_WPS_TLV_ENCRYPTION_SETTINGS | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = CY_WPS_TLV_R_SNONCE2,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_secret1,
        },
        // Sending M7, Receiving M8
        [CY_WPS_SENDING_SECRET_NONCE2] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M8,
            .outgoing_message_type = WPS_ID_MESSAGE_M7,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_ENROLLEE_NONCE | CY_WPS_TLV_ENCRYPTION_SETTINGS | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = CY_WPS_TLV_CREDENTIAL | CY_WPS_TLV_SSID | CY_WPS_TLV_AUTH_TYPE | CY_WPS_TLV_ENCR_TYPE | CY_WPS_TLV_NW_KEY | CY_WPS_TLV_MAC_ADDR ,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_secret2,
        },
    },

#ifdef WCM_ENABLE_WPS_REGISTRAR
    [CY_WPS_REGISTRAR_AGENT] =
    {
        // Receiving M1, Sending M2
        [CY_WPS_SENDING_PUBLIC_KEYS] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M1,
            .outgoing_message_type = WPS_ID_MESSAGE_M2,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_ENROLLEE_NONCE | CY_WPS_TLV_PUBLIC_KEY | CY_WPS_TLV_MAC_ADDR | CY_WPS_TLV_AUTH_TYPE_FLAGS | CY_WPS_TLV_ENCR_TYPE_FLAGS ),
            .encrypted_tlv_mask    = 0,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_nonce,
        },
        // Receiving M3, Sending M4
        [CY_WPS_SENDING_SECRET_HASHES] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M3,
            .outgoing_message_type = WPS_ID_MESSAGE_M4,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_REGISTRAR_NONCE | CY_WPS_TLV_E_HASH1 | CY_WPS_TLV_E_HASH2  | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = 0,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_hashes,
        },
        // Receiving M5, Sending M6
        [CY_WPS_SENDING_SECRET_NONCE1] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M5,
            .outgoing_message_type = WPS_ID_MESSAGE_M6,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_REGISTRAR_NONCE | CY_WPS_TLV_ENCRYPTION_SETTINGS | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = CY_WPS_TLV_E_SNONCE1,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_secret2,
        },
        // Receiving M7, Sending M8
        [CY_WPS_SENDING_CREDENTIALS] =
        {
            .valid_message_type    = WPS_ID_MESSAGE_M7,
            .outgoing_message_type = WPS_ID_MESSAGE_M8,
            .tlv_mask              = (CY_WPS_TLV_VERSION | CY_WPS_TLV_MSG_TYPE | CY_WPS_TLV_REGISTRAR_NONCE | CY_WPS_TLV_ENCRYPTION_SETTINGS | CY_WPS_TLV_AUTHENTICATOR),
            .encrypted_tlv_mask    = CY_WPS_TLV_E_SNONCE2,
            .packet_generator      = (cy_wps_packet_generator_t)cy_wps_write_credentials,
        },
    },
#endif
};

/******************************************************
 *               Function Definitions
 ******************************************************/
/* Helper functions for WPS */
void cy_host_start_timer( void* workspace, uint32_t timeout )
{
    cy_host_workspace_t* host = (cy_host_workspace_t*) workspace;
    cy_rtos_get_time( &host->timer_reference );
    host->timer_timeout = timeout;
}

void cy_host_stop_timer( void* workspace )
{
    cy_host_workspace_t* host = (cy_host_workspace_t*) workspace;
    host->timer_timeout = 0;
}

uint32_t cy_host_get_current_time( void )
{
    cy_time_t time;
    cy_rtos_get_time(&time);
    return time;
}

uint32_t cy_host_get_timer( void* workspace )
{
    cy_host_workspace_t* host = (cy_host_workspace_t*)workspace;
    return host->timer_timeout;
}

cy_rslt_t cy_host_random_bytes( void* buffer, size_t buffer_length, size_t* output_length )
{
#ifndef COMPONENT_4390X
#ifdef COMPONENT_MBEDTLS
    int result = 0;
    size_t length;

    result = mbedtls_hardware_poll(NULL, (unsigned char*)buffer, buffer_length, &length);
    if(result != 0)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "mbedtls_hardware_poll failed \r\n");
        return CY_RSLT_WPS_ERROR;
    }

    *output_length = length;
#endif
#else
    /* 43907 kits does not have TRNG module. Get the random
     * number from wifi-mw-core internal PRNG API. */
    cy_rslt_t result;
    result = cy_prng_get_random(buffer, buffer_length);
    if(result != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_prng_get_random failed \r\n");
        return result;
    }
    *output_length = buffer_length;
#endif
    return CY_RSLT_SUCCESS;
}

uint32_t cy_hton32(uint32_t intlong)
{
    return htobe32(intlong);
}

uint16_t cy_hton16(uint16_t intshort)
{
    return htobe16(intshort);
}

cy_rslt_t cy_wps_process_event(cy_wps_agent_t* workspace, cy_event_message_t* event)
{
    cy_rslt_t result;

    /* Check for an abort */
    if (event->event_type == CY_EVENT_ABORT_REQUESTED)
    {
        workspace->wps_result = CY_RSLT_WPS_ABORTED;
        return CY_RSLT_SUCCESS;
    }

    /* First try the agent specific event handler */
    result = workspace->event_handler(workspace, event);
    if (result == CY_RSLT_WPS_UNPROCESSED)
    {
        /* Pass it to the common event handler */
        result = cy_wps_common_event_handler(workspace, event);
    }
    else if (result == CY_RSLT_WPS_OTHER_ENROLLEE)
    {
        result = CY_RSLT_SUCCESS;
    }

    return result;
}

static cy_rslt_t cy_wps_common_event_handler(cy_wps_agent_t* workspace, cy_event_message_t* message)
{
    uint8_t            message_type = 0;
    uint8_t*           wps_packet_data;
    uint16_t           wps_packet_data_size;
    cy_packet_t        outgoing_packet;
    uint8_t*           iter;
    cy_rslt_t          result = CY_RSLT_SUCCESS;
    cy_wps_msg_packet_t*  packet_header;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Received WPS event type = [%d]\r\n", message->event_type);
    switch(message->event_type)
    {
        case CY_EVENT_EAPOL_PACKET_RECEIVED:
            if ( workspace->current_main_stage != CY_WPS_IN_WPS_HANDSHAKE )
            {
                /* We can ignore this packet */
                goto return_with_packet;
            }

            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Process Packet Fragmentation\r\n");
            result = cy_wps_process_packet_fragmentation(workspace, message->data.packet, &wps_packet_data, &wps_packet_data_size);
            if (result == CY_RSLT_WPS_IN_PROGRESS)
            {
                result = CY_RSLT_SUCCESS;
                goto return_with_packet;
            }
            else if (result != CY_RSLT_SUCCESS)
            {
                goto return_with_packet_and_maybe_fragment;
            }

            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Processing received packet\r\n");

            packet_header            = (cy_wps_msg_packet_t*)wps_packet_data;
            wps_packet_data          = cy_get_wps_packet_data( packet_header );
            if (wps_packet_data == NULL)
            {
                result = CY_RSLT_WPS_ERROR_INCORRECT_MESSAGE;
                goto return_with_packet_and_maybe_fragment;
            }
            wps_packet_data_size     = (uint16_t)(wps_packet_data_size - (wps_packet_data - (uint8_t*)packet_header));

            /* Check if it is a packet we care about */
            if ( packet_header->eap.type != CY_EAP_TYPE_WPS )
            {
                /* We can ignore this packet */
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Packet type is not WPS. Type = [%d]\r\n", packet_header->eap.type);
                goto return_with_packet_and_maybe_fragment;
            }

            if ( packet_header->eap.code == CY_EAP_CODE_FAILURE )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Received EAP Fail\r\n");
                result = CY_RSLT_WPS_ERROR_RECEIVED_EAP_FAIL;
                goto return_with_packet_and_maybe_fragment;
            }

            /* Some registrars send a second WSC start so we have to check the packet length or we may crash */
            if ( wps_packet_data_size == 0 )
            {
                /* We can ignore this packet */
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS pkt size 0\r\n");
                goto return_with_packet_and_maybe_fragment;
            }

            /* Find the message type TLV */
            if ( tlv_read_value( WPS_ID_MSG_TYPE, wps_packet_data, wps_packet_data_size, &message_type, 1, TLV_UINT8 ) != TLV_SUCCESS )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Message type couldn't be found\r\n");
                result = CY_RSLT_WPS_ERROR_MESSAGE_MISSING_TLV;
                goto return_with_packet_and_maybe_fragment;
            }

            // Reverse registrar mode check
            if ( workspace->in_reverse_registrar_mode != 0 && message_type == WPS_ID_MESSAGE_M2D )
            {
                // Send NACK
                cy_wps_send_wsc_nack(workspace, 0);

                workspace->agent_type = CY_WPS_REGISTRAR_AGENT;



                result = CY_RSLT_WPS_ATTEMPTED_EXTERNAL_REGISTRAR_DISCOVERY;
                goto return_with_packet_and_maybe_fragment;
            }

            /* Check if we are a Registrar and this message is the reply for our last send packet. If so we can move to next sub-stage */
            if ( ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT ) && ( message_type == wps_states[workspace->agent_type][workspace->current_sub_stage + 1].valid_message_type ) )
            {
                ++workspace->current_sub_stage;
            }

            /* Check if this message is valid for the current state */
            if ( message_type == wps_states[workspace->agent_type][workspace->current_sub_stage].valid_message_type )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Received message %d\r\n", message_type);

                if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
                {
                    workspace->last_received_id = packet_header->eap.id;
                }

                result = cy_wps_process_message_content( workspace, wps_packet_data, wps_packet_data_size, wps_states[workspace->agent_type][workspace->current_sub_stage].tlv_mask );
                if ( result != CY_RSLT_SUCCESS )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS: Processing message error\r\n");
                    if (result == CY_RSLT_WPS_ERROR_SECRET_NONCE_MISMATCH)
                    {
                        cy_wps_send_wsc_nack(workspace, 18);
                    }
                    else if (result == CY_RSLT_WPS_PBC_OVERLAP)
                    {
                        cy_wps_send_wsc_nack(workspace, 12); // XXX
                    }
                    else
                    {
                        if (result == CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS)
                        {
                            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS: Received wep credentials\r\n");
                            /* Terminate WPS */
                            workspace->wps_result = CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS;
                        }
                        cy_wps_send_wsc_nack(workspace, 0); // XXX
                    }
                    goto return_with_packet_and_maybe_fragment;
                }

                if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
                {
                    /* Move to next stage as we've received the reply to our packet */
                    ++workspace->current_sub_stage;
                }
            }
            /* Check if we are an enrollee and we've received an M2D after sending M1 */
            else if (workspace->agent_type == CY_WPS_ENROLLEE_AGENT && workspace->current_sub_stage == CY_WPS_SENDING_PUBLIC_KEYS && message_type == WPS_ID_MESSAGE_M2D)
            {
                workspace->last_received_id = packet_header->eap.id;

                // Send ACK
                cy_wps_send_basic_packet(workspace, WPS_ID_MESSAGE_ACK, 0);
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Sending ACK\r\n");

                // Wait for more M2D, but if we time out we should join to another AP
                cy_host_start_timer( workspace->wps_host_workspace, 1000 );
                goto return_with_packet_and_maybe_fragment;
            }
            /* Check if we're an enrollee and this message is valid for the previous state since some P2P registrars time out early and resend M2 */
            else if ( ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT ) &&
                      (message_type == wps_states[workspace->agent_type][workspace->current_sub_stage - 1].valid_message_type ) )
            {
                result = CY_RSLT_SUCCESS;
                goto return_with_packet_and_maybe_fragment;
            }
            else
            {
                result = CY_RSLT_WPS_ERROR_INCORRECT_MESSAGE;
                goto return_with_packet_and_maybe_fragment;
            }

            if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
            {
                ++workspace->last_received_id;
            }

            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Free unfragmented pkt @ BESL_EVENT_EAPOL_PACKET_RECEIVED\r\n");
            cy_wps_free_unfragmented_packet( workspace );
            whd_buffer_release(workspace->interface->whd_driver, message->data.packet, WHD_NETWORK_RX);
            break;

        case CY_WPS_EVENT_RECEIVED_WPS_START:
            workspace->current_sub_stage = CY_WPS_SENDING_PUBLIC_KEYS;
            break;

        case CY_EVENT_TIMER_TIMEOUT:
            return CY_RSLT_WPS_ERROR_NO_RESPONSE;
            break;

        case CY_WPS_EVENT_PBC_OVERLAP_NOTIFY_USER:
            cy_wps_pbc_overlap_array_notify( message->data.packet );
            return CY_RSLT_SUCCESS;
            break;

        case CY_EVENT_NO_EVENT:
        case CY_EVENT_ABORT_REQUESTED:
        case CY_EVENT_RECEIVED_IDENTITY_REQUEST:
        case CY_EVENT_COMPLETE:
        case CY_WPS_EVENT_DISCOVER_COMPLETE:
        case CY_WPS_EVENT_ENROLLEE_ASSOCIATED:
        case CY_WPS_EVENT_RECEIVED_IDENTITY:
        case CY_WPS_EVENT_RECEIVED_EAPOL_START:
        default:
            return CY_RSLT_WPS_UNKNOWN_EVENT;
            break;
    }

    if (workspace->current_main_stage == CY_WPS_IN_WPS_HANDSHAKE)
    {
        /* Send the packet for the current state */
        whd_host_buffer_get( workspace->interface->whd_driver, &outgoing_packet, WHD_NETWORK_TX, 1024 + WHD_LINK_HEADER, true );
        whd_buffer_add_remove_at_front( workspace->interface->whd_driver, &outgoing_packet, WHD_LINK_HEADER );
        packet_header = (cy_wps_msg_packet_t*) whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, outgoing_packet );
        memset(packet_header, 0, whd_buffer_get_current_piece_size( workspace->interface->whd_driver, outgoing_packet));
        iter = packet_header->data;
        iter = cy_wps_write_common_header( workspace, iter, wps_states[workspace->agent_type][workspace->current_sub_stage].outgoing_message_type );
        iter = wps_states[workspace->agent_type][workspace->current_sub_stage].packet_generator( workspace, iter );
        cy_wps_send_protocol_message( workspace, outgoing_packet, (uint8_t*) &packet_header->data, iter );
    }

    return CY_RSLT_SUCCESS;

return_with_packet_and_maybe_fragment:
cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Free unfragmented pkt\r\n");
    cy_wps_free_unfragmented_packet( workspace );

return_with_packet:
cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Free host pkt\r\n");
    whd_buffer_release( workspace->interface->whd_driver, message->data.packet, WHD_NETWORK_RX );
    return result;
}

static uint8_t* cy_wps_write_common_header(cy_wps_agent_t* workspace, uint8_t* start_of_packet, uint8_t message_type)
{
    uint8_t version           = WPS_VERSION;
    uint8_t temp_message_type = message_type;

    // Write the version and message type
    uint8_t* iter = start_of_packet;
    iter = tlv_write_value( iter, WPS_ID_VERSION,  WPS_ID_VERSION_S,  &version,           TLV_UINT8 );
    iter = tlv_write_value( iter, WPS_ID_MSG_TYPE, WPS_ID_MSG_TYPE_S, &temp_message_type, TLV_UINT8 );

    // Write their nonce (if we have it)
    // Since we always have our nonce and this is common code for both Registrar and Enrollee we will check both bits and ignore our agent_type
    if ( ( message_type != WPS_ID_MESSAGE_M2 ) && ( workspace->available_crypto_material & (CY_WPS_CRYPTO_MATERIAL_REGISTRAR_NONCE | CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE)) == (CY_WPS_CRYPTO_MATERIAL_REGISTRAR_NONCE | CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE) )
    {
        iter = tlv_write_value( iter, agent_specific_tlv_id[OPPOSITE_AGENT_TYPE(workspace->agent_type)][CY_WPS_NONCE_INDEX], SIZE_128_BITS, &workspace->their_data.nonce, TLV_UINT8_PTR );
    }
    return iter;
}

static void cy_wps_send_protocol_message(cy_wps_agent_t* workspace, cy_packet_t* packet, uint8_t* start_of_packet, uint8_t* iter)
{
    cy_wps_hash_t hmac_output;
    uint16_t eap_length;
    cy_wps_msg_packet_t* packet_header;
    uint16_t aligned_length;
    uint32_t aligned_vendor_type;

    // Check if we have auth key
    if ( ( workspace->available_crypto_material & CY_WPS_CRYPTO_MATERIAL_AUTH_KEY ) != 0 )
    {
        /* Calculate and add HMAC. The HMAC should have been started with the processing of the previous message */
        cy_sha2_hmac_update( &workspace->hmac, start_of_packet, (uint32_t)( iter - start_of_packet ) );
        cy_sha2_hmac_finish( &workspace->hmac, (unsigned char*) &hmac_output );
        iter = tlv_write_value( iter, WPS_ID_AUTHENTICATOR, SIZE_64_BITS, &hmac_output, TLV_UINT8_PTR );

        /* Start calculating the authenticator for the next message */
        cy_sha2_hmac_starts( &workspace->hmac, (unsigned char*) &workspace->auth_key, SIZE_256_BITS, 0 );
        cy_sha2_hmac_update( &workspace->hmac, start_of_packet, (uint32_t)( iter - start_of_packet ) );
    }
    else
    {
        /* We should only get here when the agent is an enrollee and is sending M1
         * Create a copy of the message so it can be used to verify the M2 HMAC when we receive their nonce and public key */
        if ( workspace->m1_copy != NULL )
        {
            cy_wps_free( workspace->m1_copy );
        }
        workspace->m1_copy_length = (uint16_t)( iter - start_of_packet );
        workspace->m1_copy = (uint8_t*) cy_wps_malloc( "wps m1 copy", workspace->m1_copy_length );
        memcpy( workspace->m1_copy, start_of_packet, workspace->m1_copy_length );
    }

    eap_length = (uint16_t)( (uint16_t)( iter - start_of_packet ) + sizeof(cy_eap_header_t) + sizeof(cy_eap_expanded_header_t) );

    /* Fill EAP header */
    packet_header                       = (cy_wps_msg_packet_t*)whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, packet );
    packet_header->eap.code             = (workspace->agent_type == CY_WPS_REGISTRAR_AGENT || workspace->in_reverse_registrar_mode == 1) ? CY_EAP_CODE_REQUEST : CY_EAP_CODE_RESPONSE;
    packet_header->eap.id               = workspace->last_received_id;
    CY_WPS_HOST_WRITE_16_BE(&aligned_length, eap_length);
    packet_header->eap.length = aligned_length;
    packet_header->eap.type             = CY_EAP_TYPE_WPS;
    packet_header->eap_expanded.flags   = 0;
    packet_header->eap_expanded.op_code = 4; /* WSC message */
    memcpy( packet_header->eap_expanded.vendor_id, WFA_VENDOR_EXT_ID, 3 );
    CY_WPS_HOST_WRITE_32_BE(&aligned_vendor_type, 1);
    packet_header->eap_expanded.vendor_type = aligned_vendor_type;

    /* Send EAPOL frame */
    if ( ( workspace->current_main_stage != CY_WPS_CLOSING_EAP ) && ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT ) )
    {
        /* After the enrollee sends m1 wait a little longer in case the registrar is slow */
        if ( workspace->current_sub_stage == CY_WPS_SENDING_PUBLIC_KEYS )
        {
            cy_host_start_timer( workspace->wps_host_workspace, wps_m2_timeout );
        }
        else
        {
            cy_host_start_timer( workspace->wps_host_workspace, WPS_EAPOL_PACKET_TIMEOUT );
        }
    }
    cy_wps_send_eapol_packet( packet, workspace, CY_EAP_PACKET, &workspace->their_data.mac_address, eap_length );
}

static cy_rslt_t cy_wps_calculate_kdk(cy_wps_agent_t* workspace)
{
    cy_kdk_input_t                     kdk_input;
    cy_wps_session_key_derivation_output_t kdf_output;
    cy_wps_hash_t                      kdk;
    uint8_t                            diffie_hellman_key[SIZE_256_BITS];
    cy_wps_NN_t                        wps_prime;
    cy_wps_NN_t                        nn_workspace;
    cy_wps_NN_t                        shared_secret;
    cy_wps_NN_t                        their_public_key;

    /* Generate the Diffie-Hellman shared secret */
    wps_NN_set( &wps_prime, DH_P_VALUE );
    wps_NN_set( &their_public_key, workspace->their_data.public_key.key );

    nn_workspace.len  = 48;
    shared_secret.len = 48;
    NN_ExpModMont( (NN_t*) &shared_secret, (NN_t*) &their_public_key, (NN_t*) &workspace->my_private_key, (NN_t*) &wps_prime, (NN_t*) &nn_workspace );

    /* Compute the Diffie-Hellman key based on the shared secret */
    wps_NN_get( &shared_secret, (uint8_t*) nn_workspace.num );
    cy_sha256( (uint8_t*) nn_workspace.num, 192, (unsigned char*) diffie_hellman_key, 0 );


    /* Generate the KDK */
    memcpy( &kdk_input.enrollee_nonce,  &workspace->enrollee_data->nonce,       sizeof(cy_wps_nonce_t) );
    memcpy( &kdk_input.enrollee_mac,    &workspace->enrollee_data->mac_address, sizeof(whd_mac_t) );
    memcpy( &kdk_input.registrar_nonce, &workspace->registrar_data->nonce,      sizeof(cy_wps_nonce_t) );

    cy_sha2_hmac( diffie_hellman_key, SIZE_256_BITS, (uint8_t*) &kdk_input, sizeof(cy_kdk_input_t), (uint8_t*) &kdk, 0 );

    /* Generate auth_key, key_wrap_key and emsk */
    cy_wps_calculate_kdf( (uint8_t*) &kdk, sizeof(kdk), KDK_PERSONALIZATION_STRING, (uint8_t*) &kdf_output, sizeof(cy_wps_session_keys_t) );
    memcpy( &workspace->auth_key,     &kdf_output.keys.auth_key,     sizeof(cy_auth_key_t) );
    memcpy( &workspace->key_wrap_key, &kdf_output.keys.key_wrap_key, sizeof(cy_key_wrap_key_t) );
    memcpy( &workspace->emsk,         &kdf_output.keys.emsk,         SIZE_256_BITS );

    workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_AUTH_KEY | CY_WPS_CRYPTO_MATERIAL_KEY_WRAP_KEY;

    return CY_RSLT_SUCCESS;
}

void cy_wps_init_workspace(cy_wps_agent_t* workspace)
{
    /* Get my MAC address */
    whd_wifi_get_mac_address(workspace->interface, &workspace->my_data.mac_address);

    memcpy(&workspace->uuid, WPS_TEMPLATE_UUID, sizeof(WPS_TEMPLATE_UUID));
    memcpy(&workspace->uuid.octet[sizeof(cy_wps_uuid_t) - sizeof(whd_mac_t)], &workspace->my_data.mac_address, sizeof(whd_mac_t));

    workspace->primary_device.category      = cy_hton16(workspace->device_details->device_category);
    workspace->primary_device.oui           = cy_hton32(WIFI_ALLIANCE_OUI);
    workspace->primary_device.sub_category  = cy_hton16(workspace->device_details->sub_category);
    workspace->rfBand                       = WPS_RFBAND_24GHZ;
    workspace->my_data.authTypeFlags        = workspace->device_details->authentication_type_flags;
    workspace->my_data.encrTypeFlags        = workspace->device_details->encryption_type_flags;
    workspace->connTypeFlags                = 0x01;
    workspace->scState                      = 0x01;
    workspace->my_data.supported_version    = WPS_VERSION2;
    workspace->their_data.supported_version = 0;
    workspace->osVersion                    = 1;
    workspace->fragmented_packet            = NULL;
    workspace->fragmented_packet_length     = 0;
    workspace->processing_fragmented_packet = FALSE;
    workspace->m1_copy                      = NULL;
    workspace->m1_copy_length               = 0;

    if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
    {
        cy_wps_enrollee_init( workspace );
    }
}

void cy_wps_reset_workspace( cy_wps_agent_t* workspace, whd_interface_t interface )
{
    cy_wps_cleanup_workspace( workspace );

    memset( &workspace->their_data, 0, sizeof(cy_wps_agent_data_t) );
    memset( &workspace->hmac, 0, sizeof( workspace->hmac ) );
    memset( &workspace->auth_key,     0, sizeof( workspace->auth_key ) );
    memset( &workspace->key_wrap_key, 0, sizeof( workspace->key_wrap_key ) );
    memset( &workspace->emsk,         0, sizeof( workspace->emsk ) );

    workspace->last_received_id          = 0;
    workspace->available_crypto_material = 0;
    workspace->in_reverse_registrar_mode = 0;

    if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
    {
        cy_wps_enrollee_reset( workspace, interface );
    }


    cy_wps_prepare_workspace_crypto( workspace );
}

void cy_wps_prepare_workspace_crypto( cy_wps_agent_t* workspace )
{
    cy_wps_NN_t wps_generator;
    cy_wps_NN_t wps_prime;
    cy_wps_NN_t calculation_workspace;
    cy_wps_NN_t public_key;
    size_t      output_length = 0;

    /* Public nonce generation */
    cy_host_random_bytes( (uint8_t*) &workspace->my_data.nonce, SIZE_128_BITS, &output_length );

    /* Public-Private key generation */
    workspace->my_private_key.len = PRIVATE_KEY_NN_LENGTH;
    cy_host_random_bytes( (uint8_t*) workspace->my_private_key.num, PRIVATE_KEY_BYTE_LENGTH, &output_length );

    // Create public key
    wps_generator.len = 48;
    memset( wps_generator.num, 0, 192 );
    wps_generator.num[47] = DH_G_VALUE;

    wps_NN_set( &wps_prime, DH_P_VALUE );

    calculation_workspace.len = 48;

    public_key.len = 48;
    NN_ExpModMont( (NN_t*) &public_key, (NN_t*) &wps_generator, (NN_t*) &workspace->my_private_key, (NN_t*) &wps_prime, (NN_t*) &calculation_workspace );
    wps_NN_get( &public_key, workspace->my_data.public_key.key );

    /* Secret nonce generation */
    cy_host_random_bytes( (uint8_t*) workspace->my_data.secret_nonce[0].nonce, SIZE_128_BITS, &output_length );
    cy_host_random_bytes( (uint8_t*) workspace->my_data.secret_nonce[1].nonce, SIZE_128_BITS, &output_length );
}

static cy_rslt_t cy_wps_calculate_hash(cy_wps_agent_t* workspace, cy_wps_agent_data_t* source, cy_wps_hash_t* output, uint8_t hash)
{
    cy_wps_hash_input_t hash_input;

    wps_assert("Bad args", (hash == 0) || (hash == 1));

    memcpy( &hash_input.secret,               &source->secret_nonce[hash],            sizeof(cy_wps_nonce_t) );
    memcpy( &hash_input.psk,                  &workspace->psk[hash],                  sizeof(cy_wps_psk_t) );
    memcpy( &hash_input.enrollee_public_key,  &workspace->enrollee_data->public_key,  sizeof(cy_public_key_t) );
    memcpy( &hash_input.registrar_public_key, &workspace->registrar_data->public_key, sizeof(cy_public_key_t) );
    cy_sha2_hmac( (unsigned char *)&workspace->auth_key, SIZE_256_BITS, (uint8_t*) &hash_input, sizeof( hash_input ), output->octet, 0 );

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_wps_calculate_psk(cy_wps_agent_t* workspace)
{
    cy_wps_hash_t hmac_output;
    uint16_t   password_length = (uint16_t) strlen( workspace->password );

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS password %s\r\n", workspace->password);

    /* Hash 1st half of password and copy first 128 bits into psk1. If it is an odd length, the extra byte goes along with the first half */
    cy_sha2_hmac( (unsigned char *)&workspace->auth_key, SIZE_256_BITS, (uint8_t*) workspace->password, (uint32_t) ( ( password_length / 2 ) + ( password_length % 2 ) ), (uint8_t*) &hmac_output, 0 );
    memcpy( &workspace->psk[0], &hmac_output, SIZE_128_BITS );

    /* Hash 2nd half of password and copy fist 128 bits into psk2 */
    cy_sha2_hmac( (unsigned char *)&workspace->auth_key, SIZE_256_BITS, (uint8_t*) ( workspace->password + ( password_length / 2 ) + ( password_length % 2 ) ), password_length / 2, (uint8_t*) &hmac_output, 0 );
    memcpy( &workspace->psk[1], &hmac_output, SIZE_128_BITS );

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_wps_process_message_content( cy_wps_agent_t* workspace, uint8_t* content, uint16_t content_length, uint32_t tlv_mask )
{
    cy_wps_hash_t         hmac_output;
    tlv16_data_t*         tlv;
    uint16_t              encrypted_data_length = 0;
    cy_rslt_t             result = CY_RSLT_SUCCESS;
    uint32_t              parsed_tlvs    = 0;
    uint32_t              valid_tlv_mask = wps_states[workspace->agent_type][workspace->current_sub_stage].tlv_mask;
    cy_wps_encryption_data_t* encrypted_data = NULL;
    uint8_t*              authenticator  = NULL;

    /* Note that the P2P registrar can't do the overlap check for p2p clients because they can change their MAC address between sending the probe request and sending M1 */
    if ( ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT) && ( workspace->wps_mode == CY_WPS_PBC_MODE ) && ( workspace->is_p2p_registrar == 0 ) &&
         ( cy_wps_pbc_overlap_check( &workspace->their_data.mac_address ) == CY_RSLT_WPS_PBC_OVERLAP ) )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "PBC overlap during message processing\r\n");
        workspace->wps_result = CY_RSLT_WPS_PBC_OVERLAP;
        return CY_RSLT_WPS_PBC_OVERLAP;
    }

    /* Process TLVs contained in message */
    for ( tlv = (tlv16_data_t*) content; ( (uint8_t*) tlv - content ) < content_length; tlv = (tlv16_data_t*) ( (uint8_t*) tlv + sizeof(tlv16_header_t) + cy_hton16(tlv->length) ) )
    {
#if 1
        tlv16_header_t temp_tlv;
        tlv16_header_t* aligned_tlv = &temp_tlv;
        memcpy(aligned_tlv, tlv, sizeof(tlv16_header_t));
#else
#define aligned_tlv     tlv
#endif
        switch ( cy_hton16(aligned_tlv->type) )
        {
            case WPS_ID_VERSION:
                parsed_tlvs |= CY_WPS_TLV_VERSION;
                if ( ( tlv->data[0] & 0xF0 ) != WPS_VERSION )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS version mismatch\r\n");
                    return CY_RSLT_WPS_ERROR_VERSION_MISMATCH;
                }
                break;

            case WPS_ID_ENROLLEE_NONCE:
                parsed_tlvs |= CY_WPS_TLV_ENROLLEE_NONCE;
                workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE;
                if ( (valid_tlv_mask & CY_WPS_TLV_ENROLLEE_NONCE) != 0 )
                {
                    if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
                    {
                        if ( memcmp( &workspace->enrollee_data->nonce, tlv->data, sizeof(cy_wps_nonce_t) ) != 0 )
                        {
                            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS enrollee nonce mismatch\r\n");
                            return CY_RSLT_WPS_ERROR_ENROLLEE_NONCE_MISMATCH;
                        }
                    }
                    else if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
                    {
                        memcpy( &workspace->enrollee_data->nonce, tlv->data, sizeof(cy_wps_nonce_t) );
                    }
                }
                break;

            case WPS_ID_REGISTRAR_NONCE:
                parsed_tlvs |= CY_WPS_TLV_REGISTRAR_NONCE;
                workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_REGISTRAR_NONCE;
                if (( valid_tlv_mask & CY_WPS_TLV_REGISTRAR_NONCE ) != 0 )
                {
                    if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
                    {
                        if ( memcmp( &workspace->registrar_data->nonce, tlv->data, sizeof(cy_wps_nonce_t) ) != 0 )
                        {
                            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS registrar nonce mismatch\r\n");
                            return CY_RSLT_WPS_ERROR_REGISTRAR_NONCE_MISMATCH;
                        }
                    }
                    else if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
                    {
                        memcpy( &workspace->registrar_data->nonce, tlv->data, sizeof(cy_wps_nonce_t) );
                    }
                }
                break;

            case WPS_ID_E_HASH1:
                parsed_tlvs |= CY_WPS_TLV_E_HASH1;
                workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_ENROLLEE_HASH1;
                if (( valid_tlv_mask & CY_WPS_TLV_E_HASH1 ) != 0 )
                {
                    memcpy( &workspace->enrollee_data->secret_hash[0], tlv->data, sizeof(cy_wps_hash_t) );
                }
                break;

            case WPS_ID_E_HASH2:
                parsed_tlvs |= CY_WPS_TLV_E_HASH2;
                workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_ENROLLEE_HASH2;
                if ( (valid_tlv_mask & CY_WPS_TLV_E_HASH2) != 0 )
                {
                    memcpy( &workspace->enrollee_data->secret_hash[1], tlv->data, sizeof(cy_wps_hash_t) );
                }
                break;

            case WPS_ID_R_HASH1:
                parsed_tlvs |= CY_WPS_TLV_R_HASH1;
                workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_REGISTRAR_HASH1;
                if ( (valid_tlv_mask & CY_WPS_TLV_R_HASH1) != 0 )
                {
                    memcpy( &workspace->registrar_data->secret_hash[0], tlv->data, sizeof(cy_wps_hash_t) );
                }
                break;

            case WPS_ID_R_HASH2:
                parsed_tlvs |= CY_WPS_TLV_R_HASH2;
                workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_REGISTRAR_HASH2;
                if ( (valid_tlv_mask & CY_WPS_TLV_R_HASH2 )!= 0 )
                {
                    memcpy( &workspace->registrar_data->secret_hash[1], tlv->data, sizeof(cy_wps_hash_t) );
                }
                break;

            case WPS_ID_ENCR_SETTINGS:
                parsed_tlvs |= CY_WPS_TLV_ENCRYPTION_SETTINGS;
                if ( cy_hton16(aligned_tlv->length) >= sizeof(cy_wps_iv_t) + WPS_ENCRYPTION_BLOCK_SIZE )
                {
                    encrypted_data        = (cy_wps_encryption_data_t*) tlv->data;
                    encrypted_data_length = (uint16_t) ( cy_hton16(aligned_tlv->length) - sizeof(cy_wps_iv_t) );
                }
                break;

            case WPS_ID_AUTHENTICATOR:
                parsed_tlvs |= CY_WPS_TLV_AUTHENTICATOR;
                authenticator = tlv->data;
                break;

            case WPS_ID_AUTH_TYPE_FLAGS:
                parsed_tlvs |= CY_WPS_TLV_AUTH_TYPE_FLAGS;
                workspace->their_data.authTypeFlags = CY_WPS_HOST_READ_16_BE(tlv->data);
                if ( ( workspace->their_data.authTypeFlags & workspace->my_data.authTypeFlags ) == 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS authentication type not supported\r\n");
                    return CY_RSLT_WPS_ERROR_AUTHENTICATION_TYPE_ERROR;
                }
                break;

            case WPS_ID_ENCR_TYPE_FLAGS:
                parsed_tlvs |= CY_WPS_TLV_ENCR_TYPE_FLAGS;
                workspace->their_data.encrTypeFlags = CY_WPS_HOST_READ_16_BE(tlv->data);
                if ( ( workspace->their_data.encrTypeFlags & workspace->my_data.encrTypeFlags ) == 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS encryption type not supported\r\n");
                    return CY_RSLT_WPS_ERROR_ENCRYPTION_TYPE_ERROR;
                }
                break;

            case WPS_ID_MAC_ADDR:
                if (workspace->agent_type == CY_WPS_REGISTRAR_AGENT)
                {
                    parsed_tlvs |= CY_WPS_TLV_MAC_ADDR;
                    workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_ENROLLEE_MAC_ADDRESS;
                    memcpy(&workspace->enrollee_data->mac_address, tlv->data, sizeof(whd_mac_t));
                }
                break;

            case WPS_ID_UUID_R:
                parsed_tlvs |= CY_WPS_TLV_UUID_R;
                break;

            case WPS_ID_PUBLIC_KEY:
                parsed_tlvs |= CY_WPS_TLV_PUBLIC_KEY;
                workspace->available_crypto_material |= (workspace->agent_type == CY_WPS_ENROLLEE_AGENT) ? CY_WPS_CRYPTO_MATERIAL_ENROLLEE_PUBLIC_KEY : CY_WPS_CRYPTO_MATERIAL_REGISTRAR_PUBLIC_KEY;
                memcpy( &workspace->their_data.public_key, tlv->data, sizeof(cy_public_key_t) );
                break;

            case WPS_ID_MSG_TYPE:
                parsed_tlvs |= CY_WPS_TLV_MSG_TYPE;
                if (*(tlv->data) == WPS_MESSAGE_TYPE_M1)
                {
                    // Catchall for Enrollees that don't include the WPS element in their Probe Request, add their MAC address to the overlap array
                    if ( ( workspace->wps_mode == CY_WPS_PBC_MODE) && ( workspace->is_p2p_registrar == 0 ) && ( workspace->is_p2p_enrollee == 0 ) )
                    {
                        cy_wps_update_pbc_overlap_array( workspace, &workspace->their_data.mac_address );
                    }
                }
                break;

            case WPS_ID_X509_CERT:
                /* XXX: Add X509 certificate support */
                break;

            case WPS_ID_VENDOR_EXT:
                if ( memcmp( tlv->data, WFA_VENDOR_EXT_ID, 3 ) == 0 )
                {
                    tlv8_uint8_t* tlv8 = (tlv8_uint8_t*) tlv_find_tlv8( &tlv->data[3], (uint32_t)( aligned_tlv->length - 3 ), WPS_WFA_SUBID_VERSION2 );
                    if ((tlv8 != NULL) && (workspace->their_data.supported_version == 0 ))
                    {
                        workspace->their_data.supported_version = tlv8->data;
                    }
                }
                break;
        }
    }

    if ( parsed_tlvs != tlv_mask )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Mismatched TLVs. %lu != %lu\r\n", parsed_tlvs, tlv_mask);
        return CY_RSLT_WPS_ERROR_MESSAGE_TLV_MASK_MISMATCH;
    }

    /* Check if we don't have the auth key, but do have the required inputs */
    if (((workspace->available_crypto_material & CY_WPS_CRYPTO_MATERIAL_AUTH_KEY) == 0) &&
        (workspace->available_crypto_material & KDK_REQUIRED_CRYPTO_MATERIAL) == KDK_REQUIRED_CRYPTO_MATERIAL)
    {
        cy_wps_calculate_kdk(workspace);
        cy_wps_calculate_psk(workspace);

        /* Should now be able to calculate my hashes as well */
        cy_wps_calculate_hash(workspace, &workspace->my_data, &workspace->my_data.secret_hash[0], 0);
        cy_wps_calculate_hash(workspace, &workspace->my_data, &workspace->my_data.secret_hash[1], 1);

        workspace->available_crypto_material |= (workspace->agent_type == CY_WPS_ENROLLEE_AGENT) ?
                                                CY_WPS_CRYPTO_MATERIAL_ENROLLEE_HASH1 | CY_WPS_CRYPTO_MATERIAL_ENROLLEE_HASH2 :
                                                CY_WPS_CRYPTO_MATERIAL_REGISTRAR_HASH1 | CY_WPS_CRYPTO_MATERIAL_REGISTRAR_HASH2;
        /* Prepare the hmac for use */
        cy_sha2_hmac_starts( &workspace->hmac, (unsigned char*) &workspace->auth_key, SIZE_256_BITS, 0 );
    }

    /* Check if we didn't receive an authenticator. Only valid when processing M1 (Registrar only) */
    if ( authenticator == NULL )
    {
        cy_sha2_hmac_update( &workspace->hmac, content, content_length );
    }
    /* HMAC validation (if we have the auth key ) */
    else if ( (workspace->available_crypto_material & CY_WPS_CRYPTO_MATERIAL_AUTH_KEY) != 0 )
    {
        /* Check if we need to take into account the copy of M1. Only valid when processing M2 (Enrollee only) */
        if (workspace->m1_copy != NULL)
        {
            cy_sha2_hmac_update( &workspace->hmac, workspace->m1_copy, workspace->m1_copy_length );
            cy_wps_free(workspace->m1_copy);
            workspace->m1_copy        = NULL;
            workspace->m1_copy_length = 0;
        }
        cy_sha2_hmac_update( &workspace->hmac, content, content_length - sizeof(tlv16_header_t) - WPS_AUTHENTICATOR_LEN );
        cy_sha2_hmac_finish( &workspace->hmac, (unsigned char*) &hmac_output );
        if ( memcmp( &hmac_output, authenticator, WPS_AUTHENTICATOR_LEN ) != 0 )
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Message HMAC error\r\n");
            return CY_RSLT_WPS_ERROR_MESSAGE_HMAC_FAIL;
        }
        else
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Message HMAC correct\r\n");
        }

        /* Start the HMAC calculation for next message */
        cy_sha2_hmac_starts  ( &workspace->hmac, (unsigned char*) &workspace->auth_key, SIZE_256_BITS, 0 );
        cy_sha2_hmac_update( &workspace->hmac, content, content_length );
    }

    /* If we have encrypted data, decrypt and process TLVs contained within */
    if ( encrypted_data != NULL )
    {
        result = cy_wps_process_encrypted_tlvs( workspace, encrypted_data, encrypted_data_length, wps_states[workspace->agent_type][workspace->current_sub_stage].encrypted_tlv_mask );
    }

    return result;
}

static cy_rslt_t cy_wps_process_encrypted_tlvs( cy_wps_agent_t* workspace, cy_wps_encryption_data_t* encrypted_data, uint16_t encrypted_data_length, uint32_t valid_tlv_mask )
{
    tlv16_data_t* tlv;
    cy_wps_hash_t hmac_output;
    uint32_t      parsed_tlvs = 0;
    cy_rslt_t     result = CY_RSLT_SUCCESS;
    int32_t       valid_credential_records = -1;
    int32_t       invalid_wep_records      = 0;
    uint8_t*      plain_text = encrypted_data->iv;
    int           plain_text_length;
    tlv16_data_t* key_wrap_auth = NULL;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Processing encrypted TLV\r\n");

    /* Decrypt the encrypted data */
    plain_text_length = cy_wps_decrypt_data( encrypted_data, encrypted_data_length, &workspace->key_wrap_key, plain_text );

    /* Find the key wrap auth element */
    if ( plain_text_length > 0 )
    {
        key_wrap_auth = tlv_find_tlv16(plain_text, (uint32_t) plain_text_length, WPS_ID_KEY_WRAP_AUTH);
    }

    if ( key_wrap_auth == NULL )
    {
        return CY_RSLT_WPS_ERROR_MESSAGE_MISSING_TLV;
    }

    /* Calculate the HMAC of the data (data only, not the last auth TLV) and compare it against the received HMAC */
    cy_sha2_hmac( (unsigned char *)&workspace->auth_key, SIZE_256_BITS, plain_text, (uint32_t) plain_text_length - sizeof(tlv16_header_t) - SIZE_64_BITS, (uint8_t*) &hmac_output, 0 );
    if ( memcmp( &hmac_output, key_wrap_auth->data, SIZE_64_BITS ) != 0 )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS: HMAC error\r\n");
        return CY_RSLT_WPS_ERROR_ENCRYPTED_TLV_HMAC_FAIL;
    }

    /* Process all the remaining TLV elements */
    for ( tlv = (tlv16_data_t*) plain_text; ( (uint8_t*) tlv - plain_text ) < plain_text_length; tlv = (tlv16_data_t*) ( (uint8_t*) tlv + sizeof(tlv16_header_t) + cy_hton16( tlv->length ) ) )
    {
        tlv16_header_t  temp_tlv;
        tlv16_header_t* aligned_tlv = &temp_tlv;

        if (result != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS: HMAC error\r\n");
            return result;
        }

        memcpy(aligned_tlv, tlv, sizeof(tlv16_header_t));

        switch ( cy_hton16( aligned_tlv->type ) )
        {
            case WPS_ID_R_SNONCE1:
                parsed_tlvs |= CY_WPS_TLV_R_SNONCE1;
                if ( (valid_tlv_mask & CY_WPS_TLV_R_SNONCE1) != 0 )
                {
                    /* Validate the secret nonce */
                    result = cy_wps_validate_secret_nonce(workspace, workspace->registrar_data, tlv->data, 0);
                }
                break;

            case WPS_ID_R_SNONCE2:
                parsed_tlvs |= CY_WPS_TLV_R_SNONCE2;
                if ( ( valid_tlv_mask & CY_WPS_TLV_R_SNONCE2 ) != 0 )
                {
                    /* Validate the secret nonce */
                    result = cy_wps_validate_secret_nonce(workspace, workspace->registrar_data, tlv->data, 1);
                }
                break;

            case WPS_ID_E_SNONCE1:
                parsed_tlvs |= CY_WPS_TLV_E_SNONCE1;
                if ( (valid_tlv_mask & CY_WPS_TLV_E_SNONCE1) != 0 )
                {
                    /* Validate the secret nonce */
                    result = cy_wps_validate_secret_nonce(workspace, workspace->enrollee_data, tlv->data, 0);
                }
                break;

            case WPS_ID_E_SNONCE2:
                parsed_tlvs |= CY_WPS_TLV_E_SNONCE2;
                if (( valid_tlv_mask & CY_WPS_TLV_E_SNONCE2) != 0 )
                {
                    /* Validate the secret nonce */
                    result = cy_wps_validate_secret_nonce(workspace, workspace->enrollee_data, tlv->data, 1);
                }
                break;

            case WPS_ID_CREDENTIAL:
                parsed_tlvs |= CY_WPS_TLV_CREDENTIAL;
                /* Note that we have seen a credential TLV */
                if ( valid_credential_records == -1 )
                {
                    valid_credential_records = 0;
                }
                result = cy_wps_process_credential( workspace, tlv->data, cy_hton16(aligned_tlv->length) );
                if ( result == CY_RSLT_SUCCESS )
                {
                    ++valid_credential_records;
                }
                else if ( result == CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS )
                {
                    ++invalid_wep_records;
                    result = CY_RSLT_SUCCESS;
                }
                break;
        }
    }

    if ( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS: error processing encrypted TLVs\r\n");
        return result;
    }

    /* Check if we received a credentials TLV */
    if ( result == CY_RSLT_SUCCESS && valid_credential_records != -1 )
    {
        cy_host_start_timer( workspace->wps_host_workspace, 500 );

        /* Handshake is finished so allow the enrollee the deal with the timeout */
        workspace->current_main_stage = CY_WPS_CLOSING_EAP;
        workspace->current_sub_stage  = 0;

        /* Check if any were valid */
        if ( valid_credential_records > 0 )
        {
            // Send back Done
            cy_wps_send_done( workspace );
        }
        else /* Otherwise we received invalid credentials (e.g. WEP security) */
        {
            if ( invalid_wep_records != 0 )
            {
                result = CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS;
            }
            else
            {
                result = CY_RSLT_WPS_ERROR_RECEIVED_INVALID_CREDENTIALS;
            }
            /* Send back NACK, but let the state machine continue to the end */
            cy_wps_send_wsc_nack(workspace, 0);
        }
    }

    return result;
}

static cy_rslt_t cy_wps_process_credential( cy_wps_agent_t* workspace, uint8_t* data, uint16_t data_length )
{
    cy_wps_internal_credential_t credential;
    tlv16_data_t*    tlv     = (tlv16_data_t*) data;
    uint8_t*         tlv_end = data + data_length;
#if 1
    tlv16_header_t  temp_tlv;
    tlv16_header_t* aligned_tlv = &temp_tlv;
#else
#define aligned_tlv      tlv
#endif

    memset(&credential, 0, sizeof(credential));

    for ( ; (uint8_t*) tlv < tlv_end; tlv = (tlv16_data_t*) ( tlv->data + cy_hton16( aligned_tlv->length ) ) )
    {

        memcpy(aligned_tlv, tlv, sizeof(tlv16_header_t));

        switch ( cy_hton16( aligned_tlv->type ) )
        {
            case WPS_ID_SSID:
                credential.ssid_length = (uint8_t) cy_hton16( aligned_tlv->length );
                memcpy( credential.ssid, tlv->data, credential.ssid_length );
                break;

            case WPS_ID_AUTH_TYPE:
                credential.authentication_type = CY_WPS_HOST_READ_16_BE(tlv->data);
                break;

            case WPS_ID_ENCR_TYPE:
                credential.encryption_type = CY_WPS_HOST_READ_16_BE(tlv->data);
                break;

            case WPS_ID_NW_KEY:
                credential.network_key_length = (uint8_t) cy_hton16( aligned_tlv->length );
                memcpy( credential.network_key, tlv->data, credential.network_key_length );
                break;

            case WPS_ID_KEY_WRAP_AUTH:
                break;

            case WPS_ID_MAC_ADDR:
                if ( workspace->my_data.supported_version >= WPS_VERSION2 && workspace->their_data.supported_version >= WPS_VERSION2 )
                {
                    if ( memcmp( tlv->data, &workspace->my_data.mac_address, sizeof(whd_mac_t) ) != 0 )
                    {
                        /* MAC Address verification failed */

                    }
                }
                break;
        }
    }

    if ( credential.encryption_type != CY_WPS_NO_UNDEFINED && credential.encryption_type != CY_WPS_WEP_ENCRYPTION )
    {
        cy_wps_host_store_credential( workspace->wps_host_workspace, &credential );
        return CY_RSLT_SUCCESS;
    }
    else
    {
        return CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS;
    }
}


/*
 * Note: It is assumed that output = wps_hash_t[n] where sizeof(output) >= output_length;
 */
static cy_rslt_t cy_wps_calculate_kdf(uint8_t* key, uint16_t key_length, char* personalization_string, uint8_t* output, uint16_t output_length)
{
    uint32_t i;
    uint32_t iterations;
    uint32_t temp;
    uint16_t string_length;
    uint8_t input[4 + MAX_PERSONALIZATION_STRING_SIZE + 4];
    cy_wps_hash_t* hmac_output = (cy_wps_hash_t*) output;

    iterations = output_length / PRF_DIGEST_SIZE;

    /* Prepare the input buffer. During the iterations, we need only replace the value of i at the start of the buffer. */
    string_length = (uint16_t) strlen( personalization_string );
    cy_wps_assert("WPS: personalization_string too long\r\n", string_length <= MAX_PERSONALIZATION_STRING_SIZE);
    memcpy( &input[4], personalization_string, string_length );
    temp = cy_hton32( (uint32_t)( output_length * 8 ) );
    memcpy( &input[4 + string_length], &temp, 4 );

    for ( i = 0; i < iterations; i++ )
    {
        /* Set the current value of i at the start of the input buffer */
        temp = cy_hton32( i + 1 ); /* i should start at 1 */
        memcpy( &input[0], &temp, 4 );
        cy_sha2_hmac( key, key_length, input, (uint32_t)( 4 + string_length + 4 ), (uint8_t*) &hmac_output[i], 0 );
    }

    return CY_RSLT_SUCCESS;
}


static void cy_wps_encrypt_data(uint8_t* input, uint16_t input_length, cy_key_wrap_key_t* encr_key, uint8_t* output, cy_wps_iv_t* iv)
{
    size_t output_length;

    /* Generate a random iv */
    cy_host_random_bytes( iv->octet, SIZE_128_BITS, &output_length );

    cy_aes_cbc_encrypt( input, input_length, output, encr_key->octet, (SIZE_128_BITS * 8), iv->octet );
}

/* Note: plain_text can point to the encryption_data->data */
static int cy_wps_decrypt_data(cy_wps_encryption_data_t* encrypted_data, uint16_t encrypted_data_length, cy_key_wrap_key_t* encr_key, uint8_t* plain_text)
{
    uint32_t output_length;
    cy_aes_cbc_decrypt( encrypted_data->data, encrypted_data_length, plain_text, &output_length, encr_key->octet, (SIZE_128_BITS * 8), encrypted_data->iv );

    return output_length;
}


/*
 * Function to create M1 and M2
 */
uint8_t* cy_wps_write_nonce(cy_wps_agent_t* workspace, uint8_t* iter)
{
    uint16_t config_error       = 0;
    if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
    {
        iter = tlv_write_value( iter, agent_specific_tlv_id[OPPOSITE_AGENT_TYPE(workspace->agent_type)][CY_WPS_NONCE_INDEX], SIZE_128_BITS, &workspace->their_data.nonce, TLV_UINT8_PTR );
        iter = tlv_write_value( iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_NONCE_INDEX], sizeof(cy_wps_nonce_t), &workspace->my_data.nonce, TLV_UINT8_PTR );
        iter = tlv_write_value( iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_UUID_INDEX],  WPS_ID_UUID_S,       &workspace->uuid,          TLV_UINT8_PTR );
    }
    if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
    {
        iter = tlv_write_value( iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_UUID_INDEX],  WPS_ID_UUID_S,       &workspace->uuid,          TLV_UINT8_PTR );
        iter = tlv_write_value( iter, WPS_ID_MAC_ADDR,        WPS_ID_MAC_ADDR_S,        &workspace->my_data.mac_address, TLV_UINT8_PTR );
        iter = tlv_write_value( iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_NONCE_INDEX], sizeof(cy_wps_nonce_t), &workspace->my_data.nonce, TLV_UINT8_PTR );
    }
    iter = tlv_write_value( iter, WPS_ID_PUBLIC_KEY,      sizeof(cy_public_key_t),     &workspace->my_data.public_key, TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_AUTH_TYPE_FLAGS, WPS_ID_AUTH_TYPE_FLAGS_S, &workspace->my_data.authTypeFlags,      TLV_UINT16 );
    iter = tlv_write_value( iter, WPS_ID_ENCR_TYPE_FLAGS, WPS_ID_ENCR_TYPE_FLAGS_S, &workspace->my_data.encrTypeFlags,      TLV_UINT16 );
    iter = tlv_write_value( iter, WPS_ID_CONN_TYPE_FLAGS, WPS_ID_CONN_TYPE_FLAGS_S, &workspace->connTypeFlags,      TLV_UINT8 );
    iter = tlv_write_value( iter, WPS_ID_CONFIG_METHODS,  WPS_ID_CONFIG_METHODS_S,  &workspace->device_details->config_methods,  TLV_UINT16 );
    if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
    {
        iter = tlv_write_value( iter, WPS_ID_SC_STATE,        WPS_ID_SC_STATE_S,        &workspace->scState,             TLV_UINT8 );
    }
    iter = tlv_write_value( iter, WPS_ID_MANUFACTURER,    (uint16_t) strlen( workspace->device_details->manufacturer ), workspace->device_details->manufacturer,  TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_MODEL_NAME,      (uint16_t) strlen( workspace->device_details->model_name   ), workspace->device_details->model_name,    TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_MODEL_NUMBER,    (uint16_t) strlen( workspace->device_details->model_number ), workspace->device_details->model_number,  TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_SERIAL_NUM,      (uint16_t) strlen( workspace->device_details->serial_number), workspace->device_details->serial_number, TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_PRIM_DEV_TYPE,   WPS_ID_PRIM_DEV_TYPE_S,                                       &workspace->primary_device,               TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_DEVICE_NAME,     (uint16_t) strlen( workspace->device_details->device_name ),  workspace->device_details->device_name,   TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_RF_BAND,         WPS_ID_RF_BAND_S,                                             &workspace->rfBand,                       TLV_UINT8     );
    iter = tlv_write_value( iter, WPS_ID_ASSOC_STATE,     WPS_ID_ASSOC_STATE_S,                                         &workspace->association_state,            TLV_UINT16    );
    if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
    {
        iter = tlv_write_value( iter, WPS_ID_DEVICE_PWD_ID,   WPS_ID_DEVICE_PWD_ID_S,  &workspace->device_password_id, TLV_UINT16 );
        iter = tlv_write_value( iter, WPS_ID_CONFIG_ERROR,    WPS_ID_CONFIG_ERROR_S,   &config_error,                  TLV_UINT16 );
    }
    if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
    {
        iter = tlv_write_value( iter, WPS_ID_CONFIG_ERROR,    WPS_ID_CONFIG_ERROR_S,   &config_error,                  TLV_UINT16 );
        iter = tlv_write_value( iter, WPS_ID_DEVICE_PWD_ID,   WPS_ID_DEVICE_PWD_ID_S,  &workspace->device_password_id, TLV_UINT16 );
    }
    iter = tlv_write_value( iter, WPS_ID_OS_VERSION,      WPS_ID_OS_VERSION_S,     &workspace->device_details->os_version, TLV_UINT32 );

    /* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        /* Add reqToEnroll subelement to vendorExt_bufObj */
        if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
        {
            cy_wps_m1_vendor_ext_t vendor_ext;

            memcpy( vendor_ext.vendor_id, WFA_VENDOR_EXT_ID, 3 );
            vendor_ext.subid_version2.type      = WPS_WFA_SUBID_VERSION2;
            vendor_ext.subid_version2.length    = 1;
            vendor_ext.subid_version2.data      = workspace->my_data.supported_version;
            vendor_ext.request_to_enroll.type   = WPS_WFA_SUBID_REQ_TO_ENROLL;
            vendor_ext.request_to_enroll.length = 1;
            vendor_ext.request_to_enroll.data   = 1;
            iter = tlv_write_value( iter, WPS_ID_VENDOR_EXT, sizeof(cy_wps_m1_vendor_ext_t), &vendor_ext, TLV_UINT8_PTR );
        }
        else
        {
            iter = cy_wps_write_vendor_extension( iter, workspace );
        }
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Sending nonce\r\n");
    return iter;
}

/*
 * Function to create M3 and M4
 */
uint8_t* cy_wps_write_hashes(cy_wps_agent_t* workspace, uint8_t* iter)
{
    // Write my hash1 and hash2
    iter = tlv_write_value( iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_HASH1_INDEX], sizeof(cy_wps_hash_t), &workspace->my_data.secret_hash[0], TLV_UINT8_PTR );
    iter = tlv_write_value( iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_HASH2_INDEX], sizeof(cy_wps_hash_t), &workspace->my_data.secret_hash[1], TLV_UINT8_PTR );

    // If I am a registrar then I need to send my encrypted secret nonce 1 too
    if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
    {
        iter = cy_wps_write_secret1( workspace, iter );
    }
    else
    /* WSC 2.0, add WFA vendor id and sub-elements to vendor extension attribute  */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        iter = cy_wps_write_vendor_extension( iter, workspace );
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Sending hashes\r\n");
    return iter;
}

/*
 * Function to create M5 and M6
 */
uint8_t* cy_wps_write_secret1(cy_wps_agent_t* workspace, uint8_t* iter)
{
    uint8_t* temp_iter;

    // Write my secret 1
    temp_iter = cy_wps_start_encrypted_tlv(iter);
    temp_iter = tlv_write_value( temp_iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_SNONCE1_INDEX], sizeof(cy_wps_nonce_t), &workspace->my_data.secret_nonce[0], TLV_UINT8_PTR );
    iter      = cy_wps_end_encrypted_tlv(workspace, iter, temp_iter );

    /* WSC 2.0, add WFA vendor id and sub-elements to vendor extension attribute  */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        iter = cy_wps_write_vendor_extension( iter, workspace );
    }

    return iter;
}

/*
 * Function to create M7
 */
uint8_t* cy_wps_write_secret2(cy_wps_agent_t* workspace, uint8_t* iter)
{
    uint8_t* temp_iter;

    // Write my secret 2
    temp_iter = cy_wps_start_encrypted_tlv(iter);
    temp_iter = tlv_write_value( temp_iter, agent_specific_tlv_id[workspace->agent_type][CY_WPS_SNONCE2_INDEX], sizeof(cy_wps_nonce_t), &workspace->my_data.secret_nonce[1], TLV_UINT8_PTR );
    iter      = cy_wps_end_encrypted_tlv(workspace, iter, temp_iter );

    /* WSC 2.0, add WFA vendor id and sub-elements to vendor extension attribute  */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        iter = cy_wps_write_vendor_extension( iter, workspace );
    }

    return iter;
}

#ifdef WCM_ENABLE_WPS_REGISTRAR
/*
 * Function to create M8
 */
static uint8_t* cy_wps_write_credentials(cy_wps_agent_t* workspace, uint8_t* iter)
{
    uint8_t* encrypt_iter;
    uint8_t* credential_iter;
    uint16_t network_index = 1;
    cy_wps_internal_credential_t credential;
    uint16_t authentication_type;
    uint16_t encryption_type;
    tlv16_data_t* credential_header;
    memset(&credential, 0, sizeof(cy_wps_internal_credential_t));
    cy_wps_host_retrieve_credential(workspace->wps_host_workspace, &credential);

    // Not all WPS v1.0 enrollees understand mixed modes of authentication or encryption so minimize the choice
    if ( workspace->their_data.supported_version < WPS_VERSION2 )
    {
        authentication_type = cy_hton16( credential.authentication_type & workspace->their_data.authTypeFlags );
        encryption_type     = cy_hton16( credential.encryption_type & workspace->their_data.encrTypeFlags );
        if ( encryption_type & (uint16_t)cy_hton16(CY_WPS_AES_ENCRYPTION ) )
        {
            encryption_type = (uint16_t)cy_hton16(CY_WPS_AES_ENCRYPTION);
        }
    }
    else
    {
        authentication_type = cy_hton16( credential.authentication_type );
        encryption_type     = cy_hton16( credential.encryption_type  );
    }

    /* Write credentials */
    encrypt_iter    = cy_wps_start_encrypted_tlv( iter );
    credential_iter = tlv_write_header( encrypt_iter, WPS_ID_CREDENTIAL,  0 );
    credential_iter = tlv_write_value( credential_iter, WPS_ID_NW_INDEX,  1,                              &network_index,                  TLV_UINT8 );
    credential_iter = tlv_write_value( credential_iter, WPS_ID_SSID,      credential.ssid_length,         credential.ssid,                 TLV_UINT8_PTR );
    credential_iter = tlv_write_value( credential_iter, WPS_ID_AUTH_TYPE, WPS_ID_AUTH_TYPE_S,             &authentication_type,            TLV_UINT8_PTR );
    credential_iter = tlv_write_value( credential_iter, WPS_ID_ENCR_TYPE, WPS_ID_ENCR_TYPE_S,             &encryption_type,                TLV_UINT8_PTR );
    credential_iter = tlv_write_value( credential_iter, WPS_ID_NW_KEY,    credential.network_key_length,  credential.network_key,          TLV_UINT8_PTR );
    credential_iter = tlv_write_value( credential_iter, WPS_ID_MAC_ADDR,  sizeof(whd_mac_t),             &workspace->their_data.mac_address, TLV_UINT8_PTR );

    /* Finish off the credential TLV */
    credential_header = (tlv16_data_t*)encrypt_iter;
    credential_header->length       = cy_hton16( (uint16_t)( credential_iter - credential_header->data ) );

    /* Finish off the encrypted TLV */
    iter = cy_wps_end_encrypted_tlv( workspace, iter, credential_iter );

    /* WSC 2.0, add WFA vendor id and sub-elements to vendor extension attribute  */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        iter = cy_wps_write_vendor_extension( iter, workspace );
    }

    if ( workspace->wps_mode == CY_WPS_PBC_MODE )
    {
        cy_wps_clear_pbc_overlap_array( );
        cy_wps_record_last_pbc_enrollee( &workspace->their_data.mac_address );
    }

    /* Give 500ms for enrollee to send WSC Done before we decide it's all over */
    cy_host_start_timer( workspace->wps_host_workspace, 500 );
    workspace->current_main_stage = CY_WPS_CLOSING_EAP;

    return iter;
}
#endif

static uint8_t* cy_wps_start_encrypted_tlv(uint8_t* iter)
{
    return (iter + sizeof(tlv16_header_t) + sizeof(cy_wps_iv_t));
}

/*
 * Note: start_of_encrypted_tlv_header should be the value provided to wps_start_encrypted_tlv()
 *       data_length should be the the number of bytes from start_of_encrypted_tlv_header to the end of the content to be encrypted
 */
static uint8_t* cy_wps_end_encrypted_tlv( cy_wps_agent_t* workspace, uint8_t* start_of_encrypted_tlv_header, uint8_t* end_of_data )
{
    cy_wps_hash_t hmac_output;
    tlv16_header_t* header   = (tlv16_header_t*)start_of_encrypted_tlv_header;
    uint8_t*   start_of_data = start_of_encrypted_tlv_header + sizeof(tlv16_header_t) + sizeof(cy_wps_iv_t);
    uint16_t   data_length   = (uint16_t)(end_of_data - start_of_data);
    uint16_t encrypted_size;

    /* Hash existing content */
    cy_sha2_hmac( (unsigned char *)&workspace->auth_key, SIZE_256_BITS, start_of_data, data_length, (uint8_t*) &hmac_output, 0 );

    /* Append a key wrap auth TLV */
    end_of_data = tlv_write_value( end_of_data, WPS_ID_KEY_WRAP_AUTH, SIZE_64_BITS, &hmac_output, TLV_UINT8_PTR );
    data_length = (uint16_t)(end_of_data - start_of_data);

    /* Encrypt the entire block and put the IV at the right spot */
    encrypted_size = (uint16_t) WPS_PADDED_AES_ROUND_UP(data_length, AES_BLOCK_SZ);
    cy_wps_encrypt_data( start_of_data, data_length, &workspace->key_wrap_key, start_of_data, (cy_wps_iv_t*)(start_of_encrypted_tlv_header + sizeof(tlv16_header_t)) );

    /* Fill in the TLV header reserved at the start by wps_start_encrypted_tlv() */
    header->type   = cy_hton16(WPS_ID_ENCR_SETTINGS);
    header->length = cy_hton16( (uint16_t)( encrypted_size + sizeof(cy_wps_iv_t) ) );

    return start_of_data + encrypted_size;
}


void cy_wps_deinit_workspace( cy_wps_agent_t* workspace )
{
    uint8_t a;
    for ( a = 0; a < 3; ++a )
    {
        if ( workspace->ie.common[a].data != NULL )
        {
            cy_wps_host_remove_vendor_ie( (uint32_t) workspace->interface, workspace->ie.common[a].data, workspace->ie.common[a].length, workspace->ie.common[a].packet_mask );
            cy_wps_free( workspace->ie.common[a].data );
            workspace->ie.common[a].data = NULL;
        }
    }

    cy_wps_cleanup_workspace( workspace );
}

static void cy_wps_cleanup_workspace( cy_wps_agent_t* workspace )
{
    if ( workspace->m1_copy != NULL )
    {
        cy_wps_free(workspace->m1_copy);
        workspace->m1_copy = NULL;
    }

    cy_wps_free_unfragmented_packet( workspace );
}

static void cy_wps_init_unfragmented_packet( cy_wps_agent_t* workspace, uint16_t total_length )
{
    WPS_ASSERT(workspace->fragmented_packet == NULL);

    workspace->fragmented_packet_length = 0;
    if ( total_length != 0 )
    {
        workspace->fragmented_packet_length_max = total_length;
    }
    else
    {
        workspace->fragmented_packet_length_max = 1024;
    }
    workspace->fragmented_packet = (uint8_t*) cy_wps_malloc("wps fragd pkt", workspace->fragmented_packet_length_max);
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Allocated fragmented packet = [0x%X]. Len Max = [%d]\r\n", (unsigned int*) workspace->fragmented_packet, workspace->fragmented_packet_length_max);
}

static void cy_wps_append_fragment( cy_wps_agent_t* workspace, void* fragment, uint16_t fragment_length )
{
    WPS_ASSERT(workspace->fragmented_packet != NULL);

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Frag Pkt Len + Frag Len = [%d], Max Frag Len = [%d]\r\n", workspace->fragmented_packet_length + fragment_length, workspace->fragmented_packet_length_max);
    if ( workspace->fragmented_packet_length + fragment_length >= workspace->fragmented_packet_length_max )
    {
        void* new_packet = cy_wps_malloc("wps append frag", (uint32_t)( workspace->fragmented_packet_length_max + fragment_length + 1024 ) );
        WPS_ASSERT(new_packet != NULL);
        workspace->fragmented_packet_length_max = (uint16_t)( workspace->fragmented_packet_length_max + fragment_length + 1024 );
        memcpy( new_packet, workspace->fragmented_packet, workspace->fragmented_packet_length );
        cy_wps_free( workspace->fragmented_packet );
        workspace->fragmented_packet = (uint8_t*) new_packet;
    }

    memcpy( &workspace->fragmented_packet[workspace->fragmented_packet_length], fragment, fragment_length );
    workspace->fragmented_packet_length = (uint16_t) ( workspace->fragmented_packet_length + fragment_length );
}

static void cy_wps_retrieve_unfragmented_packet( cy_wps_agent_t* workspace, cy_packet_t* packet, uint16_t* packet_length )
{
    *packet        = workspace->fragmented_packet;
    *packet_length = workspace->fragmented_packet_length;
}

static void cy_wps_free_unfragmented_packet( cy_wps_agent_t* workspace )
{
    if ( workspace->fragmented_packet != NULL )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Freeing defragmented packet. Len Max = [%d]\r\n", workspace->fragmented_packet_length_max);
        cy_wps_free( workspace->fragmented_packet );
        workspace->fragmented_packet = NULL;
        workspace->fragmented_packet_length = 0;
        workspace->processing_fragmented_packet = FALSE;
    }
}

static cy_rslt_t cy_wps_process_packet_fragmentation(cy_wps_agent_t* workspace, cy_packet_t eapol_packet, uint8_t** data, uint16_t* data_size )
{
    uint8_t*          packet_data              = whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, eapol_packet );
    uint16_t          real_packet_length       = whd_buffer_get_current_piece_size( workspace->interface->whd_driver, eapol_packet );        /* Packet length includes Ethernet header */
    cy_wps_msg_packet_t* packet                   = (cy_wps_msg_packet_t*) packet_data;
    uint16_t          data_length              = (uint16_t)( real_packet_length - sizeof(cy_wps_msg_packet_header_t) );
    uint16_t          eap_packet_length;
    uint16_t          calculated_packet_length;
    eap_packet_length = packet->eapol.length;
    eap_packet_length = CY_WPS_HOST_READ_16_BE((uint8_t *)&eap_packet_length);
    calculated_packet_length = sizeof( cy_ether_header_t ) + sizeof( cy_eapol_header_t ) + eap_packet_length;

    /* Sanity check various lengths */
    if ( real_packet_length < calculated_packet_length )
    {
        /* This is a runt packet. The only thing we can do is flag an error. */
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Runt WPS packet. Real packet length: %u Calculated packet length: %u\r\n", (unsigned int)real_packet_length, (unsigned int)calculated_packet_length );
        return CY_RSLT_WPS_ERROR_RUNT_WPS_PACKET;
    }
    else if ( real_packet_length > calculated_packet_length )
    {
        /* This is a giant packet. Recalculate data length to remove trailing garbage */
        data_length = eap_packet_length - ( sizeof( cy_eap_header_t ) + sizeof( cy_eap_expanded_header_t ) );
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS packet with trailing garbage. Recalculated data length is: %u\r\n", (unsigned int)data_length );
    }

    /* Check if we have a fragment */
    if ( workspace->processing_fragmented_packet == TRUE )
    {
        packet_data          = cy_get_wps_packet_data( packet );

        /* Append to end of fragment */
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Append Fragment. Data Len = [%d], sizeof(eapol_pkt) = [%d]\r\n", data_length, sizeof(cy_eapol_packet_header_t));
        cy_wps_append_fragment( workspace, packet_data, data_length );

        /* Check if there is are more fragments to come */
        if ( packet->eap_expanded.flags & WPS_MORE_FRAGMENTS_MASK )
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Received another fragment (%d bytes)\r\n", data_length - sizeof(cy_eapol_packet_header_t) );
            goto send_frag_ack;
        }
        else
        {
            uint8_t temp_id = packet->eap.id;
            /* Start processing the message */
            cy_wps_retrieve_unfragmented_packet( workspace, (void**) &packet_data, &calculated_packet_length );
            packet = (cy_wps_msg_packet_t*) packet_data;
            packet->eap.id = temp_id;
            workspace->processing_fragmented_packet = FALSE;
        }
    }
    /* Else check if this is the first fragment */
    else if ( packet->eap_expanded.flags & WPS_MORE_FRAGMENTS_MASK )
    {
        workspace->processing_fragmented_packet = TRUE;
        if ( packet->eap_expanded.flags & WPS_LENGTH_FIELD_MASK )
        {
            uint16_t* length_field = (uint16_t*) packet->data;
            cy_wps_init_unfragmented_packet( workspace, cy_hton16( *length_field ) );
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%d bytes expected\r\n", cy_hton16( *length_field ));
        }
        else
        {
            cy_wps_init_unfragmented_packet( workspace, 0 );
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Total length unknown\r\n");
        }
        cy_wps_append_fragment( workspace, packet_data, calculated_packet_length );

        goto send_frag_ack;
    }

    *data      = packet_data;
    *data_size = calculated_packet_length;
    return CY_RSLT_SUCCESS;

send_frag_ack:
    /* Send back a frag ack */
    workspace->last_received_id = packet->eap.id;
    cy_host_stop_timer( workspace->wps_host_workspace );
    cy_host_start_timer( workspace->wps_host_workspace , WPS_EAPOL_PACKET_TIMEOUT );
    cy_wps_send_frag_ack( workspace );
    return CY_RSLT_WPS_IN_PROGRESS;
}

static uint32_t cy_wps_send_frag_ack( cy_wps_agent_t* workspace )
{
    cy_packet_t packet;
    cy_wps_msg_packet_header_t* header;
    uint16_t aligned_length;
    uint32_t aligned_vendor_type;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Sending frag ACK\r\n");

    /* Create the packet and write the EAP header */
    whd_host_buffer_get( workspace->interface->whd_driver, &packet, WHD_NETWORK_TX, 1024 + WHD_LINK_HEADER, true );
    whd_buffer_add_remove_at_front( workspace->interface->whd_driver, &packet, WHD_LINK_HEADER );

    header = (cy_wps_msg_packet_header_t*) whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, packet );

    if ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT )
    {
        header->eap.code   = CY_EAP_CODE_REQUEST;
    }
    else
    {
        header->eap.code   = CY_EAP_CODE_RESPONSE;
    }
    header->eap.id     = workspace->last_received_id;
    CY_WPS_HOST_WRITE_16_BE(&aligned_length, (sizeof(cy_eap_header_t) + sizeof(cy_eap_expanded_header_t)));
    header->eap.length = aligned_length;
    header->eap.type   = CY_EAP_TYPE_WPS;

    header->eap_expanded.flags       = 0;
    header->eap_expanded.op_code     = 6;
    CY_WPS_HOST_WRITE_32_BE(&aligned_vendor_type, 1);
    header->eap_expanded.vendor_type = aligned_vendor_type;
    memcpy( header->eap_expanded.vendor_id, WFA_VENDOR_EXT_ID, 3 );

    cy_wps_send_eapol_packet( packet, workspace, CY_EAP_PACKET, &workspace->their_data.mac_address, sizeof(cy_eap_header_t) + sizeof(cy_eap_expanded_header_t) );
    return CY_RSLT_SUCCESS;
}

static uint8_t* cy_get_wps_packet_data(cy_wps_msg_packet_t* packet)
{
    /* Check if the packet contains the length field */
    if ((packet->eap_expanded.flags & WPS_LENGTH_FIELD_MASK) != 0)
    {
        return (packet->data + sizeof(uint16_t));
    }
    else
    {
        return packet->data;
    }
}

cy_rslt_t cy_wps_send_basic_packet( cy_wps_agent_t* workspace, uint8_t type, uint16_t optional_config_error )
{
    uint8_t               version;
    cy_packet_t           packet;
    cy_wps_msg_packet_t*  packet_header;
    uint8_t*              iter;
    uint16_t              eap_length;
    uint16_t              aligned_length;
    uint32_t              aligned_vendor_type;

    /* Create the packet */
    whd_host_buffer_get( workspace->interface->whd_driver, &packet, WHD_NETWORK_TX, 1024 + WHD_LINK_HEADER, true );
    whd_buffer_add_remove_at_front( workspace->interface->whd_driver, &packet, WHD_LINK_HEADER );

    packet_header = (cy_wps_msg_packet_t*) whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, packet );
    iter = packet_header->data;

    switch ( type )
    {
        case WPS_PRIVATE_ID_FRAG_ACK:
            packet_header->eap_expanded.op_code = 6; /* WSC Frag ACK */
            break;
        case WPS_ID_MESSAGE_ACK:
            packet_header->eap_expanded.op_code = 2; /* WSC ACK */
            break;
        case WPS_ID_MESSAGE_DONE:
            packet_header->eap_expanded.op_code = 5; /* WSC Done */
            break;
        case WPS_ID_MESSAGE_NACK:
            packet_header->eap_expanded.op_code = 3; /* WSC NACK */
            break;
        default:
            packet_header->eap_expanded.op_code = 4; /* WSC Message */
            break;
    }

    memcpy( packet_header->eap_expanded.vendor_id, WFA_VENDOR_EXT_ID, 3 );
    CY_WPS_HOST_WRITE_32_BE(&aligned_vendor_type, 1);
    packet_header->eap_expanded.vendor_type = aligned_vendor_type;
    /* Version */
    version = WPS_VERSION;
    iter = tlv_write_value( iter, WPS_ID_VERSION, WPS_ID_VERSION_S, &version, TLV_UINT8 );

    /* Message type */
    iter = tlv_write_value( iter, WPS_ID_MSG_TYPE, WPS_ID_MSG_TYPE_S, &type, TLV_UINT8 );

    /* Enrollee and Registrar nonces */
    iter = tlv_write_value( iter, WPS_ID_ENROLLEE_NONCE,  sizeof(cy_wps_nonce_t), &workspace->enrollee_data->nonce,  TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_REGISTRAR_NONCE, sizeof(cy_wps_nonce_t), &workspace->registrar_data->nonce, TLV_UINT8_PTR );

    if ( type == WPS_ID_MESSAGE_NACK )
    {
        iter = tlv_write_value( iter, WPS_ID_CONFIG_ERROR, WPS_ID_CONFIG_ERROR_S, &optional_config_error, TLV_UINT16 );
    }

    /* WSC 2.0 vendor extension */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        iter = cy_wps_write_vendor_extension( iter, workspace );
    }

    eap_length = (uint16_t)( (uint16_t)(iter - packet_header->data) + sizeof(cy_eap_header_t) + sizeof(cy_eap_expanded_header_t) );
    /* write the EAP header */
    packet_header->eap.code           = (workspace->agent_type == CY_WPS_REGISTRAR_AGENT || workspace->in_reverse_registrar_mode == 1) ? CY_EAP_CODE_REQUEST : CY_EAP_CODE_RESPONSE;;
    packet_header->eap.id             = workspace->last_received_id;
    CY_WPS_HOST_WRITE_16_BE(&aligned_length, eap_length );
    packet_header->eap.length = aligned_length;
    packet_header->eap.type           = CY_EAP_TYPE_WPS;
    packet_header->eap_expanded.flags = 0;

    cy_wps_send_eapol_packet( packet, workspace, CY_EAP_PACKET, &workspace->their_data.mac_address, eap_length );

    return CY_RSLT_SUCCESS;
}

static uint8_t* cy_wps_write_vendor_extension( uint8_t* iter, cy_wps_agent_t* workspace )
{
    template_vendor_extension_t* vendor_ext = (template_vendor_extension_t*) iter;

    vendor_ext->header.type     = cy_hton16(WPS_ID_VENDOR_EXT);
    vendor_ext->header.length   = cy_hton16(6);
    memcpy( vendor_ext->vendor_extension_id, WFA_VENDOR_EXT_ID, 3 );

    vendor_ext->version2.type   = WPS_WFA_SUBID_VERSION2;
    vendor_ext->version2.length = 1;
    vendor_ext->version2.data   = workspace->my_data.supported_version;

    return iter + sizeof(template_vendor_extension_t);
}

static cy_rslt_t cy_wps_validate_secret_nonce(cy_wps_agent_t* workspace, cy_wps_agent_data_t* agent_data, uint8_t* data, uint8_t which_nonce)
{
    cy_wps_hash_t temp_hash;
    /* false positive: 238404 Out-of-bounds access
     * data pointer is pointing to tlv->data[1]. coverity is assuming it is only 1 byte array. but this is variable
     * length array and memory for it is already allocated.
     */
    memcpy( &agent_data->secret_nonce[which_nonce], data, sizeof(cy_wps_nonce_t) );
    cy_wps_calculate_hash( workspace, agent_data, &temp_hash, which_nonce );
    if ( memcmp( &temp_hash, &agent_data->secret_hash[which_nonce], sizeof(cy_wps_hash_t) ) != 0 )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "WPS: Secret nonce validation failed\r\n");
        return CY_RSLT_WPS_ERROR_SECRET_NONCE_MISMATCH;
    }
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_wps_send_wsc_nack(cy_wps_agent_t* workspace, uint16_t config_error)
{
    return cy_wps_send_basic_packet( workspace, WPS_ID_MESSAGE_NACK, config_error );
}

void cy_wps_send_eapol_packet(cy_packet_t packet, cy_wps_agent_t* workspace, cy_eapol_packet_type_t type, whd_mac_t* their_mac_address, uint16_t content_size )
{
    cy_host_workspace_t* wps_host_workspace = (cy_host_workspace_t*) workspace->wps_host_workspace;
    cy_eapol_packet_t* header = (cy_eapol_packet_t*) whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, packet );
    memcpy( header->ethernet.ether_dhost, their_mac_address, sizeof(whd_mac_t) );
    memcpy( header->ethernet.ether_shost, &workspace->my_data.mac_address, sizeof(whd_mac_t) );
    header->ethernet.ether_type = cy_hton16( ETHER_TYPE_802_1X );
    header->eapol.version = 1;
    header->eapol.type    = type;
    header->eapol.length  = cy_hton16( content_size );
    whd_buffer_set_size( wps_host_workspace->interface->whd_driver, packet, (uint16_t)( content_size + sizeof(cy_eapol_packet_header_t) ));
    whd_network_send_ethernet_data( wps_host_workspace->interface, packet );
}

static cy_rslt_t cy_wps_send_done( cy_wps_agent_t* workspace )
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Sending WSC Done\r\n");
    return cy_wps_send_basic_packet( workspace, WPS_ID_MESSAGE_DONE, 0 );
}
