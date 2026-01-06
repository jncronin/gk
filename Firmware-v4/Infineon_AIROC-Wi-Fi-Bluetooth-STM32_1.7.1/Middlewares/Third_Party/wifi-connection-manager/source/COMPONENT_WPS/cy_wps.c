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
* @file cy_wps.c
* @brief Cypress WPS implementation
*/

/******************************************************
 *            Includes
 ******************************************************/

#include "cy_wps.h"

#include "cy_eapol.h"
#include "cy_wps_common.h"
#include "cy_wps_constants.h"
#include "cy_wps_structures.h"
#include "string.h"
#include "stdlib.h"
#include "cy_wcm_log.h"
#include "whd_wifi_api.h"
#include "cy_wps_memory.h"
#include "whd_types_int.h"
#include "whd_events_int.h"
#include "whd_network_types.h"
#include "whd_cdc_bdc.h"
#include "whd_buffer_api.h"
#include "whd_int.h"
#include "whd_debug.h"
#include "cy_wifimwcore_eapol.h"

/******************************************************
 *             Constants
 ******************************************************/

#define EAPOL_HEADER_SPACE                (sizeof(ether_header_t) + sizeof(eapol_header_t))
#define WL_CHANSPEC_CHAN_MASK             0x00ff
#define WL_CHANSPEC_BAND_2G               0x2000
#define WPS_THREAD_STACK_SIZE             (4*1024)
#define AUTHORIZED_MAC_LIST_LENGTH        (1)
#define ACTIVE_WPS_WORKSPACE_ARRAY_SIZE   (1)

#define DOT11_IE_ID_VENDOR_SPECIFIC       ( 221 )
#define KDK_REQUIRED_CRYPTO_MATERIAL      (CY_WPS_CRYPTO_MATERIAL_ENROLLEE_NONCE | CY_WPS_CRYPTO_MATERIAL_REGISTRAR_NONCE | CY_WPS_CRYPTO_MATERIAL_ENROLLEE_MAC_ADDRESS)

/* Time related constants */
#define SECONDS                      (1000)
#define MINUTES                      (60 * SECONDS)
#define WPS_TOTAL_MAX_TIME           (120*1000)  /* In milliseconds */
#define DUAL_BAND_WPS_SCAN_TIMEOUT   (5000)      /* In milliseconds */
#define SINGLE_BAND_WPS_SCAN_TIMEOUT (2500)      /* In milliseconds. 4390 takes longest time to complete scan. */
#define DEFAULT_WPS_JOIN_TIMEOUT     (1500)
#define WPS_PBC_OVERLAP_WINDOW       (120*1000) /* In milliseconds */

/******************************************************
 *             Macros
 ******************************************************/

#define IF_TO_WORKSPACE( interface )   ( active_wps_workspaces[0] )     /* STA = 0,  AP = 1 */

/******************************************************
 *             Local Structures
 ******************************************************/

typedef struct
{
    cy_host_workspace_t host_workspace;

    union
    {
        struct
        {
            cy_wps_credential_t*     enrollee_output;
            uint16_t*                enrollee_output_length;
            uint16_t                 stored_credential_count;
            cy_wps_ap_t              ap_list[AP_LIST_SIZE];
            uint8_t                  ap_list_counter;
            cy_wps_scan_handler_t    scan_handler_ptr;
        } enrollee;
        struct
        {
            const cy_wps_credential_t* ap_details;
            whd_mac_t                  authorized_mac_list[AUTHORIZED_MAC_LIST_LENGTH];
        } registrar;
    } stuff;
} cy_wps_workspace_t;

typedef struct
{
    cy_time_t     probe_request_rx_time;
    whd_mac_t     probe_request_mac;
} cy_wps_pbc_overlap_record_t;

typedef void (*cy_wps_pbc_probreq_notify_callback_t)(whd_mac_t mac);

/******************************************************
 *             Static Variables
 ******************************************************/
static whd_scan_result_t scan_result;

/* Active WPS workspaces.
 * Need to have one for each interface, but for now as only STA is supported. added only one */
static cy_wps_agent_t* active_wps_workspaces[ACTIVE_WPS_WORKSPACE_ARRAY_SIZE] = {0};

static       cy_wps_pbc_overlap_record_t pbc_overlap_array[2] = { { 0 } };
static       cy_wps_pbc_overlap_record_t last_pbc_enrollee    = { 0 };
static const whd_event_num_t          wps_events[]         = { WLC_E_PROBREQ_MSG, WLC_E_NONE };
const        uint32_t wps_m2_timeout                       = WPS_PUBLIC_KEY_MESSAGE_TIMEOUT;

static       cy_wps_pbc_probreq_notify_callback_t pbc_probreq_notify_callback;

/******************************************************
 *             Static Function Prototypes
 ******************************************************/

static void           cy_wps_thread                    ( cy_thread_arg_t arg );
static void           cy_wps_whd_scan_result_handler   ( whd_scan_result_t** result_ptr, void* user_data, whd_scan_status_t status );
static void*          cy_wps_softap_event_handler      ( whd_interface_t interface, const whd_event_header_t* event_header, const uint8_t* event_data, /*@returned@*/ void* handler_user_data );
#ifdef WCM_ENABLE_WPS_REGISTRAR
static cy_rslt_t      cy_wps_internal_pbc_overlap_check( const whd_mac_t* mac );
#endif
static void           cy_network_process_wps_eapol_data( /*@only@*/ whd_interface_t interface, whd_buffer_t buffer );
static tlv8_header_t* cy_wps_parse_dot11_tlvs          ( const tlv8_header_t* tlv_buf, uint32_t buflen, dot11_ie_id_t key );

/******************************************************
 *             Function definitions
 ******************************************************/

cy_rslt_t cy_wps_init(cy_wps_agent_t* workspace, const cy_wps_device_detail_t* details, cy_wps_agent_type_t type, whd_interface_t interface )
{
    cy_wps_workspace_t* host_workspace;
    cy_rslt_t result;

    if ( workspace->is_p2p_enrollee != 1 && workspace->is_p2p_registrar != 1)
    {
        memset(workspace, 0, sizeof(cy_wps_agent_t));
    }
    host_workspace = cy_wps_malloc("wps", sizeof(cy_wps_workspace_t));
    if (host_workspace == NULL)
    {
        return CY_RSLT_WPS_OUT_OF_HEAP_SPACE;
    }
    memset(host_workspace, 0, sizeof(cy_wps_workspace_t));
    workspace->wps_host_workspace = host_workspace;

#ifdef RTOS_USE_STATIC_THREAD_STACK
    host_workspace->host_workspace.thread_stack = cy_wps_malloc("wps stack", WPS_THREAD_STACK_SIZE);
    if (host_workspace->host_workspace.thread_stack == NULL)
    {
        cy_wps_free(workspace->wps_host_workspace);
        workspace->wps_host_workspace = NULL;
        return WPS_ERROR_WPS_STACK_MALLOC_FAIL;
    }
    memset( host_workspace->host_workspace.thread_stack, 0, WPS_THREAD_STACK_SIZE );
#else
    host_workspace->host_workspace.thread_stack = NULL;
#endif
    host_workspace->host_workspace.interface = interface;
    workspace->interface      = interface;
    workspace->agent_type     = type;
    workspace->device_details = details;
    workspace->wps_result     = CY_RSLT_WPS_NOT_STARTED;
    if ( workspace->is_p2p_enrollee != 1 )
    {
        workspace->wps_mode           = CY_WPS_PIN_MODE;
        workspace->device_password_id = CY_WPS_DEFAULT_DEVICEPWDID;
    }

    result = cy_rtos_init_queue(&host_workspace->host_workspace.event_queue, 10, sizeof(cy_event_message_t));
    if ( result != CY_RSLT_SUCCESS )
    {
        return result;
    }

    cy_wps_init_workspace( workspace );

    cy_wifimwcore_eapol_register_receive_handler( (cy_wifimwcore_eapol_packet_handler_t) cy_network_process_wps_eapol_data );

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wps_management_set_event_handler( cy_wps_agent_t* workspace, bool enable )
{
    whd_result_t   result;
    uint16_t       event_index;

    /* Add WPS event handler */
    if ( enable == true )
    {
        result = whd_management_set_event_handler(workspace->interface, wps_events, cy_wps_softap_event_handler, workspace, &event_index);
    }
    else
    {
        result = whd_management_set_event_handler( workspace->interface, wps_events, NULL, workspace, &event_index );
    }

    if ( result != WHD_SUCCESS )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Error setting event handler %u\r\n", (unsigned int)result);
    }
    return (cy_rslt_t) result;
}

cy_rslt_t cy_wps_get_result( cy_wps_agent_t* workspace )
{
    switch ( workspace->wps_result)
    {
        case CY_RSLT_WPS_COMPLETE:
            return CY_RSLT_SUCCESS;

        case CY_RSLT_WPS_PBC_OVERLAP:
            return CY_RSLT_WPS_PBC_OVERLAP;

        case CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS:
            return CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS;

        default:
            return CY_RSLT_WPS_ERROR;
    }
}

cy_rslt_t cy_wps_deinit( cy_wps_agent_t* workspace )
{
    cy_host_workspace_t* host_workspace = &((cy_wps_workspace_t*) workspace->wps_host_workspace)->host_workspace;

    cy_wps_deinit_workspace(workspace);

    if ( host_workspace != NULL )
    {
        /* Delete the WPS thread */
        if ( workspace->wps_result != CY_RSLT_WPS_NOT_STARTED )
        {
            if( host_workspace->thread != NULL )
           {
                cy_rtos_terminate_thread( &host_workspace->thread );
            }
        }

        if ( host_workspace->thread_stack != NULL )
        {
            cy_wps_free( host_workspace->thread_stack );
            host_workspace->thread_stack = NULL;
        }

        cy_rtos_deinit_queue( &host_workspace->event_queue );
        cy_wps_free( host_workspace );
        workspace->wps_host_workspace = NULL;
    }
    cy_wifimwcore_eapol_register_receive_handler( NULL );
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wps_start( cy_wps_agent_t* workspace, cy_wps_mode_t mode, const char* password, cy_wps_credential_t* credentials, uint16_t* credential_length )
{
    cy_rslt_t result;
    cy_host_workspace_t* host_workspace = &((cy_wps_workspace_t*) workspace->wps_host_workspace)->host_workspace;

    switch ( mode )
    {
        case CY_WPS_PBC_MODE:
            workspace->device_password_id = CY_WPS_PUSH_BTN_DEVICEPWDID;
            break;

        case CY_WPS_PIN_MODE:
            workspace->device_password_id = CY_WPS_DEFAULT_DEVICEPWDID;
            break;

        default:
            break;
    }

#ifdef WCM_ENABLE_WPS_REGISTRAR
    if ( ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT ) &&
         ( cy_wps_internal_pbc_overlap_check( NULL ) == CY_RSLT_WPS_PBC_OVERLAP ) && ( mode == CY_WPS_PBC_MODE ) )
    {
        return CY_RSLT_WPS_PBC_OVERLAP;
    }
#endif

    result = cy_wps_internal_init( workspace, (uint32_t) workspace->interface, mode, password, credentials, credential_length );
    if ( result == CY_RSLT_SUCCESS )
    {
        if ( cy_rtos_create_thread( &host_workspace->thread, cy_wps_thread, "wps", host_workspace->thread_stack, WPS_THREAD_STACK_SIZE, CY_RTOS_PRIORITY_ABOVENORMAL, (cy_thread_arg_t) workspace ) != CY_RSLT_SUCCESS )
        {
            return result;
        }
    }
    cy_rtos_delay_milliseconds(10); /* Delay required to allow the WPS thread to run */

    return result;
}

#ifdef WCM_ENABLE_P2P
cy_rslt_t cy_p2p_wps_start( cy_wps_agent_t* workspace )
{
    cy_rslt_t result;
    cy_host_workspace_t* host_workspace = &((cy_wps_workspace_t*) workspace->wps_host_workspace)->host_workspace;

    if ( ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT ) &&
         ( cy_wps_internal_pbc_overlap_check( NULL ) == CY_RSLT_WPS_PBC_OVERLAP ) && ( workspace->wps_mode == CY_WPS_PBC_MODE ) )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "PBC overlap detected. Wait and try again later\r\n");
        return CY_RSLT_WPS_PBC_OVERLAP;
    }

    result = cy_rtos_create_thread( &host_workspace->thread, cy_wps_thread, "wps", host_workspace->thread_stack, WPS_THREAD_STACK_SIZE, (cy_thread_priority_t)RTOS_HIGHER_PRIORTIY_THAN(RTOS_DEFAULT_THREAD_PRIORITY), (cy_thread_arg_t) workspace );

    return result;
}
#endif

cy_rslt_t cy_wps_restart( cy_wps_agent_t* workspace )
{
    cy_rtos_get_time( &workspace->start_time );
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wps_reset_registrar( cy_wps_agent_t* workspace, whd_mac_t* mac )
{
    if ( workspace->current_main_stage == CY_WPS_IN_WPS_HANDSHAKE )
    {
        if ( memcmp( mac, &workspace->their_data.mac_address, sizeof(whd_mac_t)) == 0 )
        {
            workspace->current_sub_stage = CY_WPS_EAP_START;
            workspace->current_main_stage = CY_WPS_INITIALISING;
            workspace->available_crypto_material = KDK_REQUIRED_CRYPTO_MATERIAL;
        }
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wps_internal_init( cy_wps_agent_t* workspace, uint32_t interface, cy_wps_mode_t mode, const char* password, cy_wps_credential_t* credentials, uint16_t* credential_length )
{
    cy_wps_workspace_t* host_workspace = (cy_wps_workspace_t*)workspace->wps_host_workspace;

    if( mode == CY_WPS_PBC_MODE )
    {
        workspace->password = "00000000";
    }
    else
    {
        workspace->password = password;
    }

    workspace->wps_mode = (cy_wps_mode_t) mode;

    if (workspace->agent_type == CY_WPS_ENROLLEE_AGENT)
    {
        host_workspace->stuff.enrollee.enrollee_output        = credentials;
        host_workspace->stuff.enrollee.enrollee_output_length = credential_length;
    }
    else
    {
        if (credentials == NULL)
        {
            return CY_RSLT_WPS_BADARG;
        }
        host_workspace->stuff.registrar.ap_details = credentials;
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wps_wait_till_complete( cy_wps_agent_t* workspace )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if ( workspace->wps_result != CY_RSLT_WPS_NOT_STARTED )
    {
        cy_host_workspace_t* host_workspace = &((cy_wps_workspace_t*) workspace->wps_host_workspace)->host_workspace;
        result = cy_rtos_join_thread( &host_workspace->thread );
    }
    return result;
}

int cy_wps_get_stored_credential_count( cy_wps_agent_t* workspace )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*) workspace->wps_host_workspace;
    return host->stuff.enrollee.stored_credential_count;
}

cy_rslt_t cy_wps_abort( cy_wps_agent_t* workspace )
{
    cy_host_workspace_t* host_workspace = &((cy_wps_workspace_t*) workspace->wps_host_workspace)->host_workspace;
    cy_event_message_t   message;
    message.event_type = CY_EVENT_ABORT_REQUESTED;
    message.data.value = 0;
    return (cy_rslt_t) cy_rtos_put_queue( &host_workspace->event_queue, &message, CY_RTOS_NEVER_TIMEOUT, false );
}

void cy_wps_scan_result_handler( whd_scan_result_t* result, void* user_data )
{
    /* Process scan result */
    uint8_t                  keep_record = 0;
    cy_wps_agent_t*             workspace;
    uint8_t*                 data;
    uint32_t                 length;
    tlv8_data_t*             tlv8;
    cy_wps_uuid_t               uuid;
    cy_dsss_parameter_set_ie_t* dsie = NULL;
    cy_ht_operation_ie_t*       ht_operation_ie = NULL;

    whd_scan_result_t*       bss_info;

    if ( ( result == NULL ) || ( user_data == NULL ) )
    {
        goto exit;
    }



    workspace = (cy_wps_agent_t*) user_data;
    bss_info = result;

    length = bss_info->ie_len;

    data  =  (uint8_t*) bss_info->ie_ptr;

    do
    {
        if ( length == 0 )
        {
            break;
        }
        /* Scan result for vendor specific IE */
        tlv8 = tlv_find_tlv8( data, length, DOT11_IE_ID_VENDOR_SPECIFIC );
        if ( tlv8 == NULL )
        {
            break;
        }

        /* Check if the TLV we've found is outside the bounds of the scan result length. i.e. Something is bad */
        if (( (uint32_t)( (uint8_t*) tlv8 - data ) + tlv8->length + sizeof(tlv8_header_t)) > length)
        {
            goto exit;
        }

        length -= (uint32_t)( (uint8_t*) tlv8 - data ) + tlv8->length + sizeof(tlv8_header_t);
        data = (uint8_t*) tlv8 + tlv8->length + sizeof(tlv8_header_t);

        /* Verify extension is WPS extension */
        if ( memcmp( tlv8->data, WPS_OUI, 3 ) == 0 && tlv8->data[3] == WPS_OUI_TYPE )
        {
            tlv16_data_t* tlv16;

            /* Look for pwd ID */
            tlv16_uint16_t* pwd_id = (tlv16_uint16_t*) tlv_find_tlv16( &tlv8->data[4], (uint32_t)( tlv8->length - 4 ), WPS_ID_DEVICE_PWD_ID );
            if ( workspace->wps_mode == CY_WPS_PIN_MODE ||
                 ( pwd_id != NULL && cy_hton16( pwd_id->data ) == CY_WPS_PUSH_BTN_DEVICEPWDID ) ) /* XXX needs to change if NFC is in use */
            {
                keep_record = 1;

                /* Look for the AP's UUID */
                tlv16 = tlv_find_tlv16( &tlv8->data[4], (uint32_t)( tlv8->length - 4 ), WPS_ID_UUID_E );
                if ( ( tlv16 == NULL ) || ( cy_hton16(tlv16->length) != WPS_UUID_LENGTH ) )
                {
                    memset( &uuid, 0, sizeof( cy_wps_uuid_t ) );
                }
                else
                {
                    memcpy( &uuid, tlv16->data, sizeof( cy_wps_uuid_t ) );
                }

                /* Check if the Selected Registrar attribute is missing or false. Only applicable to PBC mode */
                if ( workspace->wps_mode == CY_WPS_PBC_MODE )
                {
                    /* Check if the registrar is using PBC mode */
                    if ( ( pwd_id != NULL && cy_hton16( pwd_id->data ) == CY_WPS_PUSH_BTN_DEVICEPWDID ) )
                    {
                        tlv16 = tlv_find_tlv16( &tlv8->data[4], (uint32_t)( tlv8->length - 4 ), WPS_ID_SEL_REGISTRAR );
                        if ( tlv16 != NULL )
                        {
                            /* Check that selected registrar is asserted */
                            if (tlv16->data[0] == 0)
                            {
                                keep_record = 0;
                            }
                        }
                        else
                        {
                            keep_record = 0;
                        }
                    }
                    else
                    {
                        keep_record = 0;
                    }
                }
            }
        }
    } while ( tlv8 != NULL );

    if ( keep_record == 1 )
    {
        /* Adjust the channel */
        data   = (uint8_t*) bss_info->ie_ptr;
        uint32_t ie_len = result->ie_len;
        length = CY_WPS_HOST_READ_32((uint32_t *)&ie_len);
        /* In 2.4 GHz the radio firmware may report off channel probe responses. Parse the response to check if it is on or off the AP operating channel. */
        dsie =  (cy_dsss_parameter_set_ie_t*) cy_wps_parse_dot11_tlvs( (tlv8_header_t*)data, length, DOT11_IE_ID_DSSS_PARAMETER_SET );
        if ( ( dsie != NULL ) && ( dsie->length == DSSS_PARAMETER_SET_LENGTH ) )
        {
            result->channel = dsie->current_channel;
        }

        /* In 5GHz the DS Parameter Set element may not be present. If it's not present then for an 802.11a AP we use the channel from the chanspec, since
         * it will be a 20 MHz wide channel. If it's an 802.11n AP then we need to examine the HT operations element to find the primary 20 MHz channel
         * since the chanspec may report the center frequency if it's an 802.11n 40MHz or wider channel, which is not the same as the 20 MHz channel that the
         * beacons are on. */
        if ( dsie == NULL )
        {
            /* Find the primary channel */
            ht_operation_ie = (cy_ht_operation_ie_t*)cy_wps_parse_dot11_tlvs( (tlv8_header_t*)data, length, DOT11_IE_ID_HT_OPERATION );
            if ( ( ht_operation_ie != NULL ) && ( ht_operation_ie->length == HT_OPERATION_IE_LENGTH ) )
            {
                result->channel =  ht_operation_ie->primary_channel;
            }
        }

        cy_wps_host_store_ap( workspace->wps_host_workspace, result, &uuid );
    }

exit:
    return;
}

#ifdef WCM_ENABLE_WPS_REGISTRAR
cy_rslt_t cy_wps_set_directed_wps_target( cy_wps_agent_t* workspace, cy_wps_ap_t* ap, uint32_t maximum_join_attempts )
{
    workspace->directed_wps_max_attempts = maximum_join_attempts;
    workspace->ap = ap;

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_wps_internal_pbc_overlap_check( const whd_mac_t* mac )
{
    if ( cy_wps_pbc_overlap_check( mac ) != CY_RSLT_SUCCESS )
    {
        return CY_RSLT_WPS_PBC_OVERLAP;
    }
    return CY_RSLT_SUCCESS;
}
#endif

static void cy_wps_thread( cy_thread_arg_t arg )
{
    cy_wps_thread_main( arg );
    cy_rtos_exit_thread();
}

void cy_wps_thread_main( cy_thread_arg_t arg )
{
    cy_time_t             current_time;
    cy_rslt_t          result;
    cy_event_message_t  message;
    cy_wps_agent_t*          workspace = (cy_wps_agent_t*)arg;
    cy_wps_workspace_t* host      = (cy_wps_workspace_t*) workspace->wps_host_workspace;
    cy_rslt_t          wps_result;
    workspace->wps_result = CY_RSLT_WPS_IN_PROGRESS;

    /* Now that our queue is initialized we can flag the workspace as active */
    IF_TO_WORKSPACE( workspace->interface ) = workspace;


    cy_wps_prepare_workspace_crypto( workspace );

    /* Start 120 second timer */
    cy_rtos_get_time( &workspace->start_time );


    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Starting WPS Enrollee\r\n");
    cy_wps_enrollee_start( workspace, workspace->interface );

    while ( workspace->wps_result == CY_RSLT_WPS_IN_PROGRESS )
    {
        uint32_t     time_to_wait;
        bool         waiting_for_event = false;

        cy_rtos_get_time(&current_time);

        if (( current_time - workspace->start_time ) >= 2 * MINUTES )
        {
            workspace->wps_result = CY_RSLT_WPS_TIMEOUT;
            continue;
        }

        time_to_wait = ( 2 * MINUTES ) - ( current_time - workspace->start_time );
        if ( host->host_workspace.timer_timeout != 0 )
        {
            waiting_for_event = true;
            time_to_wait = MIN( time_to_wait, host->host_workspace.timer_timeout - (current_time - host->host_workspace.timer_reference));
        }

        if ( cy_rtos_get_queue( &host->host_workspace.event_queue, &message, time_to_wait, false ) != CY_RSLT_SUCCESS )
        {
            /* Create a timeout message */
            message.event_type = CY_EVENT_TIMER_TIMEOUT;
            message.data.value = 0;
        }

        /* Process the message */
        result = cy_wps_process_event( workspace, &message );
        if ( result != CY_RSLT_SUCCESS )
        {
            if ( result == CY_RSLT_WPS_ATTEMPTED_EXTERNAL_REGISTRAR_DISCOVERY )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO,"Client attempted external registrar discovery\r\n");
            }
            else
            {
                if ( waiting_for_event == true )
                {
                    int32_t time_left;
                    cy_rtos_get_time( &current_time );
                    time_left = MAX( ( ( 2 * MINUTES ) - ( current_time - workspace->start_time ) )/1000, 0);
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "WPS Procedure failed. Restarting with %li seconds left\r\n", (long)time_left);
                    REFERENCE_DEBUG_ONLY_VARIABLE( time_left );
                }
            }

            /* Reset the agent type if we were in reverse registrar mode */
            if (workspace->in_reverse_registrar_mode != 0)
            {
                workspace->agent_type = CY_WPS_REGISTRAR_AGENT;
            }

            if ( workspace->agent_type == CY_WPS_ENROLLEE_AGENT )
            {
                whd_wifi_leave( workspace->interface );
            }

            cy_wps_reset_workspace( workspace, workspace->interface );
        }
    }

    /* Remove workspace from list of active workspaces */
    IF_TO_WORKSPACE( workspace->interface ) = NULL;

    /* Print result (if enabled) */
    if ( workspace->wps_result == CY_RSLT_WPS_COMPLETE )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "WPS completed successfully\r\n");
    }
    else if ( workspace->wps_result == CY_RSLT_WPS_PBC_OVERLAP )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "PBC overlap detected - wait and try again\r\n");
    }
    else if ( workspace->wps_result == CY_RSLT_WPS_ABORTED )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "WPS aborted\r\n");
    }
    else
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "WPS timed out\r\n");
    }

    /* Indicate result via the callbacks */
    wps_result = workspace->wps_result; /* Need to make a copy because this thread can change the value before the application can check the result */
    if ( workspace->cy_wps_result_callback != NULL )
    {
        workspace->cy_wps_result_callback( &wps_result );
    }
    if ( workspace->cy_wps_internal_result_callback != NULL )
    {
        workspace->cy_wps_internal_result_callback( &wps_result );
    }

    
    /* De-init the workspace */
    cy_wps_deinit_workspace( workspace );
    whd_wifi_leave( workspace->interface );


    /* Clean up left over messages in the event queue */
    while ( cy_rtos_get_queue( &host->host_workspace.event_queue, &message, 0, false ) == CY_RSLT_SUCCESS )
    {
        if (message.event_type == CY_EVENT_EAPOL_PACKET_RECEIVED)
        {
            whd_buffer_release( workspace->interface->whd_driver, message.data.packet, WHD_NETWORK_RX);
        }
    }
}

static void cy_network_process_wps_eapol_data( /*@only@*/ whd_interface_t interface, whd_buffer_t buffer )
{
    cy_wps_agent_t* workspace = IF_TO_WORKSPACE( interface );
    if ( workspace != NULL )
    {
        /* Don't queue the packet unless the WPS thread is running */
        if( workspace->wps_result == CY_RSLT_WPS_IN_PROGRESS)
        {
            cy_host_workspace_t* host = &((cy_wps_workspace_t*) workspace->wps_host_workspace)->host_workspace;
            cy_event_message_t message;
            message.event_type = CY_EVENT_EAPOL_PACKET_RECEIVED;
            message.data.packet = buffer;
            if ( cy_rtos_put_queue( &host->event_queue, &message, 0, false ) != CY_RSLT_SUCCESS )
            {
                whd_buffer_release( workspace->interface->whd_driver, buffer, WHD_NETWORK_RX );
            }
        }
        else
        {
            whd_buffer_release( workspace->interface->whd_driver, buffer, WHD_NETWORK_RX );
        }
    }
    else
    {
        whd_buffer_release( interface->whd_driver, buffer, WHD_NETWORK_RX );
    }
}

#ifdef WCM_ENABLE_WPS_REGISTRAR
int cy_wps_validate_pin_checksum( const char* str )
{
    unsigned long int PIN;
    unsigned long int accum = 0;

    PIN = (unsigned long int) atoi( str );

    accum += 3 * ((PIN / 10000000) % 10);
    accum += 1 * ((PIN / 1000000) % 10);
    accum += 3 * ((PIN / 100000) % 10);
    accum += 1 * ((PIN / 10000) % 10);
    accum += 3 * ((PIN / 1000) % 10);
    accum += 1 * ((PIN / 100) % 10);
    accum += 3 * ((PIN / 10) % 10);
    accum += 1 * ((PIN / 1) % 10);

    return (0 == (accum % 10));
}
#endif


/******************************************************
 *               WPS Host API Definitions
 ******************************************************/


cy_rslt_t cy_wps_host_join( void* workspace, cy_wps_ap_t* ap, whd_interface_t interface )
{
    cy_host_workspace_t* host = &((cy_wps_workspace_t*) workspace)->host_workspace;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Joining '%.*s'\r\n", ap->scan_result.SSID.length, ap->scan_result.SSID.value);

    uint8_t attempts = 0;
    cy_rslt_t ret;

    do
    {
        ++attempts;
        ret = (cy_rslt_t) whd_wifi_join_specific(interface, &ap->scan_result, NULL, 0 );
        if (ret != CY_RSLT_SUCCESS)
        {
            continue;
        }
    } while ( ret != CY_RSLT_SUCCESS && attempts < 2 );

    if ( ret != CY_RSLT_SUCCESS )
    {
        if ( whd_wifi_is_ready_to_transceive( interface ) != WHD_SUCCESS )
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WPS join failed on interface %u\r\n", (unsigned int)interface);
            cy_host_start_timer( host, 100 );
            return CY_RSLT_WPS_ERROR_JOIN_FAILED;
        }
    }

    cy_event_message_t message;
    message.event_type = CY_WPS_EVENT_ENROLLEE_ASSOCIATED;
    message.data.value = 0;
    cy_rtos_put_queue(&host->event_queue, &message, 0, false);

    return CY_RSLT_SUCCESS;
}

void cy_wps_host_add_vendor_ie( uint32_t interface, void* data, uint16_t data_length, uint32_t packet_mask )
{
    whd_wifi_manage_custom_ie( ( whd_interface_t )interface, WHD_ADD_CUSTOM_IE, (uint8_t*) WPS_OUI, WPS_OUI_TYPE, data, data_length, packet_mask );
}

void cy_wps_host_remove_vendor_ie( uint32_t interface, void* data, uint16_t data_length, uint32_t packet_mask )
{
    whd_wifi_manage_custom_ie( ( whd_interface_t )interface, WHD_REMOVE_CUSTOM_IE, (uint8_t*) WPS_OUI, WPS_OUI_TYPE, data, data_length, packet_mask );
}

/*
 * NOTE: This function is called from the context of the WICED thread and so should not consume
 *       much stack space and must not printf().
 */
cy_wps_ap_t* cy_wps_host_store_ap( void* workspace, whd_scan_result_t* scan_result, cy_wps_uuid_t* uuid )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*)workspace;

    if (host->stuff.enrollee.ap_list_counter < AP_LIST_SIZE)
    {
        int a;
        cy_wps_ap_t* ap;

        /* Check if this AP has already been added */
        for (a = 0; a < host->stuff.enrollee.ap_list_counter; ++a)
        {
            ap = &host->stuff.enrollee.ap_list[a];
            if (memcmp(&ap->scan_result.BSSID, &scan_result->BSSID, sizeof(whd_mac_t)) == 0)
            {
                return NULL;
            }
        }

        /* Add to AP list */
        ap = &host->stuff.enrollee.ap_list[host->stuff.enrollee.ap_list_counter++];

        /* Save SSID, BSSID, channel, security, band and UUID */
        ap->scan_result.SSID.length = scan_result->SSID.length;
        memcpy( ap->scan_result.SSID.value, scan_result->SSID.value, scan_result->SSID.length );
        memcpy( &ap->scan_result.BSSID, &scan_result->BSSID, sizeof(ap->scan_result.BSSID) );
        ap->scan_result.channel  = scan_result->channel;
        if( scan_result->security == WHD_SECURITY_OPEN )
        {
            ap->scan_result.security = WHD_SECURITY_OPEN;
        }
        else
        {
            ap->scan_result.security = WHD_SECURITY_WPS_SECURE;
        }
        ap->scan_result.band     = scan_result->band;
        memcpy( &ap->uuid, uuid, sizeof( cy_wps_uuid_t ) );

        return ap;
    }

    return NULL;
}

cy_rslt_t cy_wps_enrollee_pbc_overlap_check( cy_wps_agent_t* workspace )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*)workspace->wps_host_workspace;
    uint16_t ap_list_size;

    ap_list_size = cy_wps_host_get_ap_list_size( workspace->wps_host_workspace );
    if ( ap_list_size > 1 )
    {
        /* If the enrollee only supports one band then PBC overlap is occurring if the AP list is > 1 */
        if ( workspace->band_list.number_of_bands < 2 )
        {
            return CY_RSLT_WPS_PBC_OVERLAP;
        }
        /* If the enrollee supports two bands and the AP list size is two then check if the
         * AP is dual band by checking that the UUIDs in the AP entries are the same.
         */
        if ( ap_list_size == 2)
        {
            if (memcmp( &host->stuff.enrollee.ap_list[0].uuid, &host->stuff.enrollee.ap_list[1].uuid, sizeof( cy_wps_uuid_t ) ) == 0)
            {
                return CY_RSLT_SUCCESS;
            }
        }
    }
    else
    {
        return CY_RSLT_SUCCESS;
    }

    return CY_RSLT_WPS_PBC_OVERLAP;
}


cy_wps_ap_t* cy_wps_host_retrieve_ap( void* workspace )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*)workspace;
    if ( host->stuff.enrollee.ap_list_counter != 0 )
    {
        return &host->stuff.enrollee.ap_list[--host->stuff.enrollee.ap_list_counter];
    }

    return NULL;
}

uint16_t cy_wps_host_get_ap_list_size( void* workspace )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*)workspace;
    return host->stuff.enrollee.ap_list_counter;
}

void cy_wps_host_store_credential( void* workspace, cy_wps_internal_credential_t* credential )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*)workspace;
    cy_wps_credential_t* temp;
    uint8_t credential_length = 0, ssid_length = 0;

    /* Store credentials if we have room */
    if ( host->stuff.enrollee.stored_credential_count < *(host->stuff.enrollee.enrollee_output_length))
    {
        temp = &host->stuff.enrollee.enrollee_output[host->stuff.enrollee.stored_credential_count];
        memset( temp, 0, sizeof(cy_wps_credential_t) );

        /* Copy only CY_WPS_PASSPHRASE_LENGTH when credential->network_key_length is greater than CY_WPS_PASSPHRASE_LENGTH */
        credential_length = (credential->network_key_length > CY_WPS_PASSPHRASE_LENGTH) ? CY_WPS_PASSPHRASE_LENGTH : credential->network_key_length;
        memcpy( temp->passphrase, credential->network_key, credential_length );
        temp->passphrase[credential_length] = 0;

        /* Copy only CY_WPS_SSID_LENGTH when credential->ssid_length is greater than CY_WPS_SSID_LENGTH */
        ssid_length = (credential->ssid_length > CY_WPS_SSID_LENGTH) ? CY_WPS_SSID_LENGTH : credential->ssid_length;
        memcpy( temp->ssid, credential->ssid, ssid_length );
        temp->ssid[ssid_length] = 0;

        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO,"Storing credentials for %s\r\n", temp->ssid);
        switch ( credential->encryption_type )
        {
            case CY_WPS_MIXED_ENCRYPTION:
            case CY_WPS_AES_ENCRYPTION:
                if ( credential->authentication_type == CY_WPS_WPA2_PSK_AUTHENTICATION )
                {
                    temp->security = WHD_SECURITY_WPA2_AES_PSK;
                }
                else if ( credential->authentication_type == CY_WPS_WPA_PSK_AUTHENTICATION )
                {
                    temp->security = WHD_SECURITY_WPA_AES_PSK;
                }
                else
                {
                    temp->security = WHD_SECURITY_WPA2_MIXED_PSK;
                }
                break;
            case CY_WPS_TKIP_ENCRYPTION:
                if ( ( credential->authentication_type == CY_WPS_WPA2_PSK_AUTHENTICATION ) ||
                     ( credential->authentication_type == CY_WPS_WPA2_WPA_PSK_MIXED_AUTHENTICATION ) )
                {
                    temp->security = WHD_SECURITY_WPA2_TKIP_PSK;
                }
                else if ( credential->authentication_type == CY_WPS_WPA_PSK_AUTHENTICATION )
                {
                    temp->security = WHD_SECURITY_WPA_TKIP_PSK;
                }
                break;
            case CY_WPS_WEP_ENCRYPTION:
                temp->security = WHD_SECURITY_WEP_PSK;
                break;
            case CY_WPS_NO_ENCRYPTION:
            default:
                temp->security = WHD_SECURITY_OPEN;
                break;
        }

        ++host->stuff.enrollee.stored_credential_count;
    }
}

#ifdef WCM_ENABLE_WPS_REGISTRAR
void cy_wps_host_retrieve_credential( void* workspace, cy_wps_internal_credential_t* credential )
{
    cy_wps_workspace_t*  host       = (cy_wps_workspace_t*) workspace;
    const cy_wps_credential_t* ap_details = host->stuff.registrar.ap_details;

    memcpy( credential->ssid, ap_details->ssid, sizeof( credential->ssid ) );
    credential->network_key_length = (uint8_t)strlen((char*)ap_details->passphrase);
    memcpy( credential->network_key, ap_details->passphrase, sizeof( credential->network_key ) );

    switch ( ap_details->security )
    {
        default:
        case WHD_SECURITY_OPEN:
            credential->encryption_type     = CY_WPS_NO_ENCRYPTION;
            credential->authentication_type = CY_WPS_OPEN_AUTHENTICATION;
            break;
        case WHD_SECURITY_WEP_PSK:
            credential->encryption_type     = CY_WPS_WEP_ENCRYPTION;
            credential->authentication_type = CY_WPS_OPEN_AUTHENTICATION;
            break;
        case WHD_SECURITY_WPA_TKIP_PSK:
            credential->encryption_type     = CY_WPS_TKIP_ENCRYPTION;
            credential->authentication_type = CY_WPS_WPA_PSK_AUTHENTICATION;
            break;
        case WHD_SECURITY_WPA_AES_PSK:
            credential->encryption_type     = CY_WPS_AES_ENCRYPTION;
            credential->authentication_type = CY_WPS_WPA_PSK_AUTHENTICATION;
            break;
        case WHD_SECURITY_WPA2_AES_PSK:
            credential->encryption_type     = CY_WPS_AES_ENCRYPTION;
            credential->authentication_type = CY_WPS_WPA2_PSK_AUTHENTICATION;
            break;
        case WHD_SECURITY_WPA2_TKIP_PSK:
            credential->encryption_type     = CY_WPS_TKIP_ENCRYPTION;
            credential->authentication_type = CY_WPS_WPA2_PSK_AUTHENTICATION;
            break;
        case WHD_SECURITY_WPA2_MIXED_PSK:
            credential->encryption_type     = CY_WPS_MIXED_ENCRYPTION;
            credential->authentication_type = CY_WPS_WPA2_PSK_AUTHENTICATION;
            break;
    }
}
#endif

static void cy_wps_whd_scan_result_handler( whd_scan_result_t** result_ptr, void* user_data, whd_scan_status_t status )
{
    uint8_t a;

    /* Verify the workspace is still valid */
    for ( a = 0; a < ACTIVE_WPS_WORKSPACE_ARRAY_SIZE; ++a )
    {
        if (active_wps_workspaces[a] == user_data)
        {
            /* Get the host workspace now that we know the workspace is still valid */
            cy_wps_workspace_t* host = (cy_wps_workspace_t*) ( (cy_wps_agent_t*) ( user_data ) )->wps_host_workspace;

            /* Check if scan is complete */
            if ( result_ptr == NULL )
            {
                cy_event_message_t message;
                message.event_type = CY_WPS_EVENT_DISCOVER_COMPLETE;
                message.data.value = 0;
                cy_rtos_put_queue( &host->host_workspace.event_queue, &message, 0, false );
            }
            else if ( status == WHD_SCAN_INCOMPLETE )
            {
                host->stuff.enrollee.scan_handler_ptr( *result_ptr, user_data );
            }

            break;
        }
    }
}

void cy_wps_host_scan( cy_wps_agent_t* workspace, cy_wps_scan_handler_t result_handler, whd_interface_t interface )
{
    whd_buffer_t buffer;
    whd_buffer_t response;
    cy_wps_workspace_t* host = (cy_wps_workspace_t*) (workspace->wps_host_workspace);
    host->stuff.enrollee.ap_list_counter  = 0;
    host->stuff.enrollee.scan_handler_ptr = result_handler;
    uint8_t attempts = 0;
    cy_rslt_t ret;
    uint16_t chlist[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,0 };
    whd_scan_extended_params_t extparam = { 2, 40, 110, 50 };
    uint32_t scan_timeout = DUAL_BAND_WPS_SCAN_TIMEOUT; /* Default scan timeout to cover scans of both 2.4GHz and 5 GHz bands */

    /* Check if only one band is supported and adjust the scan timeout accordingly. Note that if the scan timeout is shorter than the time required to scan
     * all the channels then the WPS enrollee will go into a scan request/abort loop which causes various problems including lockup for SPI builds. */
    memset( &workspace->band_list, 0, sizeof( whd_band_list_t ) );

    whd_cdc_get_ioctl_buffer(host->host_workspace.interface->whd_driver, &buffer, sizeof(whd_band_list_t) );

    whd_cdc_send_ioctl(host->host_workspace.interface,  CDC_GET, WLC_GET_BANDLIST, buffer, &response);

    memcpy(&workspace->band_list, (uint32_t*) whd_buffer_get_current_piece_data_pointer(host->host_workspace.interface->whd_driver, response), sizeof(whd_band_list_t));
    whd_buffer_release(host->host_workspace.interface->whd_driver, response, WHD_NETWORK_RX);

    if ( workspace->band_list.number_of_bands == 1 )
    {
        if ( workspace->band_list.number_of_bands == 1 )
        {
            scan_timeout = SINGLE_BAND_WPS_SCAN_TIMEOUT;
        }
    }
    memset(&scan_result, 0, sizeof(whd_scan_result_t) );

    do
    {
        ++attempts;
        ret = (cy_rslt_t) whd_wifi_scan(interface, WHD_SCAN_TYPE_ACTIVE, WHD_BSS_TYPE_INFRASTRUCTURE, 0, 0, chlist, &extparam, cy_wps_whd_scan_result_handler, &scan_result, workspace);
    } while ( ret != CY_RSLT_SUCCESS && attempts < 5 );

    if (ret != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "WPS scan failure\r\n");
        cy_host_start_timer( host, 100 );
    }
    else
    {
        cy_host_start_timer( host, scan_timeout );
    }
}

void cy_wps_host_get_authorized_macs( void* workspace, whd_mac_t** mac_list, uint8_t* mac_list_length )
{
    cy_wps_workspace_t* host = (cy_wps_workspace_t*)workspace;
    *mac_list = &host->stuff.registrar.authorized_mac_list[0];
    *mac_list_length = 1;
}

static void* cy_wps_softap_event_handler( whd_interface_t interface, const whd_event_header_t* event_header, const uint8_t* event_data, /*@returned@*/ void* handler_user_data )
{
    const uint8_t* data;
    uint32_t       length;
    tlv8_data_t*   tlv8;
    cy_wps_agent_t*   workspace;

    if ( ( event_header == NULL ) || ( event_data == NULL ) || ( handler_user_data == NULL ) )
    {
        return handler_user_data;
    }
    workspace = (cy_wps_agent_t*)handler_user_data;
    length    = event_header->datalen;

    if ( ( length <= 24 ) || ( workspace == NULL ) )
    {
        return handler_user_data;
    }
    length -= 24; /* XXX length of management frame MAC header */
    data = event_data + 24;

    switch ( event_header->event_type )
    {
        case WLC_E_PROBREQ_MSG:
            if ( event_header->status == WLC_E_STATUS_SUCCESS )
            {
                do
                {
                    /* Scan result for vendor specific IE */
                    tlv8 = tlv_find_tlv8( data, length, DOT11_IE_ID_VENDOR_SPECIFIC );
                    if ( tlv8 == NULL )
                    {
                        break;
                    }
                    /* Check if the TLV we've found is outside the bounds of the scan result length. i.e. Something is bad */
                    if ((( (uint8_t*) tlv8 - data ) + tlv8->length + sizeof(tlv8_header_t)) > length)
                    {
                        break;
                    }

                    length -= ( (uint8_t*) tlv8 - data ) + tlv8->length + sizeof(tlv8_header_t);
                    data = (uint8_t*) tlv8 + tlv8->length + sizeof(tlv8_header_t);

                    /* Verify extension is WPS extension */
                    if ( memcmp( tlv8->data, WPS_OUI, 3 ) == 0 && tlv8->data[3] == WPS_OUI_TYPE )
                    {
                        /* Look for pwd ID, check if it is the same mode that we are */
                        tlv16_uint16_t* pwd_id = (tlv16_uint16_t*) tlv_find_tlv16( &tlv8->data[4], tlv8->length - 4, WPS_ID_DEVICE_PWD_ID );
                        if ( pwd_id != NULL && cy_hton16( pwd_id->data ) == CY_WPS_PUSH_BTN_DEVICEPWDID )
                        {
                            cy_wps_update_pbc_overlap_array( workspace, (whd_mac_t*)(event_data + 10) );
                        }
                    }
                } while ( ( tlv8 != NULL ) && ( length > 0 ) );
            }
            break;

        default:
            break;
    }

    return handler_user_data;
}

/* Note that this function may be called from the Wiced thread context */
void cy_wps_update_pbc_overlap_array(cy_wps_agent_t* workspace, const whd_mac_t* mac_address )
{
    int i;
    cy_time_t              rx_time;
    cy_event_message_t   message;
    cy_host_workspace_t* host = &((cy_wps_workspace_t*)workspace->wps_host_workspace)->host_workspace;
    whd_mac_t             mac;

    cy_rtos_get_time(&rx_time);
    memcpy( &mac, mac_address, sizeof( whd_mac_t ) );

    /* If the MAC address is the same as the last enrollee give it 5 seconds to stop asserting PBC mode */
    if ( ( memcmp( &mac, (char*) &last_pbc_enrollee.probe_request_mac, sizeof(whd_mac_t) ) == 0 ) &&
         ( ( rx_time - last_pbc_enrollee.probe_request_rx_time ) <= WPS_PBC_MODE_ASSERTION_DELAY ) )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Received probe request asserting PBC mode from last enrollee\r\n");
        goto return_without_notify;
    }

    /* If the MAC address is already in the array update the rx time */
    for ( i = 0; i < 2; i++ )
    {
        if (memcmp(&mac, (char*)&pbc_overlap_array[i].probe_request_mac, sizeof(whd_mac_t)) == 0)
        {
            /* Notify the application that a probe request has been received, but only do this every few seconds. */
            if ( ( rx_time - pbc_overlap_array[i].probe_request_rx_time ) <= WPS_PBC_NOTIFICATION_DELAY )
            {
                pbc_overlap_array[i].probe_request_rx_time = rx_time;
                goto return_without_notify;
            }
            else
            {
                pbc_overlap_array[i].probe_request_rx_time = rx_time;
                goto return_with_notify;
            }
        }
    }

    /* If this is a new MAC address replace oldest existing entry with this MAC address */
    if ( pbc_overlap_array[0].probe_request_rx_time == 0 ) /* Initial condition for array record 0 */
    {
        memcpy((char*)&pbc_overlap_array[0].probe_request_mac, &mac, sizeof(whd_mac_t));
        pbc_overlap_array[0].probe_request_rx_time = rx_time;
    }
    else if ( pbc_overlap_array[1].probe_request_rx_time == 0 ) /* Initial condition for array record 1 */
    {
        memcpy((char*)&pbc_overlap_array[1].probe_request_mac, &mac, sizeof(whd_mac_t));
        pbc_overlap_array[1].probe_request_rx_time = rx_time;
    }
    else if ( pbc_overlap_array[0].probe_request_rx_time <= pbc_overlap_array[1].probe_request_rx_time )
    {
        memcpy((char*)&pbc_overlap_array[0].probe_request_mac, &mac, sizeof(whd_mac_t));
        pbc_overlap_array[0].probe_request_rx_time = rx_time;
    }
    else
    {
        memcpy((char*)&pbc_overlap_array[1].probe_request_mac, &mac, sizeof(whd_mac_t));
    }

return_with_notify:
    /* This event is only so that the user of the SoftAP (not group owner) can be notified of PBC overlap during the Registrar WPS handshake.
     * If the event queue is full we can safely skip this event so the wait time is zero.
     */
    if ( ( workspace->is_p2p_enrollee == 0 ) && ( workspace->is_p2p_registrar == 0 ) &&
            ( workspace->wps_result == CY_RSLT_WPS_IN_PROGRESS ) && ( workspace->agent_type == CY_WPS_REGISTRAR_AGENT ) &&
            ( workspace->wps_mode == CY_WPS_PBC_MODE ))
    {
        message.event_type  = CY_WPS_EVENT_PBC_OVERLAP_NOTIFY_USER;
        message.data.packet = &mac;
        if (cy_rtos_put_queue(&host->event_queue, &message, 0, false) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "No room in the WPS event queue\r\n");
        }
    }

return_without_notify:
    return;
}

#ifdef WCM_ENABLE_WPS_REGISTRAR
void wps_register_pbc_probreq_callback( cy_wps_pbc_probreq_notify_callback_t pbc_probreq_callback )
{
    pbc_probreq_notify_callback = pbc_probreq_callback;
}
#endif

void cy_wps_pbc_overlap_array_notify( const whd_mac_t* mac )
{
    if ( pbc_probreq_notify_callback != NULL )
    {
        whd_mac_t probreq_mac;
        memcpy(&probreq_mac, mac, sizeof(whd_mac_t));
        pbc_probreq_notify_callback(probreq_mac);
    }
}

void cy_wps_clear_pbc_overlap_array( void )
{
    memset( (char*)pbc_overlap_array, 0, sizeof(pbc_overlap_array) );
}

cy_rslt_t cy_wps_pbc_overlap_check(const whd_mac_t* mac )
{
    cy_time_t detection_window_start;
    cy_time_t time_now;

    /* Detection window starts at 0, or 120 seconds prior to now if the host has been up for more than 120 seconds */
    detection_window_start = 0;
    cy_rtos_get_time(&time_now);
    if ( time_now > WPS_PBC_OVERLAP_WINDOW )
    {
        detection_window_start = time_now - WPS_PBC_OVERLAP_WINDOW;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "PBC overlap detection window start %u\r\n", (unsigned int)detection_window_start);

    /* This tests the case where M1 has arrived and there may or may not be a probe request from the same enrollee
     * in the detection array, but there is a probe request from another enrollee.
     */
    if ( mac != NULL )
    {
        if ( ( memcmp( mac, (char*) &pbc_overlap_array[0].probe_request_mac, sizeof(whd_mac_t) ) != 0 ) &&
             ( pbc_overlap_array[0].probe_request_rx_time > detection_window_start ) )
        {
            return CY_RSLT_WPS_PBC_OVERLAP;
        }

        if ( ( memcmp( mac, (char*) &pbc_overlap_array[1].probe_request_mac, sizeof(whd_mac_t) ) != 0 ) &&
             ( pbc_overlap_array[1].probe_request_rx_time > detection_window_start ) )
        {
            return CY_RSLT_WPS_PBC_OVERLAP;
        }
        else
        {
            return CY_RSLT_SUCCESS;
        }
    }
    else
    {
        /* This tests the simple case where two enrollees have been probing during the detection window */
        if ( ( pbc_overlap_array[0].probe_request_rx_time > detection_window_start ) &&
             ( pbc_overlap_array[1].probe_request_rx_time > detection_window_start ) )
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "PBC overlap array entry 0 %u\r\n", (unsigned int)pbc_overlap_array[0].probe_request_rx_time);
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "PBC overlap array entry 1 %u\r\n", (unsigned int)pbc_overlap_array[1].probe_request_rx_time);
            return CY_RSLT_WPS_PBC_OVERLAP;
        }
    }

    return CY_RSLT_SUCCESS;
}

void cy_wps_record_last_pbc_enrollee( const whd_mac_t* mac )
{
    memcpy((char*)&last_pbc_enrollee.probe_request_mac, mac, sizeof(whd_mac_t));
    cy_rtos_get_time( &last_pbc_enrollee.probe_request_rx_time );
}

void cy_wps_register_result_callback( cy_wps_agent_t* workspace, void (*wps_result_callback)(cy_rslt_t*) )
{
    workspace->cy_wps_result_callback = wps_result_callback;
}

void cy_wps_register_internal_result_callback( cy_wps_agent_t* workspace, void (*wps_internal_result_callback)(cy_rslt_t*) )
{
    workspace->cy_wps_internal_result_callback = wps_internal_result_callback;
}

/* Note that there is an identical function in wwd_wifi.c which can't be used currently because making it non-static causes build errors related to the tlv8 structs. */
static tlv8_header_t* cy_wps_parse_dot11_tlvs( const tlv8_header_t* tlv_buf, uint32_t buflen, dot11_ie_id_t key )
{

    return (tlv8_header_t*) tlv_find_tlv8( (const uint8_t*) tlv_buf, buflen, key );
}
