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
* @file cy_wps_enrollee.c
* @brief WPS Enrollee implementation
*/

#include "cy_template_wps_packets.h"
#include "cy_wps_common.h"
#include "cy_wps_structures.h"
#include "cy_wcm_log.h"
#include "string.h" /* For memcpy() */
#include "whd_types.h"
#include "whd_wlioctl.h"
#include "whd_buffer_api.h"

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#define WPS_ENROLLEE_DISCOVER               (CY_WPS_WSC_START + 1)
#define CY_WPS_ENROLLEE_WAIT_FOR_TIMEOUT    (CY_WPS_WSC_START + 2)

#define BROADCAST_ETHERNET_ADDRESS          "\xff\xff\xff\xff\xff\xff"

#define DEFAULT_REQ_TO_ENROLL    1

#define VNDR_IE_ASSOCREQ_FLAG    0x20
#define VNDR_IE_PRBREQ_FLAG      0x10

#define WL_CHANSPEC_CHAN_MASK    0x00ff

#ifdef ENABLE_WCM_LOGS
#define cy_wcm_log_msg cy_log_msg
#else
#define cy_wcm_log_msg(a,b,c,...)
#endif

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef cy_rslt_t (*cy_wps_enrollee_action_t)( cy_wps_agent_t* workspace, whd_interface_t interface );

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

static cy_rslt_t    cy_wps_enrollee_event_handler  (cy_wps_agent_t* workspace, cy_event_message_t* message);
static cy_rslt_t    cy_wps_send_eapol_start        ( cy_wps_agent_t *workspace, whd_interface_t interface );
static cy_rslt_t    cy_wps_send_identity           ( cy_wps_agent_t* workspace, whd_interface_t interface );
static cy_rslt_t    cy_wps_find_and_join_ap        ( cy_wps_agent_t* workspace, whd_interface_t interface );
static cy_rslt_t    cy_create_wps_probe_ie         ( cy_wps_agent_t* workspace );
static cy_rslt_t    cy_create_wps_assoc_request_ie ( cy_wps_agent_t* workspace );

/******************************************************
 *               Variable Definitions
 ******************************************************/

static const cy_wps_enrollee_action_t enrollee_actions[] =
{
    [CY_WPS_EAP_START]                 = cy_wps_send_eapol_start,
    [CY_WPS_EAP_IDENTITY]              = cy_wps_send_identity,
    [CY_WPS_WSC_START]                 = NULL,
    [CY_WPS_ENROLLEE_DISCOVER]         = cy_wps_find_and_join_ap,
    [CY_WPS_ENROLLEE_WAIT_FOR_TIMEOUT] = NULL,
};

/******************************************************
 *               Function Definitions
 ******************************************************/


static cy_rslt_t cy_wps_enrollee_event_handler(cy_wps_agent_t* workspace, cy_event_message_t* message)
{
    cy_wps_msg_packet_t* packet;

    if (workspace->current_main_stage == CY_WPS_IN_WPS_HANDSHAKE )
    {
        return CY_RSLT_WPS_UNPROCESSED;
    }

    /* Process the event */
    switch ( message->event_type )
    {
        case CY_EVENT_EAPOL_PACKET_RECEIVED:
            packet = (cy_wps_msg_packet_t*)whd_buffer_get_current_piece_data_pointer(workspace->interface->whd_driver, message->data.packet);

            /* Check for identity request */
            if ( packet->eap.code == CY_EAP_CODE_REQUEST && packet->eap.type == CY_EAP_TYPE_IDENTITY )
            {
                ++workspace->identity_request_received_count;
                if (workspace->identity_request_received_count > 3)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Registrar state machine not progressing. Leaving\r\n");
                    workspace->current_sub_stage = WPS_ENROLLEE_DISCOVER;
                }
                else
                {
                    workspace->last_received_id = packet->eap.id;
                    workspace->current_sub_stage = CY_WPS_EAP_IDENTITY;
                }
            }
            /* Check for WPS start */
            else if ( packet->eap.code == CY_EAP_CODE_REQUEST && packet->eap.type == CY_EAP_TYPE_WPS && packet->eap_expanded.op_code == 1 )
            {
                /* Pass this event to the common WPS handshake code */
                workspace->last_received_id = packet->eap.id;
                workspace->current_main_stage = CY_WPS_IN_WPS_HANDSHAKE;
                message->event_type = CY_WPS_EVENT_RECEIVED_WPS_START;
                whd_buffer_release( workspace->interface->whd_driver, message->data.packet, WHD_NETWORK_RX );
                return CY_RSLT_WPS_UNPROCESSED;
            }
            /* Check for EAP fail */
            else if ( packet->eap.code == CY_EAP_CODE_FAILURE )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Received EAP Fail\r\n");

                if (workspace->current_main_stage == CY_WPS_CLOSING_EAP)
                {
                    // All done. Finish up.
                    workspace->wps_result = CY_RSLT_WPS_COMPLETE;
                    whd_wifi_leave( workspace->interface );
                }
                else
                {
                    whd_wifi_leave( workspace->interface );
                    workspace->current_sub_stage = CY_WPS_ENROLLEE_DISCOVER;
                }
            }
            whd_buffer_release( workspace->interface->whd_driver, message->data.packet, WHD_NETWORK_RX );
            break;

        case CY_WPS_EVENT_DISCOVER_COMPLETE:
            if ( workspace->wps_mode == CY_WPS_PBC_MODE )
            {
                if ( cy_wps_enrollee_pbc_overlap_check( workspace) != CY_RSLT_SUCCESS )
                {
                    workspace->wps_result = CY_RSLT_WPS_PBC_OVERLAP;
                    return CY_RSLT_SUCCESS;
                }
            }
            workspace->current_sub_stage = CY_WPS_ENROLLEE_DISCOVER;
            break;

        case CY_WPS_EVENT_ENROLLEE_ASSOCIATED:
            workspace->identity_request_received_count = 0;
            //memcpy(&workspace->their_data.mac_address, &workspace->ap->BSSID, sizeof(whd_mac_t));
            break;

        case CY_EVENT_TIMER_TIMEOUT:
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Timeout...\r\n");
            whd_wifi_leave( workspace->interface );
            if (workspace->current_main_stage == CY_WPS_CLOSING_EAP)
            {
                // All done. Finish up.
                workspace->wps_result = CY_RSLT_WPS_COMPLETE;
            }
            else
            {
                workspace->current_sub_stage = CY_WPS_ENROLLEE_DISCOVER;
            }
            break;

        case CY_EVENT_COMPLETE:
            break;

        case CY_EVENT_NO_EVENT:
        case CY_EVENT_ABORT_REQUESTED:
        case CY_EVENT_RECEIVED_IDENTITY_REQUEST:
        case CY_WPS_EVENT_RECEIVED_IDENTITY:
        case CY_WPS_EVENT_RECEIVED_WPS_START:
        case CY_WPS_EVENT_RECEIVED_EAPOL_START:
        case CY_WPS_EVENT_PBC_OVERLAP_NOTIFY_USER:
        default:
            return CY_RSLT_WPS_UNPROCESSED;
            break;
    }

    if ( workspace->current_main_stage != CY_WPS_CLOSING_EAP && enrollee_actions[workspace->current_sub_stage] != NULL )
    {
        /* Do the action of the corresponding sub stage */
        enrollee_actions[workspace->current_sub_stage](workspace, workspace->interface);
    }

    return CY_RSLT_SUCCESS;
}


void cy_wps_enrollee_init( cy_wps_agent_t* workspace )
{
    workspace->event_handler = cy_wps_enrollee_event_handler;

    workspace->enrollee_data  = &workspace->my_data;
    workspace->registrar_data = &workspace->their_data;
    workspace->available_crypto_material |= CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE |
                                            CY_WPS_CRYPTO_MATERIAL_ENROLLEE_PUBLIC_KEY |
                                            CY_WPS_CRYPTO_MATERIAL_ENROLLEE_MAC_ADDRESS;

    workspace->current_main_stage = CY_WPS_INITIALISING;
    workspace->current_sub_stage  = CY_WPS_ENROLLEE_DISCOVER;
    workspace->association_state  = 0;

    if (workspace->ie.enrollee.association_request.data != NULL)
    {
        cy_wps_host_remove_vendor_ie( (uint32_t) workspace->interface, workspace->ie.enrollee.association_request.data, workspace->ie.enrollee.association_request.length, workspace->ie.enrollee.association_request.packet_mask );
        cy_wps_free(workspace->ie.enrollee.association_request.data);
        workspace->ie.enrollee.association_request.data = NULL;
    }

    if (workspace->ie.enrollee.probe_request.data != NULL)
    {
        cy_wps_host_remove_vendor_ie( (uint32_t) workspace->interface, workspace->ie.enrollee.probe_request.data, workspace->ie.enrollee.probe_request.length, workspace->ie.enrollee.probe_request.packet_mask );
        cy_wps_free(workspace->ie.enrollee.probe_request.data);
        workspace->ie.enrollee.probe_request.data = NULL;
    }

    // Add IEs to probe request and association request
    cy_create_wps_assoc_request_ie( workspace );
    cy_create_wps_probe_ie        ( workspace );
    workspace->ie.enrollee.association_request.packet_mask = VNDR_IE_ASSOCREQ_FLAG;
    workspace->ie.enrollee.probe_request.packet_mask       = VNDR_IE_PRBREQ_FLAG;
    if ( workspace->ie.enrollee.association_request.data != NULL )
    {
        cy_wps_host_add_vendor_ie( (uint32_t) workspace->interface, workspace->ie.enrollee.association_request.data, workspace->ie.enrollee.association_request.length, workspace->ie.enrollee.association_request.packet_mask );
    }
    if ( workspace->ie.enrollee.probe_request.data != NULL )
    {
        cy_wps_host_add_vendor_ie( (uint32_t) workspace->interface, workspace->ie.enrollee.probe_request.data, workspace->ie.enrollee.probe_request.length, workspace->ie.enrollee.probe_request.packet_mask );
    }
}

void cy_wps_enrollee_reset( cy_wps_agent_t* workspace, whd_interface_t interface )
{
    cy_wps_enrollee_init( workspace );
    cy_wps_find_and_join_ap( workspace, interface );
}

void cy_wps_enrollee_start( cy_wps_agent_t* workspace, whd_interface_t interface )
{
    uint8_t a;
    workspace->wps_result = CY_RSLT_WPS_IN_PROGRESS;

    /* The P2P enrollee agent adds its own IEs in a different order so we don't handle it here */
    if ( ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT ) && ( workspace->is_p2p_enrollee == 0 ) )
    {
        // Remove WPS IEs from Probe Request and Association Request if they already exist
        for ( a = 0; a < 3; ++a )
        {
            if ( workspace->ie.common[a].data != NULL )
            {
                cy_wps_host_remove_vendor_ie( (uint32_t) workspace->interface, workspace->ie.common[a].data, workspace->ie.common[a].length, workspace->ie.common[a].packet_mask );
                cy_wps_free( workspace->ie.common[a].data );
                workspace->ie.common[a].data = NULL;
            }
        }

        // Add IEs to probe request and association request
        cy_create_wps_assoc_request_ie( workspace );
        cy_create_wps_probe_ie        ( workspace );
        workspace->ie.enrollee.association_request.packet_mask = VNDR_IE_ASSOCREQ_FLAG;
        workspace->ie.enrollee.probe_request.packet_mask       = VNDR_IE_PRBREQ_FLAG;
        cy_wps_host_add_vendor_ie( (uint32_t) workspace->interface, workspace->ie.enrollee.association_request.data, workspace->ie.enrollee.association_request.length, workspace->ie.enrollee.association_request.packet_mask );
        cy_wps_host_add_vendor_ie( (uint32_t) workspace->interface, workspace->ie.enrollee.probe_request.data, workspace->ie.enrollee.probe_request.length, workspace->ie.enrollee.probe_request.packet_mask );
    }

    // Kick off a scan to discover APs
    cy_wps_find_and_join_ap( workspace, interface );
}

static cy_rslt_t cy_wps_send_eapol_start( cy_wps_agent_t *workspace, whd_interface_t interface )
{
    cy_packet_t packet;
    UNUSED_PARAMETER( interface );

    whd_host_buffer_get( workspace->interface->whd_driver, &packet, WHD_NETWORK_TX, sizeof(cy_eapol_packet_t) + WHD_LINK_HEADER, true );
    if (packet == 0)
    {
        return CY_RSLT_WPS_ERROR_CREATING_EAPOL_PACKET;
    }

    whd_buffer_add_remove_at_front( workspace->interface->whd_driver, &packet, WHD_LINK_HEADER );

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Sending EAPOL start\r\n");
    cy_host_start_timer( workspace->wps_host_workspace, WPS_EAPOL_PACKET_TIMEOUT );
    cy_wps_send_eapol_packet( packet, workspace, CY_EAPOL_START, &workspace->their_data.mac_address,0);

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_wps_send_identity( cy_wps_agent_t* workspace, whd_interface_t interface )
{
    cy_packet_t packet;
    cy_eap_packet_t*    header;
    UNUSED_PARAMETER( interface );
    uint16_t aligned_length;

    whd_host_buffer_get( workspace->interface->whd_driver, &packet, WHD_NETWORK_TX, sizeof(cy_eap_packet_t) + (sizeof( ENROLLEE_ID_STRING ) - 1) + WHD_LINK_HEADER, true );
    whd_buffer_add_remove_at_front( workspace->interface->whd_driver, &packet, WHD_LINK_HEADER );
    header             = (cy_eap_packet_t*) whd_buffer_get_current_piece_data_pointer( workspace->interface->whd_driver, packet );
    header->eap.code   = CY_EAP_CODE_RESPONSE;
    header->eap.id     = workspace->last_received_id;
    CY_WPS_HOST_WRITE_16_BE(&aligned_length, (sizeof(cy_eap_header_t) + sizeof(ENROLLEE_ID_STRING) - 1));
    header->eap.length = aligned_length;
    header->eap.type   = CY_EAP_TYPE_IDENTITY;
    memcpy( header->data, ENROLLEE_ID_STRING, sizeof( ENROLLEE_ID_STRING ) - 1 );
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Sending Identity\r\n");

    cy_host_start_timer( workspace->wps_host_workspace, WPS_EAPOL_PACKET_TIMEOUT );
    cy_wps_send_eapol_packet( packet, workspace, CY_EAP_PACKET, &workspace->their_data.mac_address, sizeof(cy_eap_header_t) + sizeof(ENROLLEE_ID_STRING) - 1 );

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_wps_find_and_join_ap( cy_wps_agent_t* workspace, whd_interface_t interface )
{
    cy_rslt_t result;
    size_t    output_length = 0;
    do
    {
        if ( workspace->directed_wps_max_attempts != 0 )
        {
            if ( workspace->ap_join_attempts >= workspace->directed_wps_max_attempts )
            {
                workspace->wps_result = CY_RSLT_WPS_TIMEOUT;
                return CY_RSLT_WPS_TIMEOUT;
            }
        }
        else
        {
            /* Get an AP off the list stored by the host */
            workspace->ap = cy_wps_host_retrieve_ap( workspace->wps_host_workspace );
            workspace->ap_join_attempts = 0;
        }

        /* Check if we don't have an AP OR we have more than 1 AP in PBC mode */
        if ( workspace->ap == NULL )
        {
            /* Check if this is the first scan coming from another state */
            if ( workspace->current_sub_stage != WPS_ENROLLEE_DISCOVER )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Looking for WPS AP\r\n");
            }
            /* Run discovery */
            workspace->current_sub_stage = WPS_ENROLLEE_DISCOVER;
            cy_wps_host_scan( workspace, cy_wps_scan_result_handler, interface );
            result = CY_RSLT_SUCCESS;
        }
        else
        {
            /* Generate a new public nonce. This is useful for PIN mode where we may join multiple APs until we find the right one */
            cy_host_random_bytes( (uint8_t*) &workspace->my_data.nonce, SIZE_128_BITS, &output_length );

            /* Reset the current sub-stage to the start */
            workspace->current_sub_stage = CY_WPS_EAP_START;

            /* Join the AP */
            ++workspace->ap_join_attempts;
            result = cy_wps_host_join( workspace->wps_host_workspace, workspace->ap, interface );
            if (result == CY_RSLT_SUCCESS)
            {
                memcpy(&workspace->their_data.mac_address, &workspace->ap->scan_result.BSSID, sizeof(whd_mac_t));
            }
        }
    } while (result != CY_RSLT_SUCCESS);
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_create_wps_assoc_request_ie( cy_wps_agent_t* workspace )
{
    uint8_t  version      = WPS_VERSION;
    uint8_t  request_type = WPS_MSGTYPE_ENROLLEE_INFO_ONLY;
    uint8_t* iter;

    workspace->ie.enrollee.association_request.data = cy_wps_malloc( "wps", sizeof(template_wps2_assoc_request_ie_t) );
    if ( workspace->ie.enrollee.association_request.data == NULL )
    {
        return CY_RSLT_WPS_ERROR_OUT_OF_MEMORY;
    }

    iter = workspace->ie.enrollee.association_request.data;
    iter = tlv_write_value( iter, WPS_ID_VERSION,  WPS_ID_VERSION_S,  &version,      TLV_UINT8 );
    iter = tlv_write_value( iter, WPS_ID_REQ_TYPE, WPS_ID_REQ_TYPE_S, &request_type, TLV_UINT8 );

    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        cy_vendor_ext_t* vendor_ext = (cy_vendor_ext_t*) tlv_write_header( iter, WPS_ID_VENDOR_EXT, sizeof(cy_vendor_ext_t) );
        memcpy( vendor_ext->vendor_id, WFA_VENDOR_EXT_ID, 3 );
        vendor_ext->subid_version2.type   = WPS_WFA_SUBID_VERSION2;
        vendor_ext->subid_version2.length = 1;
        vendor_ext->subid_version2.data   = workspace->my_data.supported_version;
        iter += sizeof(tlv16_header_t) + sizeof(cy_vendor_ext_t);
    }

    workspace->ie.enrollee.association_request.length = (uint16_t)(iter - workspace->ie.enrollee.association_request.data);

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_create_wps_probe_ie( cy_wps_agent_t* workspace )
{
    uint8_t  version      = WPS_VERSION;
    uint8_t  request_type = WPS_MSGTYPE_ENROLLEE_INFO_ONLY;
    uint16_t config_error = 0;
    uint16_t assoc_state  = 0;
    uint8_t* iter;
    uint16_t size_of_probe_ie = (uint16_t)( sizeof(template_wps2_probe_ie_t) +
                                            strlen( workspace->device_details->manufacturer ) +
                                            strlen( workspace->device_details->model_name ) +
                                            strlen( workspace->device_details->model_number ) +
                                            strlen( workspace->device_details->device_name ) );

    workspace->ie.enrollee.probe_request.data = cy_wps_malloc("wps", size_of_probe_ie);
    if ( workspace->ie.enrollee.probe_request.data == NULL )
    {
        return CY_RSLT_WPS_ERROR_OUT_OF_MEMORY;
    }

    /* Add Probe request IE */
    iter = workspace->ie.enrollee.probe_request.data;
    iter = tlv_write_value( iter, WPS_ID_VERSION,        WPS_ID_VERSION_S,        &version,                  TLV_UINT8 );
    iter = tlv_write_value( iter, WPS_ID_REQ_TYPE,       WPS_ID_REQ_TYPE_S,       &request_type,             TLV_UINT8 );
    iter = tlv_write_value( iter, WPS_ID_CONFIG_METHODS, WPS_ID_CONFIG_METHODS_S, &workspace->device_details->config_methods, TLV_UINT16 );

    iter = tlv_write_value( iter, WPS_ID_UUID_E, WPS_ID_UUID_S, &workspace->uuid, TLV_UINT8_PTR );

    iter = tlv_write_value( iter, WPS_ID_PRIM_DEV_TYPE, WPS_ID_PRIM_DEV_TYPE_S, &workspace->primary_device,     TLV_UINT8_PTR );
    iter = tlv_write_value( iter, WPS_ID_RF_BAND,       WPS_ID_RF_BAND_S,       &workspace->rfBand,             TLV_UINT8 );
    iter = tlv_write_value( iter, WPS_ID_ASSOC_STATE,   WPS_ID_ASSOC_STATE_S,   &assoc_state,                   TLV_UINT16 );
    iter = tlv_write_value( iter, WPS_ID_CONFIG_ERROR,  WPS_ID_CONFIG_ERROR_S,  &config_error,                  TLV_UINT16 );
    iter = tlv_write_value( iter, WPS_ID_DEVICE_PWD_ID, WPS_ID_DEVICE_PWD_ID_S, &workspace->device_password_id, TLV_UINT16 );

    /* WSC 2.0 */
    if ( workspace->my_data.supported_version >= WPS_VERSION2 )
    {
        cy_wps_m1_vendor_ext_t* vendor_ext;

        /* Manufacturer, Model Name, Model Number, Device Name */
        iter = tlv_write_value( iter, WPS_ID_MANUFACTURER, (uint16_t) strlen( workspace->device_details->manufacturer ), workspace->device_details->manufacturer, TLV_UINT8_PTR );
        iter = tlv_write_value( iter, WPS_ID_MODEL_NAME,   (uint16_t) strlen( workspace->device_details->model_name ),   workspace->device_details->model_name,   TLV_UINT8_PTR );
        iter = tlv_write_value( iter, WPS_ID_MODEL_NUMBER, (uint16_t) strlen( workspace->device_details->model_number ), workspace->device_details->model_number, TLV_UINT8_PTR );
        iter = tlv_write_value( iter, WPS_ID_DEVICE_NAME,  (uint16_t) strlen( workspace->device_details->device_name ),  workspace->device_details->device_name,  TLV_UINT8_PTR );

        /* Add WFA Vendor Extension */
        vendor_ext = (cy_wps_m1_vendor_ext_t*) tlv_write_header( iter, WPS_ID_VENDOR_EXT, sizeof(cy_wps_m1_vendor_ext_t) );
        memcpy( vendor_ext->vendor_id, WFA_VENDOR_EXT_ID, 3 );
        vendor_ext->subid_version2.type      = WPS_WFA_SUBID_VERSION2;
        vendor_ext->subid_version2.length    = 1;
        vendor_ext->subid_version2.data      = workspace->my_data.supported_version;
        vendor_ext->request_to_enroll.type   = WPS_WFA_SUBID_REQ_TO_ENROLL;
        vendor_ext->request_to_enroll.length = 1;
        vendor_ext->request_to_enroll.data   = DEFAULT_REQ_TO_ENROLL;
        iter += sizeof(tlv16_header_t) + sizeof(cy_wps_m1_vendor_ext_t);
    }

    workspace->ie.enrollee.probe_request.length = (uint16_t)(iter - workspace->ie.enrollee.probe_request.data);

    return CY_RSLT_SUCCESS;
}
