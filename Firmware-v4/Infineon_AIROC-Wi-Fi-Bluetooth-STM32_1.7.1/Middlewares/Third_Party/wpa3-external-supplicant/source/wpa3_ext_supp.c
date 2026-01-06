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

#include "wpa3_ext_supp.h"
#include "cy_result.h"

extern cy_rslt_t cy_wcm_get_whd_interface(cy_wcm_interface_t interface_type, whd_interface_t *whd_iface);
bool wpa3_sae_supplicant_deinit_done = true;
bool wpa3_sae_wcm_registered_callback = false;
bool wpa3_sae_workspace_cleanup_initiated = false;
bool wpa3_sae_scan_in_progress = false;
whd_scan_result_t wpa3_sae_scan_result;
static cy_semaphore_t wpa3_scan_semaphore;
static void wpa3_sae_handle_connect_start ( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg);
static void wpa3_sae_handle_auth_request( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg);
static void wpa3_sae_handle_rx_frame( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg);
cy_rslt_t wpa3_sae_handle_timeout ( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg);
static void wpa3_sae_scan_callback(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status);

wpa3_supplicant_workspace_t *g_workspace = NULL;

void wpa3_sae_statemachine ( cy_thread_arg_t arg )
{
    wpa3_supplicant_workspace_t* workspace = (wpa3_supplicant_workspace_t*)arg;
    wpa3_supplicant_rtos_info_t* rtos_info = (wpa3_supplicant_rtos_info_t*)workspace->wpa3_rtos_info;
    wpa3_supplicant_event_message_t message;

    workspace->wpa3_state = WPA3_SUPPLICANT_NOTHING_STATE;
    cy_rslt_t cy_queue_result;

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_statemachine thread started\n"));

    while (  workspace->wpa3_sae_statemachine_exit == false )
    {
        memset(&message, 0, sizeof(message));
        cy_queue_result = cy_rtos_get_queue( &rtos_info->event_queue, &message, CY_RTOS_NEVER_TIMEOUT, 0 );
        if ( cy_queue_result == CY_RSLT_SUCCESS)
        {
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_statemachine received event\n"));
            wpa3_print_event(message.event_type);
            switch(message.event_type)
            {
                case WPA3_SAE_CONNECT_START:
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SAE_CONNECT_START EVENT received\n"));
                    wpa3_sae_handle_connect_start(workspace, &message);
                    break;

                case WPA3_SUPPLICANT_EVENT_AUTH_REQ:
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_AUTH_REQ received\n"));
                    wpa3_sae_handle_auth_request(workspace, &message);
                    break;

                case WPA3_SUPPLICANT_EVENT_AUTH_RX_FRAME:
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_AUTH_RX_FRAME received, message.length=%d\n", message.length));
                    wpa3_sae_handle_rx_frame(workspace, &message);
                    break;

                case WPA3_SUPPLICANT_EVENT_TIMEOUT:
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_TIMEOUT received\n"));
                    wpa3_sae_handle_timeout(workspace, &message);
                    break;
                case WPA3_SUPPLICANT_EVENT_COMPLETE:
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_COMPLETE received\n"));
                    wpa3_sae_handshake_complete(workspace);
                    workspace->wpa3_sae_statemachine_exit = true;
                    break;
                case WPA3_SUPPLICANT_EVENT_DELETE:
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_DELETE received\n"));
                    workspace->wpa3_sae_statemachine_exit = true;
                    break;
                default:
                   break;
            }
            if ( message.data != NULL )
            {
                free(message.data);
            }
        } /* end of if */
    } /* end of while */

   WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_statemachine thread exiting..queue=%p result=%ld\n", &rtos_info->event_queue, cy_queue_result));
   cy_rtos_exit_thread();
}

static void wpa3_sae_handle_connect_start ( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg)
{
    whd_auth_req_status_t auth_req;
    if ( ( workspace->wpa3_state == WPA3_SUPPLICANT_NOTHING_STATE ) &&
          ( msg->length == sizeof(wpa3_sae_auth_info_t) ) )
    {
          if ( msg->data != NULL )
          {
              /* get ssid and passphrase */
              memcpy(&workspace->wpa3_sae_auth_info, msg->data, msg->length);
          }
    }
    /* Register callback */
    WHD_WIFI_STUB_IMPL(whd_wifi_external_auth_request,_stub)(workspace->interface, wpa3_auth_req_callbackfunc, &auth_req, workspace);
}

cy_rslt_t wpa3_sae_start_scan_hdl ( whd_interface_t iface, wpa3_sae_auth_info_t * sae_auth_info)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    wpa3_supplicant_workspace_t* wksp = NULL;
    whd_ssid_t ssid;

    wksp = wpa3_sae_get_workspace();

    if ( wksp  == NULL )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_get_workspace is NULL \n"));
        return WPA3_EXT_SUPP_ERROR;
    }

    memset(&ssid, 0, sizeof(ssid));
    memcpy(ssid.value, sae_auth_info->ssid, sae_auth_info->ssid_len);
    ssid.length = sae_auth_info->ssid_len;

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_start_scan_hdl calling whd_wifi_scan \n"));

    if ( wpa3_sae_scan_in_progress == false)
    {
        result = whd_wifi_scan(iface, WHD_SCAN_TYPE_ACTIVE, WHD_BSS_TYPE_INFRASTRUCTURE,
                               &ssid, NULL, NULL, NULL, wpa3_sae_scan_callback, &wpa3_sae_scan_result, wksp);

        if (result != CY_RSLT_SUCCESS)
        {
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_start_scan_hdl result=%ld\n", result));
        }

        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_start_scan_hdl calling cy_rtos_get_semaphore \n"));

        wpa3_sae_scan_in_progress = true;
        if ( cy_rtos_get_semaphore(&wpa3_scan_semaphore, WPA3_SCAN_SEMAPHORE_TIMEOUT, false) != CY_RSLT_SUCCESS)
        {
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_start_scan_hdl unable to get semaphore \n"));
        }
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_start_scan_hdl cy_rtos_get_semaphore returned\n"));

        /* Stop scan */
        whd_wifi_stop_scan(iface);
        wpa3_sae_scan_in_progress = false;
    }
    return result;
}

cy_rslt_t wpa3_sae_handle_timeout ( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    whd_buffer_t buffer = NULL;

    if ( workspace->wpa3_state == WPA3_SUPPLICANT_NOTHING_STATE)
    {
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Build SAE commit frame at STA retries attempts=%ld*******\n", workspace->wpa3_sae_sync));
        if ( workspace->wpa3_h2e_capable == true)
        {
            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:PWE generation from PT started at STA ****\n"));
            result =  wpa3_crypto_derive_pwe_from_pt(workspace);
            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:PWE generation from PT done at STA**** result=%ld\n", result));

            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:scalar and element generation H2E started at STA ****\n"));
            result = wpa3_crypto_gen_scalar_and_element(workspace);
            WPA3_EXT_LOG_MSG(("***scalar and element generation H2E done at STA *** result=%ld\n", result));

            result = wpa3_sae_build_commit_message(workspace, &buffer, WPA3_SAE_AUTH_HASH_TO_ELEMENT);
        }
        else
        {
            result = wpa3_sae_build_commit_message(workspace, &buffer, WPA3_SAE_AUTH_STATUS_SUCCESS );
        }
        wpa3_print_state(workspace->wpa3_state);
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE confirm done at STA *******result = %ld\n", result));

        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Send SAE commit frame at STA  ********\n"));
        result = wpa3_sae_send_message(workspace, buffer);
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Sent SAE commit frame at STA ********\n"));
    }
    else if (workspace->wpa3_state == WPA3_SUPPLICANT_COMMITTED_STATE)
    {
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Build SAE confirm frame at STA retries attempts=%ld *******\n", workspace->wpa3_sae_sync));
        result = wpa3_sae_build_confirm_message(workspace, &buffer);
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE confirm done at STA ******* result = %ld\n", result));

        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Send SAE confirm frame at STA  ********\n"));
        result = wpa3_sae_send_message(workspace, buffer);
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Sent SAE confirm frame at STA ********\n"));
    }
    if ( buffer != NULL )
    {
        wpa3_buffer_free(buffer);
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:FREE buffer ptr=%p\n", buffer));
    }
    return result;
}

static void wpa3_sae_handle_auth_request( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg)
{
    whd_auth_req_status_t *auth_req_status = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    whd_buffer_t buffer = NULL;
    whd_mac_t sta_mac_addr;

    memset(&sta_mac_addr, 0, sizeof(whd_mac_t));

    if (( workspace->wpa3_state == WPA3_SUPPLICANT_NOTHING_STATE) && (msg->length == sizeof(whd_auth_req_status_t)))
    {
        auth_req_status = (whd_auth_req_status_t *)msg->data;
        if ( auth_req_status == NULL )
        {
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:event WPA3_SUPPLICANT_EVENT_AUTH_REQ data is NULL\n"));
            return;
        }
        if ( memcmp(workspace->wpa3_sae_auth_info.ssid, auth_req_status->ssid, auth_req_status->ssid_len) == 0 )
        {
            memcpy(workspace->wpa3_sae_auth_info.ap_bssid, auth_req_status->peer_mac.octet, ETHER_ADDR_LEN);
        }
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:event WPA3_SUPPLICANT_EVENT_AUTH_REQ h2e_support=%d\n", workspace->wpa3_h2e_capable));

        if ( workspace->wpa3_h2e_capable != WPA3_SAE_H2E_SUPPORTED )
        {
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:AP supports HnP\n"));
            result = whd_wifi_get_mac_address(workspace->interface, &sta_mac_addr);
            if ( result != CY_RSLT_SUCCESS )
            {
                WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_AUTH_REQ STA get MAC Address failed result:%u\n", (unsigned int)result ));
            }
            memcpy(workspace->wpa3_sae_auth_info.sta_mac, sta_mac_addr.octet, ETH_ADDR_LEN);
            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:PWE generation started at STA ****\n"));
            result =  wpa3_crypto_start_pwe_generation(workspace);
            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:PWE generation HnP done at STA**** result=%ld\n", result));

            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:scalar and element generation started at STA ****\n"));
            result = wpa3_crypto_gen_scalar_and_element(workspace);
            WPA3_EXT_LOG_MSG(("***scalar and element generation HnP done at STA *** result=%ld\n", result));

            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Build SAE commit frame at STA *******\n"));
            result = wpa3_sae_build_commit_message(workspace, &buffer, WPA3_SAE_AUTH_STATUS_SUCCESS);
            WPA3_EXT_LOG_MSG(("*** SAE commit HnP done at STA ******* result = %ld\n", result));

            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE commit frame dump HnP at STA *******\n"));
            WPA3_EXT_HEX_BUF_DUMP((buffer, 256));

            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Send SAE Commit frame HnP at STA  ********\n"));
            result = wpa3_sae_send_message(workspace, buffer);
            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Sent SAE commit frame HnP result=%ld at STA ********\n", result));
            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP "));
            wpa3_print_state(workspace->wpa3_state);
            if ( buffer != NULL )
            {
                wpa3_buffer_free(buffer);
                WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:FREE buffer ptr=%p\n", buffer));
            }
        }
        else
        {
            /* Support for H2E is TBD */
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:AP supports H2E\n"));
            result = whd_wifi_get_mac_address(workspace->interface, &sta_mac_addr);
            if ( result != CY_RSLT_SUCCESS )
            {
                WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_AUTH_REQ STA get MAC Address failed result:%u\n", (unsigned int)result ));
            }
            memcpy(workspace->wpa3_sae_auth_info.sta_mac, sta_mac_addr.octet, ETH_ADDR_LEN);
            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:PWE generation from PT started at STA ****\n"));
            result =  wpa3_crypto_derive_pwe_from_pt(workspace);
            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:PWE generation from PT done at STA**** result=%ld\n", result));

            WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:scalar and element generation H2E started at STA ****\n"));
            result = wpa3_crypto_gen_scalar_and_element(workspace);
            WPA3_EXT_LOG_MSG(("***scalar and element generation H2E done at STA *** result=%ld\n", result));

            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Build SAE commit frame H2E at STA *******\n"));
            result = wpa3_sae_build_commit_message(workspace, &buffer, WPA3_SAE_AUTH_HASH_TO_ELEMENT);
            WPA3_EXT_LOG_MSG(("*** SAE commit H2E done at STA ******* result = %ld\n", result));

            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE Commit frame H2E dump at STA  ********\n"));
            WPA3_EXT_HEX_BUF_DUMP((buffer, 256));

            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Send SAE Commit frame H2E at STA  ********\n"));
            result = wpa3_sae_send_message(workspace, buffer);
            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Sent SAE commit frame H2E result=%ld at STA ********\n", result));
            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP "));
            wpa3_print_state(workspace->wpa3_state);
            if ( buffer != NULL )
            {
                wpa3_buffer_free(buffer);
                WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:FREE buffer ptr=%p\n", buffer));
            }
        } /* end of else */
    } /* end of if */
}

static void wpa3_sae_handle_rx_frame( wpa3_supplicant_workspace_t * workspace, wpa3_supplicant_event_message_t *msg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint16_t auth_transaction = 0;
    whd_buffer_t buffer = NULL;
    uint8_t k[WPA3_SAE_KEYSEED_KEY_LEN] = {0};

    if (( workspace->wpa3_state == WPA3_SUPPLICANT_NOTHING_STATE ) &&
        (msg->length >= WPA3_SAE_RX_FRAME_MIN_LENGTH))
    {
        if ( msg->data == NULL )
        {
            WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:wpa3_sae_process_rx_frame msg->data is NULL, WPA3_SUPPLICANT_NOTHING_STATE **\n"));
            return;
        }
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:wpa3_sae_process_rx_frame state %d *******\n", workspace->wpa3_state ));
        wpa3_print_state(workspace->wpa3_state);
        result = wpa3_sae_process_rx_frame(workspace, (whd_buffer_t *)msg->data, msg->length, &auth_transaction);

        if ( result == CY_RSLT_SUCCESS)
        {
            if ( workspace->wpa3_anticlog_token_present == true )
            {
                /* Build commit message again with anticlog token or h2e */
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Build SAE commit frame (anticlog/h2e) at STA *******\n"));
                if ( workspace->wpa3_h2e_capable == true)
                {
                    result = wpa3_sae_build_commit_message(workspace, &buffer, WPA3_SAE_AUTH_HASH_TO_ELEMENT);
                }
                else
                {
                    result = wpa3_sae_build_commit_message(workspace, &buffer, WPA3_SAE_AUTH_STATUS_SUCCESS);
                }
                WPA3_EXT_LOG_MSG(("*** SAE commit done at STA ******* result = %ld\n", result));

                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE commit frame dump at STA *******\n"));
                WPA3_EXT_HEX_BUF_DUMP((buffer, 256));

                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Send SAE Commit frame at STA  ********\n"));
                result = wpa3_sae_send_message(workspace, buffer);
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Sent SAE commit frame result=%ld at STA ********\n", result));
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP "));
                wpa3_print_state(workspace->wpa3_state);
                if ( buffer != NULL )
                {
                    wpa3_buffer_free(buffer);
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:FREE buffer ptr=%p\n", buffer));
                }
            }
            else
            {
                /* set SC to 1 */
                workspace->wpa3_sae_context_info.sc++;
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Compute shared secret at STA *******\n"));
                result = wpa3_crypto_compute_shared_secret (workspace, k);
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Computed shared secret at STA result =%ld*******\n", result));

                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Derive KCK and PMK at STA ***********\n"))
                result = wpa3_crypto_derive_kck_pmk (workspace, k);
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Derived KCK and PMK at STA ***********\n"))

                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Build SAE confirm frame at STA *******\n"));
                result = wpa3_sae_build_confirm_message(workspace, &buffer);
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE confirm done at STA ******* result = %ld\n", result));

                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:SAE confirm frame dump at STA *******\n"));
                WPA3_EXT_HEX_BUF_DUMP((buffer, 256));

                WPA3_EXT_LOG_MSG(("*** Send SAE confirm frame at STA  ********\n"));
                result = wpa3_sae_send_message(workspace, buffer);
                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:Sent SAE confirm frame at STA  result=%ld********\n", result));
                workspace->wpa3_state = WPA3_SUPPLICANT_COMMITTED_STATE;

                WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:"));
                wpa3_print_state(workspace->wpa3_state);
                if ( buffer != NULL )
                {
                    wpa3_buffer_free(buffer);
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:FREE buffer ptr=%p\n", buffer));
                }
            }
        }
        else if ( result == WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO)
        {
            /* send authentication failure */
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_process_rx_frame failed result=%ld ", result));
            wpa3_print_state(workspace->wpa3_state);
            workspace->wpa3_sae_rx_handshake_fail = true;
            wpa3_sae_handshake_failure(workspace, auth_transaction, WPA3_SAE_AUTH_MISMATCH);
        }
        else
        {
            /* silently discard the packet */
            if ( workspace->wpa3_sae_sync <= WPA3_SAE_MAX_RETRANS_ATTEMTPS)
            {
                /* re-start the timer t0 */
                cy_rtos_start_timer(&(workspace->wpa3_sae_timer), WPA3_SAE_HANDSHAKE_TIMEOUT_MS);
            }
        }
    }
    else if ((workspace->wpa3_state == WPA3_SUPPLICANT_COMMITTED_STATE) &&
             ( msg->length >= WPA3_SAE_RX_FRAME_MIN_LENGTH))
    {
        if ( msg->data == NULL )
        {
             WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:wpa3_sae_process_rx_frame msg->data is NULL, WPA3_SUPPLICANT_COMMITTED_STATE **\n"));
             return;
        }

        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:wpa3_sae_process_rx_frame state WPA3_SUPPLICANT_CONFIRMED_STATE *******\n"));
        result = wpa3_sae_process_rx_frame(workspace, (whd_buffer_t *)msg->data, msg->length, &auth_transaction);
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:wpa3_sae_process_rx_frame result=%ld *******\n", result));

        if ( result == CY_RSLT_SUCCESS)
        {
            workspace->wpa3_state = WPA3_SUPPLICANT_CONFIRMED_STATE;
        }
        else if ( result == WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO)
        {
            /* send authentication failure */
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_process_rx_frame failed result=%ld ", result));
            wpa3_print_state(workspace->wpa3_state);
            workspace->wpa3_sae_rx_handshake_fail = true;
            wpa3_sae_handshake_failure(workspace, auth_transaction, WPA3_SAE_AUTH_MISMATCH);
        }
        else if ( result == WPA3_EXT_SUPP_CONFIRM_VERIFY_FAILURE)
        {
            /* send authentication failure */
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_process_rx_frame failed result=%ld", result));
            wpa3_print_state(workspace->wpa3_state);
            workspace->wpa3_sae_rx_handshake_fail = true;
            wpa3_sae_handshake_failure(workspace, auth_transaction, WPA3_SAE_AUTH_FAILURE);
        }
        else
        {
            /* silently discard the packet */
            if ( workspace->wpa3_sae_sync <= WPA3_SAE_MAX_RETRANS_ATTEMTPS)
            {
                /* increment Sync and Sc */
                workspace->wpa3_sae_sync++;
                workspace->wpa3_sae_context_info.sc++;
                /* re-start the timer t0 */
                cy_rtos_start_timer(&(workspace->wpa3_sae_timer), WPA3_SAE_HANDSHAKE_TIMEOUT_MS);
            }
        }
        WPA3_EXT_LOG_MSG(("*** WPA3-EXT-SUPP:WPA3 "));
        wpa3_print_state(workspace->wpa3_state);
    }
}

cy_rslt_t wpa3_sae_handshake_failure( wpa3_supplicant_workspace_t* workspace, uint16_t auth_transaction, uint16_t status)
{
    wpa3_supplicant_event_message_t msg;
    whd_buffer_t buffer = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if ( status == WPA3_SAE_AUTH_FAILURE)
    {
        /* build authentication reject message */
        result = wpa3_sae_build_auth_rej_message(workspace, &buffer, status, auth_transaction);

        if ( result == CY_RSLT_SUCCESS)
        {
            result = wpa3_sae_send_message ( workspace, buffer );
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_send_message result=%ld\n", result));
        }
        else
        {
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:failed to build wpa3_sae_build_auth_rej_message result=%ld\n", result));
        }
    }
    /* post an event */
    msg.data = NULL;
    msg.event_type = WPA3_SUPPLICANT_EVENT_DELETE;
    msg.length = 0;

    wpa3_supplicant_send_event(workspace, &msg);

    if ( workspace->wpa3_sae_rx_handshake_fail == true )
    {
        /* start the timer to cleanup*/
        cy_rtos_start_timer(&(workspace->wpa3_sae_timer), WPA3_SAE_HANDSHAKE_DELETE_MS);
    }
    if ( buffer != NULL )
    {
        wpa3_buffer_free(buffer);
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:FREE buffer ptr=%p\n", buffer));
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_sae_handshake_complete( wpa3_supplicant_workspace_t* workspace)
{
    whd_auth_req_status_t params;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    pmkid_t pmkid;

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_handshake_complete entered \n"));

    if ( workspace == NULL )
    {
        return WPA3_EXT_SUPP_ERROR;
    }
    memset(&params, 0, sizeof(params));

    memcpy(params.peer_mac.octet, workspace->wpa3_sae_auth_info.ap_bssid, ETH_ADDR_LEN);
    params.ssid_len = strlen((const char *)workspace->wpa3_sae_auth_info.ssid);
    memcpy(params.ssid, workspace->wpa3_sae_auth_info.ssid, params.ssid_len);
    memcpy(params.pmkid, workspace->wpa3_sae_context_info.pmkid, sizeof(params.pmkid));

    memcpy((void *)pmkid.BSSID.octet, workspace->wpa3_sae_auth_info.ap_bssid, ETH_ADDR_LEN);
    memcpy((void *)pmkid.PMKID, workspace->wpa3_sae_context_info.pmkid, sizeof(params.pmkid));

    /* set auth status */
    result = WHD_WIFI_STUB_IMPL(whd_wifi_set_auth_status,_stub)(workspace->interface, &params);
    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_set_auth_status failed result=%ld \n", result));
    }

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_set_auth_status success result=%ld \n", result));

    /* set PMKID */
    result = WHD_WIFI_STUB_IMPL(whd_wifi_set_pmksa,_stub)(workspace->interface, (const pmkid_t *)&pmkid);
    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_set_pmksa failed result=%ld \n", result));
    }
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_set_pmksa success result=%ld \n", result));

    /* set PMK */
    result = WHD_WIFI_STUB_IMPL(whd_wifi_set_pmk,_stub)(workspace->interface, workspace->wpa3_sae_context_info.pmk, sizeof(workspace->wpa3_sae_context_info.pmk));
    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_set_pmk failed result=%ld \n", result));
    }
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_set_pmk success result=%ld \n", result));

    /* stop timer t0 and start timer t1  and upon
     * expiry delete all context info
     */
    cy_rtos_stop_timer(&(workspace->wpa3_sae_timer));

    workspace->wpa3_sae_handshake_success = true;
    cy_rtos_start_timer(&(workspace->wpa3_sae_timer), WPA3_SAE_HANDSHAKE_DELETE_MS);

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_handshake_complete Exit \n"));

    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_sae_build_confirm_message( wpa3_supplicant_workspace_t *workspace, whd_buffer_t *buffer )
{
    dot11_mgmt_auth_t *auth;
    whd_buffer_t buf;
    uint16_t seqnum = 0;
    uint16_t auth_transaction = WPA3_SAE_CONFIRM_MSG;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    whd_auth_params_t *confirm_msg_buf = NULL;

    result = wpa3_buffer_alloc ( &buf, WPA3_AUTH_DATA_BUF_LEN);

    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_buffer_alloc failed \n"));
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }

    memset(buf, 0, WPA3_AUTH_DATA_BUF_LEN);

    confirm_msg_buf = (whd_auth_params_t *)buf;
    memset(confirm_msg_buf, 0, sizeof(whd_auth_params_t));
    memcpy(confirm_msg_buf->bssid.octet, workspace->wpa3_sae_auth_info.ap_bssid, sizeof(workspace->wpa3_sae_auth_info.ap_bssid));
    memcpy(confirm_msg_buf->da.octet, workspace->wpa3_sae_auth_info.ap_bssid, sizeof(workspace->wpa3_sae_auth_info.ap_bssid));
    confirm_msg_buf->fc = ((WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4));

    confirm_msg_buf->packetId =1;

    auth = (dot11_mgmt_auth_t *)confirm_msg_buf->data; /* dot11_mgmt_auth_t */

    auth->frame_control = ((WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4));

    memcpy(auth->dst_addr, workspace->wpa3_sae_auth_info.ap_bssid, ETH_ADDR_LEN);
    memcpy(auth->src_addr, workspace->wpa3_sae_auth_info.sta_mac, ETH_ADDR_LEN);

    memcpy(auth->bssid, auth->dst_addr, ETH_ADDR_LEN);
    auth->seq_ctrl = seqnum << 4;

    /* auth frame header */
    auth->auth_alg = WLAN_AUTH_SAE;
    auth->auth_transaction = auth_transaction;
    auth->status_code = WPA3_SAE_AUTH_STATUS_SUCCESS;
    confirm_msg_buf->len = OFFSETOF(dot11_mgmt_auth_t, data);

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:buf=%p confirm_msg_buf->len =%d\n", buf, confirm_msg_buf->len));

    /* copy the confirm handshake */
    result = wpa3_crypto_get_send_confirm_handshake(workspace, auth->data);

    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:failed to copy send confirm handshake \n"));
        return WPA3_EXT_SUPP_ERROR;
    }

    confirm_msg_buf->len += WPA3_SAE_CONFIRM_LEN + WPA3_SAE_SEND_CONFIRM_LEN;
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:confirm_msg_buf->len =%d after data\n", confirm_msg_buf->len));
    *buffer = buf;

    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_sae_build_auth_rej_message( wpa3_supplicant_workspace_t *workspace, whd_buffer_t *buffer, uint16_t auth_status, uint16_t auth_transaction )
{
    dot11_mgmt_auth_t *auth;
    whd_buffer_t buf;
    uint16_t seqnum = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    whd_auth_params_t *confirm_msg_buf = NULL;

    result = wpa3_buffer_alloc (&buf, WPA3_AUTH_DATA_BUF_LEN);

    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_buffer_alloc failed \n"));
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }

    memset(buf, 0, WPA3_AUTH_DATA_BUF_LEN);

    confirm_msg_buf = (whd_auth_params_t *)buf;
    memset(confirm_msg_buf, 0, sizeof(whd_auth_params_t));
    memcpy(confirm_msg_buf->bssid.octet, workspace->wpa3_sae_auth_info.ap_bssid, sizeof(workspace->wpa3_sae_auth_info.ap_bssid));
    memcpy(confirm_msg_buf->da.octet, workspace->wpa3_sae_auth_info.ap_bssid, sizeof(workspace->wpa3_sae_auth_info.ap_bssid));
    confirm_msg_buf->fc = ((WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4));

    confirm_msg_buf->packetId =1;

    auth = (dot11_mgmt_auth_t *)confirm_msg_buf->data; /* dot11_mgmt_auth_t */

    auth->frame_control = ((WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4));

    memcpy(auth->dst_addr, workspace->wpa3_sae_auth_info.ap_bssid, ETH_ADDR_LEN);
    memcpy(auth->src_addr, workspace->wpa3_sae_auth_info.sta_mac, ETH_ADDR_LEN);

    memcpy(auth->bssid, auth->dst_addr, ETH_ADDR_LEN);
    auth->seq_ctrl = seqnum << 4;

    /* auth frame header */
    auth->auth_alg = WLAN_AUTH_SAE;
    auth->auth_transaction = auth_transaction;
    auth->status_code = auth_status;
    confirm_msg_buf->len = OFFSETOF(dot11_mgmt_auth_t, data);

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:confirm_msg_buf->len =%d\n", confirm_msg_buf->len));

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:confirm_msg_buf->len =%d after data\n", confirm_msg_buf->len));
    *buffer = buf;

    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_sae_process_rx_frame ( wpa3_supplicant_workspace_t *workspace, whd_buffer_t *buffer, uint16_t len, uint16_t *auth_transaction)
{
    dot11_mgmt_auth_t *auth = (dot11_mgmt_auth_t *)buffer;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint8_t offset = OFFSETOF(dot11_mgmt_auth_t, data);
    uint8_t *buf = (uint8_t *)buffer;
    uint16_t msgtype;
    wpa3_supplicant_event_message_t msg;
    uint8_t status;
    uint8_t anticlog_tag_len = 0;
    wpa3_sae_anticlog_container_t *anticlog_cont = NULL;

    if ( ( auth == NULL ) || ( len == 0))
    {
        return WPA3_EXT_SUPP_ERROR;
    }

    /* stop SAE timer */
    cy_rtos_stop_timer(&(workspace->wpa3_sae_timer));

    if (( auth->status_code != WPA3_SAE_AUTH_STATUS_SUCCESS) )
    {
        if  ( workspace->wpa3_state == WPA3_SUPPLICANT_NOTHING_STATE )
        {
            if (auth->status_code == WPA3_SAE_AUTH_ANTICLOG_TOKEN_REQUIRED)
            {
                /* copy anti clog authentication token */
                workspace->wpa3_anticlog_token_present = true;
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:Anticlog Token present =%d\n", workspace->wpa3_anticlog_token_present));
                if ( workspace->wpa3_h2e_capable == WPA3_SAE_H2E_SUPPORTED)
                {
                    anticlog_tag_len = len - offset - WPA3_SAE_ECP_GROUP_ID_LEN;
                    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:anticlog_tag_len =%d\n", anticlog_tag_len));
                    /* parse the anticlog container and copy the data */
                    if ( ( anticlog_tag_len >= WPA3_SAE_ANTICLOG_TOKEN_MIN_LEN) &&
                         ( anticlog_tag_len <= WPA3_SAE_ANTICLOG_TOKEN_MAX_LEN))
                    {
                        anticlog_cont = wpa3_sae_find_anticlog_ie(&buf[offset + WPA3_SAE_ECP_GROUP_ID_LEN], anticlog_tag_len);
                        if ( anticlog_cont == NULL )
                        {
                            /* cannot find anticlog container */
                            anticlog_tag_len = 0;
                        }
                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: Anticlog token container anticlog_cont:%p anticlog_tag_len=%d\n", anticlog_cont, anticlog_tag_len));
                    }
                }
                else
                {
                    /* copy the anticlog data */
                    anticlog_tag_len = len - offset - WPA3_SAE_ECP_GROUP_ID_LEN;
                    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:Anticlog Token  len=%d offset=%d anticlog_tag_len =%d\n",
                                        len, offset, anticlog_tag_len));
                }
                if ( anticlog_tag_len > 0)
                {
                    memcpy(workspace->wpa3_sae_anticlog_token, &buf[offset + WPA3_SAE_ECP_GROUP_ID_LEN], anticlog_tag_len );
                    workspace->wpa3_sae_anticlog_token_len = anticlog_tag_len;
                    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: Anticlog token dump len=%d\n", workspace->wpa3_sae_anticlog_token_len));
                    WPA3_EXT_HEX_BUF_DUMP((workspace->wpa3_sae_anticlog_token, workspace->wpa3_sae_anticlog_token_len ));
                }
            }
            else if (auth->status_code == WPA3_SAE_AUTH_HASH_TO_ELEMENT)
            {
                if( workspace->wpa3_anticlog_token_present == true )
                {
                    workspace->wpa3_anticlog_token_present = false;
                    memset(workspace->wpa3_sae_anticlog_token, 0, sizeof(workspace->wpa3_sae_anticlog_token));
                }
                /* H2E element method supported by AP */
                workspace->wpa3_h2e_method = true;
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:Hash to element =%d\n", workspace->wpa3_h2e_method));
            }
            else if ( ( auth->status_code == WPA3_SAE_AUTH_MISMATCH ) ||
                      ( auth->status_code == WPA3_SAE_AUTH_INVALID_FINITE_CYCLIC_GRP))
            {
                /* post a delete event as only ECP group ID 19 is supported */
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:BadAlgo auth->status_code=%d\n", auth->status_code));

                /* need to send authenticate reject with status code 13 to peer after sending DEL event */
                return WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO;
            }
        }
        else
        {
            if ( workspace->wpa3_state == WPA3_SUPPLICANT_COMMITTED_STATE )
            {
                /* increment Sync and Sc */
                workspace->wpa3_sae_context_info.sc++;
                workspace->wpa3_sae_sync++;
            }
            /* SAE auth status is not success */
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:auth->status_code=%d", auth->status_code));
            wpa3_print_state(workspace->wpa3_state);
            return WPA3_EXT_SUPP_SILENTLY_DISCARD;
        }
    }
    msgtype = auth->auth_transaction;
    *auth_transaction = msgtype;

    status = auth->status_code;
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:seq_ctrl=%d msgtype=%d status=%d\n", auth->auth_transaction, msgtype, auth->status_code));
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_process_rx_frame entered buffer_len=%d len=%d\n", (len - offset), len));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:RX frame buffer dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((buf, len));

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_process_rx_frame entered buffer_len=%d len=%d offset=%d\n", (len - offset), len, offset));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:RX frame buffer data dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((&buf[offset], (len - offset)));

    if ( ( status == WPA3_SAE_AUTH_STATUS_SUCCESS)
         || (auth->status_code == WPA3_SAE_AUTH_ANTICLOG_TOKEN_REQUIRED)
         || (auth->status_code == WPA3_SAE_AUTH_HASH_TO_ELEMENT))
    {
        if ( msgtype == WPA3_SAE_COMMIT_MSG)
        {
            if ( status == WPA3_SAE_AUTH_STATUS_SUCCESS)
            {
                if( workspace->wpa3_anticlog_token_present == true )
                {
                    workspace->wpa3_anticlog_token_present = false;
                    memset(workspace->wpa3_sae_anticlog_token, 0, sizeof(workspace->wpa3_sae_anticlog_token));
                }
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:WPA3_SAE_COMMIT_MSG received from peeer status=%d \n", status));
                result =  wpa3_crypto_get_peer_grp_id_scalar_element(workspace, &buf[offset]);

                if ( result != CY_RSLT_SUCCESS)
                {
                    WPA3_EXT_LOG_MSG(("failed to copy AP scalar and element \n"));
                    return WPA3_EXT_SUPP_SILENTLY_DISCARD;
                }

                if ( workspace->wpa3_sae_context_info.peer_group_id != WPA3_SAE_ECP_GROUP_ID)
                {
                   /* SAE auth algo is not ECP group ID
                    * send DEL event and move to nothing state
                    */
                   WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:auth->auth_transaction=%d\n", auth->auth_transaction));
                   return WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO;
                }

               result = wpa3_crypto_chk_own_peer_scalar_element(workspace);

               if ( result != CY_RSLT_SUCCESS)
               {
                   WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:peer scalar or element is same as own silently discard\n"));
                   return WPA3_EXT_SUPP_SILENTLY_DISCARD;
               }
               WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_crypto_chk_own_peer_scalar_element() result=%ld\n", result));
            }
            else if (status == WPA3_SAE_AUTH_ANTICLOG_TOKEN_REQUIRED )
            {
                memcpy(&(workspace->wpa3_sae_context_info.peer_group_id), &buf[offset], sizeof(workspace->wpa3_sae_context_info.peer_group_id));
                len += sizeof(workspace->wpa3_sae_context_info.peer_group_id);

                if ( workspace->wpa3_sae_context_info.peer_group_id != WPA3_SAE_ECP_GROUP_ID)
                {
                      /* SAE auth algo is not ECP group ID
                       * send DEL event and move to nothing state
                       */
                      WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:auth->auth_transaction=%d\n", auth->auth_transaction));
                      return WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO;
                 }
            }
            else if ( status == WPA3_SAE_AUTH_HASH_TO_ELEMENT)
            {
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:WPA3_SAE_COMMIT_MSG H2E received from peeer status=%d \n", status));
                result =  wpa3_crypto_get_peer_grp_id_scalar_element(workspace, &buf[offset]);
                if ( result != CY_RSLT_SUCCESS)
                {
                    WPA3_EXT_LOG_MSG(("failed to copy AP scalar and element \n"));
                    return WPA3_EXT_SUPP_SILENTLY_DISCARD;
                }
                if ( workspace->wpa3_sae_context_info.peer_group_id != WPA3_SAE_ECP_GROUP_ID)
                {
                   /* SAE auth algo is not ECP group ID
                    * send DEL event and move to nothing state
                    */
                   WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:auth->auth_transaction=%d\n", auth->auth_transaction));
                   return WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO;
                 }
                 result = wpa3_crypto_chk_own_peer_scalar_element(workspace);
                 if ( result != CY_RSLT_SUCCESS)
                 {
                     WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:peer scalar or element is same as own silently discard\n"));
                     return WPA3_EXT_SUPP_SILENTLY_DISCARD;
                 }
                 WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_crypto_chk_own_peer_scalar_element() result=%ld\n", result));
            }
        }
        else if (msgtype == WPA3_SAE_CONFIRM_MSG)
        {
            /* get confirm from peer */
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:WPA3_SAE_CONFIRM_MSG received from peer status=%d\n", status));

            /* copy the peer-send-confirm and confirm message */
            result =  wpa3_crypto_get_send_confirm(workspace, &buf[offset]);

            if ( result != CY_RSLT_SUCCESS)
            {
                WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:failed to copy peer send-confirm and confirm message \n"));
                return WPA3_EXT_SUPP_SILENTLY_DISCARD;
            }

            result = wpa3_sae_verify_confirm_message(workspace);
            if ( result == CY_RSLT_SUCCESS)
            {
               WPA3_EXT_LOG_MSG(("\n***WPA3-EXT-SUPP:WPA3_SAE_CONFIRM_MSG VERIFIED CONFIRM SUCCESS***\n"));

               /* post an event */
               msg.data = NULL;
               msg.event_type = WPA3_SUPPLICANT_EVENT_COMPLETE;
               msg.length = 0;
               wpa3_supplicant_send_event(workspace, &msg);
            }
            else
            {
               WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:WPA3_SAE_CONFIRM_MSG MISMATCH*****\n"));
               return WPA3_EXT_SUPP_CONFIRM_VERIFY_FAILURE;
            }
        }
        else
        {
            /* unknown message received from peer */
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:unknown message received from peer\n"));
            result = WPA3_EXT_SUPP_SILENTLY_DISCARD;
        }
    }
    else
    {
           WPA3_EXT_LOG_MSG(("****WPA3-EXT-SUPP:wpa3_sae_process_rx_frame() authentication message status failed status=%d**** \n", status));
           return WPA3_EXT_SUPP_SILENTLY_DISCARD;
    }
    return result;
}

cy_rslt_t wpa3_sae_verify_confirm_message(wpa3_supplicant_workspace_t *workspace)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    result =  wpa3_crypto_verify_confirm_message(workspace);

    if ( result != CY_RSLT_SUCCESS)
    {
           WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_verify_confirm_message()  Verify failed result =%ld at AP\n", result));
           result = WPA3_EXT_SUPP_CONFIRM_VERIFY_FAILURE;
    }
    return result;
}

cy_rslt_t wpa3_sae_build_commit_message ( wpa3_supplicant_workspace_t *workspace, whd_buffer_t *data, uint8_t auth_status )
{
    dot11_mgmt_auth_t *auth;
    whd_buffer_t buf;
    uint16_t seqnum = 0;
    uint16_t auth_transaction = WPA3_SAE_COMMIT_MSG;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    whd_auth_params_t *commit_msg_buf = NULL;
    uint8_t *datbuf = NULL;
    uint8_t offset = 0;

    result = wpa3_buffer_alloc (&buf, WPA3_AUTH_DATA_BUF_LEN);

    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_buffer_alloc failed \n"));
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }

    memset(buf, 0, WPA3_AUTH_DATA_BUF_LEN);

    commit_msg_buf = (whd_auth_params_t *)buf;
    memset(commit_msg_buf, 0, sizeof(whd_auth_params_t));
    memcpy(commit_msg_buf->bssid.octet, workspace->wpa3_sae_auth_info.ap_bssid, sizeof(workspace->wpa3_sae_auth_info.ap_bssid));
    memcpy(commit_msg_buf->da.octet, workspace->wpa3_sae_auth_info.ap_bssid, sizeof(workspace->wpa3_sae_auth_info.ap_bssid));
    commit_msg_buf->fc = ((WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4));


    auth = (dot11_mgmt_auth_t *)commit_msg_buf->data; /* dot11_mgmt_auth_t */
    datbuf = (uint8_t *)auth->data;

    auth->frame_control = ((WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4));

    memcpy(auth->dst_addr, workspace->wpa3_sae_auth_info.ap_bssid, ETH_ADDR_LEN);
    memcpy(auth->src_addr, workspace->wpa3_sae_auth_info.sta_mac, ETH_ADDR_LEN);

    memcpy(auth->bssid, auth->dst_addr, ETH_ADDR_LEN);
    auth->seq_ctrl = seqnum << 4;

    /* auth frame header */
    auth->auth_alg = WLAN_AUTH_SAE;
    auth->auth_transaction = auth_transaction;
    auth->status_code = auth_status;
    commit_msg_buf->len = OFFSETOF(dot11_mgmt_auth_t, data);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:commit_msg_buf->len =%d\n", commit_msg_buf->len));

    result = wpa3_crypto_get_grp_id(workspace, auth->data);
    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:failed to group id\n"));
        return WPA3_EXT_SUPP_ERROR;
    }

    commit_msg_buf->len += WPA3_SAE_ECP_GROUP_ID_LEN;
    offset = WPA3_SAE_ECP_GROUP_ID_LEN;
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:commit_msg_buf->len =%d workspace->wpa3_sae_anticlog_token_len=%d\n",
                       commit_msg_buf->len, workspace->wpa3_sae_anticlog_token_len));

    if ( workspace->wpa3_anticlog_token_present == true)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_build_commit_message workspace->wpa3_anticlog_token_present=%d \n", workspace->wpa3_anticlog_token_present));
        if ( workspace->wpa3_h2e_capable == true)
        {
            result = wpa3_crypto_get_scalar_element(workspace, &datbuf[offset]);
            if ( result != CY_RSLT_SUCCESS)
            {
                 WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:failed to scalar and element\n"));
                 return WPA3_EXT_SUPP_ERROR;
            }
            commit_msg_buf->len += WPA3_SAE_SCALAR_LEN + WPA3_SAE_ELEMENT_LEN;
            offset += WPA3_SAE_SCALAR_LEN + WPA3_SAE_ELEMENT_LEN;
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_build_commit_message H2E commit_msg_buf->len=%d offset=%d\n", commit_msg_buf->len, offset));
            memcpy(&datbuf[offset], workspace->wpa3_sae_anticlog_token, workspace->wpa3_sae_anticlog_token_len);
            commit_msg_buf->len += workspace->wpa3_sae_anticlog_token_len;
            offset += workspace->wpa3_sae_anticlog_token_len;
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:commit_msg_buf->len =%d after data buffer=%p\n", commit_msg_buf->len, buf));
            *data = buf;
        }
        else
        {
            memcpy(&datbuf[offset], workspace->wpa3_sae_anticlog_token, workspace->wpa3_sae_anticlog_token_len);
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_build_commit_message offset=%d \n", commit_msg_buf->len));
            commit_msg_buf->len += workspace->wpa3_sae_anticlog_token_len;
            offset += workspace->wpa3_sae_anticlog_token_len;
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_build_commit_message copied anticlog token total len=%d  offset=%d\n", commit_msg_buf->len, offset));
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_build_commit_message token dump len=%d\n", workspace->wpa3_sae_anticlog_token_len));
            WPA3_EXT_HEX_BUF_DUMP((workspace->wpa3_sae_anticlog_token, workspace->wpa3_sae_anticlog_token_len));

            result = wpa3_crypto_get_scalar_element(workspace, &datbuf[offset]);
            if ( result != CY_RSLT_SUCCESS)
            {
                 WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:failed to scalar and element\n"));
                 return WPA3_EXT_SUPP_ERROR;
            }
            commit_msg_buf->len += WPA3_SAE_SCALAR_LEN + WPA3_SAE_ELEMENT_LEN;
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:commit_msg_buf->len =%d after data buffer=%p\n", commit_msg_buf->len, buf));
            *data = buf;
        }
    }
    else
    {
        result = wpa3_crypto_get_scalar_element(workspace, &datbuf[offset]);
        if ( result != CY_RSLT_SUCCESS)
        {
            WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:failed to scalar and element\n"));
            return WPA3_EXT_SUPP_ERROR;
        }
        commit_msg_buf->len += WPA3_SAE_SCALAR_LEN + WPA3_SAE_ELEMENT_LEN;
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:commit_msg_buf->len =%d after data buffer=%p\n", commit_msg_buf->len, buf));
        *data = buf;
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_sae_send_message ( wpa3_supplicant_workspace_t *workspace, whd_buffer_t buffer )
{
    uint32_t ret = 0;
    whd_auth_params_t *mf_params = (whd_auth_params_t *)buffer;

    if ( mf_params == NULL )
    {
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }

    ret =  WHD_WIFI_STUB_IMPL(whd_wifi_send_auth_frame,_stub)(workspace->interface, mf_params);
    if ( ret != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_send_message failed ret=%ld\n", ret));
        return ret;
    }

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_send_auth_frame()returned ret=%ld\n", ret));
    /* start a retransmission timer t0 */
    cy_rtos_start_timer(&(workspace->wpa3_sae_timer), WPA3_SAE_HANDSHAKE_TIMEOUT_MS);

    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_supplicant_h2e_pfn_list_derive_pt (uint8_t *ssid, uint8_t ssid_len, uint8_t *passphrase, uint8_t passphrase_len, uint8_t *output, uint8_t outlen )
{
    wpa3_supplicant_workspace_t *wksp = NULL;
    cy_rslt_t result;

    /* initialize WPA3 supplicant */
    result = wpa3_supplicant_init_pt_workspace(&wksp);
    wpa3_sae_set_workspace(wksp);

    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:init workspace failed result=%ld\n", result ));
        return result;
    }

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_h2e_pfn_list_derive_pt calling wpa3_crypto_derive_pt\n" ));
    result = wpa3_crypto_derive_pt( wksp, ssid, passphrase, output, outlen);
    if ( result != CY_RSLT_SUCCESS)
    {
       WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_h2e_pfn_list_derive_pt wpa3_crypto_derive_pt failed result=%ld\n", result ));
       return result;
    }
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_h2e_pfn_list_derive_pt wpa3_crypto_derive_pt done result=%ld\n", result ));

    /* deinit workspace */
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_h2e_pfn_list_derive_pt calling wpa3_supplicant_deinit_pt_workspace \n" ));
    wpa3_supplicant_deinit_pt_workspace(wksp);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_supplicant_init_pt_workspace ( wpa3_supplicant_workspace_t ** wksp)
{
    cy_rslt_t result;
    wpa3_supplicant_workspace_t *workspace = NULL;

    workspace = malloc ( sizeof(wpa3_supplicant_workspace_t));
    if (workspace == NULL )
    {
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }
    memset(workspace, 0, sizeof(wpa3_supplicant_workspace_t));

    /* Initialize WPA3 SAE context info */
    result = wpa3_supplicant_init_sae_context_info(workspace);
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_init_sae_context_info failed\n"));
        result = WPA3_EXT_SUPP_ERROR;
    }

    /* Initialize crypto for WPA3 */
    result = wpa3_crypto_init(workspace);
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_crypto_init failed\n"));
        result = WPA3_EXT_CRYPTO_ERROR;
    }
    *wksp = workspace;
    return result;
}

cy_rslt_t wpa3_supplicant_deinit_pt_workspace(wpa3_supplicant_workspace_t *wksp)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if ( wksp == NULL )
    {
        return CY_RSLT_SUCCESS;
    }
    /* de-init sae context info */
    result = wpa3_supplicant_deinit_sae_context_info(wksp);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE context info deleted\n"));

    /* de-init crypto */
    result = (cy_rslt_t) wpa3_crypto_deinit(wksp);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE crypto context deleted\n"));

    /* delete workspace */
    if ( wksp != NULL )
    {
        free(wksp);
    }
    wpa3_sae_set_workspace(NULL);
    WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:SAE workspace deleted*** \n"));
    return result;
}

cy_rslt_t wpa3_supplicant_sae_start (uint8_t *ssid, uint8_t ssid_len, uint8_t *passphrase, uint8_t passphrase_len)
{
    wpa3_supplicant_workspace_t *wksp = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    wpa3_sae_auth_info_t *wpa3_sae_auth_info = NULL;
    wpa3_supplicant_event_message_t   msg;

    if ( g_workspace != NULL )
    {
         WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant workspace already created\n"));
         return result;
    }

    /* initialize WPA3 supplicant */
    result = wpa3_supplicant_init_workspace(&wksp);
    if ( result != CY_RSLT_SUCCESS)
    {
        wpa3_supplicant_deinit_workspace(wksp);
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:init workspace failed result=%ld\n", result ));
        return result;
    }

    wpa3_sae_set_workspace(wksp);
    result = cy_rtos_create_thread(&(wksp->wpa3_rtos_info->thread_handle), wpa3_sae_statemachine, "WPA3 SAE Thread",
                                    wksp->wpa3_rtos_info->thread_stack,
                                    WPA3_THREAD_STACK_SIZE, CY_RTOS_PRIORITY_NORMAL, (cy_thread_arg_t )wksp);

    if ( result != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:supplicant create thread failed result=%ld\n", result ));
        return result;
    }

    /* post an event to wpa3_sae_statemachine thread */
    msg.event_type = WPA3_SAE_CONNECT_START;
    msg.data = malloc(sizeof(wpa3_sae_auth_info_t));

    if ( msg.data == NULL )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_auth_info_t alloc failed\n"));
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }
    memset(msg.data, 0, sizeof(wpa3_sae_auth_info_t));

    msg.length = sizeof(wpa3_sae_auth_info_t);
    wpa3_sae_auth_info = msg.data;

    /* set ssid and passphrase */
    memcpy(wpa3_sae_auth_info->ssid, ssid, ssid_len);
    wpa3_sae_auth_info->ssid_len = ssid_len;

    memcpy(wpa3_sae_auth_info->passhphrase, passphrase, passphrase_len);
    wpa3_sae_auth_info->passphrase_len = passphrase_len;

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:calling wpa3_sae_start_scan_hdl \n"));
    wpa3_sae_start_scan_hdl(wksp->interface, wpa3_sae_auth_info);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sae_start_scan_hdl returned \n"));

    result = wpa3_supplicant_send_event(wksp, &msg);
    if ( result != CY_RSLT_SUCCESS )
    {
       WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_send_event failed\n"));
       result = WPA3_EXT_SUPP_ERROR;
    }
    return result;
}

cy_rslt_t wpa3_supplicant_init_workspace ( wpa3_supplicant_workspace_t ** wksp)
{
    cy_rslt_t result;
    wpa3_supplicant_workspace_t *workspace = NULL;

    workspace = malloc ( sizeof(wpa3_supplicant_workspace_t));
    if (workspace == NULL )
    {
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }
    memset(workspace, 0, sizeof(wpa3_supplicant_workspace_t));

    workspace->wpa3_rtos_info = malloc( sizeof(wpa3_supplicant_rtos_info_t));
    if ( workspace->wpa3_rtos_info == NULL)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_rtos_info alloc failed\n"));
        free(workspace);
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }
    memset(workspace->wpa3_rtos_info, 0, sizeof(wpa3_supplicant_rtos_info_t));

    workspace->wpa3_rtos_info->thread_stack = (unsigned char *)malloc (WPA3_THREAD_STACK_SIZE);

    if ( workspace->wpa3_rtos_info->thread_stack == NULL )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:thread_stack alloc failed\n"));
        free(workspace->wpa3_rtos_info);
        free(workspace);
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }

    result = cy_rtos_init_queue( &(workspace->wpa3_rtos_info->event_queue), WPA3_SUPPLICANT_QUEUE_SZ, sizeof(wpa3_supplicant_event_message_t) );
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:cy_rtos_init_queue failed\n"));
        free(workspace->wpa3_rtos_info);
        free(workspace);
        result = WPA3_EXT_SUPP_ERROR;
    }

    result = cy_rtos_init_timer(&(workspace->wpa3_sae_timer), CY_TIMER_TYPE_ONCE, wpa3_sae_timer_expiry, (cy_timer_callback_arg_t )workspace);
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:cy_rtos_init_timer failed\n"));

        /* de-init queue */
        cy_rtos_deinit_queue(&(workspace->wpa3_rtos_info->event_queue));
        free(workspace->wpa3_rtos_info);
        free(workspace);
        result = WPA3_EXT_SUPP_ERROR;
    }

    if (cy_rtos_init_semaphore(&wpa3_scan_semaphore, WPA3_MAX_SEMA_COUNT, 0) != CY_RSLT_SUCCESS)
    {

        /* de-init queue */
        cy_rtos_deinit_queue(&(workspace->wpa3_rtos_info->event_queue));
        /* de-init timer */
        cy_rtos_deinit_timer(&(workspace->wpa3_sae_timer));
        cy_rtos_deinit_semaphore(&wpa3_scan_semaphore);

        free(workspace->wpa3_rtos_info);
        free(workspace);
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:cy_rtos_init_semaphore failed\n"));
        result = WPA3_EXT_SUPP_ERROR;
    }

    /* Get WHD interface */
    result =  cy_wcm_get_whd_interface((cy_wcm_interface_t)WPA3_STA_INTERFACE, &(workspace->interface));
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:WHD get interface failed result=%lx\n", result));
    }

    /* Initialize WPA3 SAE context info */
    result = wpa3_supplicant_init_sae_context_info(workspace);
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_init_sae_context_info failed\n"));

        mbedtls_mpi_free(&(workspace->wpa3_sae_context_info.peer_commit_scalar));
        mbedtls_ecp_point_free(&(workspace->wpa3_sae_context_info.peer_commit_element));

        /* de-init queue */
        cy_rtos_deinit_queue(&(workspace->wpa3_rtos_info->event_queue));
        /* de-init timer */
        cy_rtos_deinit_timer(&(workspace->wpa3_sae_timer));
        cy_rtos_deinit_semaphore(&wpa3_scan_semaphore);

        free(workspace->wpa3_rtos_info);
        free(workspace);
        result = WPA3_EXT_SUPP_ERROR;
    }

    /* Initialize crypto for WPA3 */
    result = wpa3_crypto_init(workspace);
    if ( result != CY_RSLT_SUCCESS )
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_crypto_init failed\n"));
        result = WPA3_EXT_CRYPTO_ERROR;
    }

    /* register callback with WCM */
    if ( wpa3_sae_wcm_registered_callback  == false)
    {
        result = cy_wcm_register_event_callback(wpa3_auth_join_callback);
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:cy_wcm_register_event_callback result=%ld\n", result));
        wpa3_sae_wcm_registered_callback = true;
    }

    *wksp = workspace;

    wpa3_sae_supplicant_deinit_done = false;
    wpa3_sae_workspace_cleanup_initiated = false;
    return result;
}

cy_rslt_t wpa3_supplicant_init_sae_context_info(wpa3_supplicant_workspace_t *wksp)
{
    if ( wksp == NULL )
    {
        return WPA3_EXT_SUPP_ERROR;
    }
    mbedtls_mpi_init(&(wksp->wpa3_sae_context_info.peer_commit_scalar));
    mbedtls_ecp_point_init(&(wksp->wpa3_sae_context_info.peer_commit_element));
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_supplicant_deinit_sae_context_info(wpa3_supplicant_workspace_t *wksp)
{
    if ( wksp == NULL )
    {
        return WPA3_EXT_SUPP_ERROR;
    }
    mbedtls_mpi_free(&(wksp->wpa3_sae_context_info.peer_commit_scalar));
    mbedtls_ecp_point_free(&(wksp->wpa3_sae_context_info.peer_commit_element));
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_supplicant_deinit_workspace(wpa3_supplicant_workspace_t *wksp)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if ( wksp == NULL )
    {
        return WPA3_EXT_SUPP_ERROR;
    }
    /* Enter to tast critical sections, to ensure that another tasks
     * (e.g WCM worker) will not interrupt during Workspace memory free.
     */
    taskENTER_CRITICAL();

    /* stop auth request */
    WHD_WIFI_STUB_IMPL(whd_wifi_stop_external_auth_request,_stub)(wksp->interface);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:whd_wifi_stop_external_auth_request done\n"));

    if ( wksp->wpa3_rtos_info != NULL )
    {
        /* join thread */
        if ( wksp->wpa3_rtos_info->thread_handle != NULL )
        {
             (void)cy_rtos_join_thread( &(wksp->wpa3_rtos_info->thread_handle) );
        }
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE thread join done\n"));

        /* delete thread stack */
        if ( wksp->wpa3_rtos_info->thread_stack != NULL )
        {
            free(wksp->wpa3_rtos_info->thread_stack);
        }
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE thread stack deleted \n"));
    }

    /* de-init sae context info */
    result =  wpa3_supplicant_deinit_sae_context_info(wksp);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE context info deleted\n"));

    /* de-init crypto */
    result = (cy_rslt_t)wpa3_crypto_deinit(wksp);
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE crypto context deleted\n"));

    /* de-init timer */
    result = cy_rtos_deinit_timer(&(wksp->wpa3_sae_timer));
    WPA3_EXT_LOG_MSG(("WPA3-EXT SAE timer de-inited\n"));

    /* de-init semaphore */
    result = cy_rtos_deinit_semaphore(&wpa3_scan_semaphore);
    WPA3_EXT_LOG_MSG(("WPA3-EXT SAE semaphore de-inited\n"));

    if ( wksp->wpa3_rtos_info != NULL )
    {
        /* de-init queue */
        result = cy_rtos_deinit_queue(&(wksp->wpa3_rtos_info->event_queue));
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE queue deleted\n"));

        /* delete rtos info */
        free(wksp->wpa3_rtos_info);
    }
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE RTOS information deleted\n"));

    /* delete workspace */

    if ( wksp != NULL )
    {
        free(wksp);
    }
    wpa3_sae_set_workspace(NULL);
    wpa3_sae_supplicant_deinit_done = true;

    taskEXIT_CRITICAL();

    if ( wpa3_sae_wcm_registered_callback == true)
    {
        result = cy_wcm_deregister_event_callback(wpa3_auth_join_callback);
        WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:SAE cy_wcm_deregister_event_callback result=%ld*** \n", result));
        wpa3_sae_wcm_registered_callback = false;
    }
    WPA3_EXT_LOG_MSG(("***WPA3-EXT-SUPP:SAE workspace deleted*** \n"));
    return result;
}

cy_rslt_t wpa3_supplicant_send_event( wpa3_supplicant_workspace_t* workspace, wpa3_supplicant_event_message_t* msg )
{
    cy_rslt_t result;
    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_supplicant_send_event called "));
    wpa3_print_event(msg->event_type);
    result = cy_rtos_put_queue( &(workspace->wpa3_rtos_info->event_queue), msg, WPA3_SUPPLICANT_WAIT_FOREVER, 0 );
    if(result != CY_RSLT_SUCCESS)
    {
        return WPA3_EXT_SUPP_ERROR;
    }
    return CY_RSLT_SUCCESS;
}

/* Callback function for WLC_E_EXT_AUTH_REQ */
void wpa3_auth_req_callbackfunc ( void *result_ptr, uint32_t len, whd_auth_status_t status, uint8_t* flag, void *user_data)
{
    wpa3_supplicant_workspace_t* wksp = (wpa3_supplicant_workspace_t*)user_data;
    wpa3_supplicant_event_message_t msg;

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_auth_req_callbackfunc entered wksp:%p h2e_support=%d len=%ld\n", wksp, wksp->wpa3_h2e_capable, len));
    if (( result_ptr != NULL ) && ( len > 0 ))
    {
        msg.length = len;
        msg.data = malloc(len);
        if ( msg.data == NULL )
        {
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: alloc failed\n"));
        }
        else
        {
            if ( wpa3_sae_workspace_cleanup_initiated == false )
            {
                if (status == WHD_AUTH_EXT_REQ)
                {
                    whd_auth_req_status_t *auth_req = (whd_auth_req_status_t *)msg.data;
                    msg.event_type = WPA3_SUPPLICANT_EVENT_AUTH_REQ;

                    memcpy(auth_req, result_ptr, len);
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:auth_req->flags:%x\n", auth_req->flags));
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:auth_req->peer_mac %02x:%02x:%02x:%02x:%02x:%02x\n",
                                     auth_req->peer_mac.octet[0], auth_req->peer_mac.octet[1],
                                     auth_req->peer_mac.octet[2], auth_req->peer_mac.octet[3],
                                     auth_req->peer_mac.octet[4], auth_req->peer_mac.octet[5]));
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:ssid_len=%ld\n", auth_req->ssid_len));
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:ssid and PMK dump\n"));
                    WPA3_EXT_HEX_BUF_DUMP((auth_req->ssid, auth_req->ssid_len));
                    WPA3_EXT_HEX_BUF_DUMP((auth_req->pmkid, sizeof(auth_req->pmkid)));
                }
                else
                {
                    memcpy(msg.data, result_ptr, len);
                    msg.event_type = WPA3_SUPPLICANT_EVENT_AUTH_RX_FRAME;
                    /* stop timer */
                    cy_rtos_stop_timer(&(wksp->wpa3_sae_timer));
                }
                /* post an event  WPA3_SUPPLICANT_EVENT_AUTH_REQ to supplicant */
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:auth_status=%d ..calling wpa3_supplicant_send_event\n", status));
                wpa3_supplicant_send_event(wksp, &msg);
            } /* end of if */
         } /* end of else */
    }
}

void wpa3_sae_cleanup_workspace(void)
{
    wpa3_supplicant_workspace_t* wksp = NULL;
    wpa3_supplicant_event_message_t msg;

    wksp = wpa3_sae_get_workspace();
    if ( ( wksp != NULL ) && (wpa3_sae_workspace_cleanup_initiated == false))
    {
          wpa3_sae_workspace_cleanup_initiated = true;
          /* delete all context info */
          WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_cleanup_workspace WPA3 workspace is not NULL \n"));

          msg.length = 0;
          msg.data = NULL;
          msg.event_type = WPA3_SUPPLICANT_EVENT_DELETE;
          wpa3_supplicant_send_event(wksp, &msg);

          WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:WPA3_SUPPLICANT_EVENT_DELETE posted \n"));
          do
          {
              cy_rtos_delay_milliseconds(WPA3_SAE_EXIT_THREAD_DELAY);
          } while ( wksp->wpa3_sae_statemachine_exit == false);

          /* delete all context info */
          wpa3_supplicant_deinit_workspace(wksp);
    }
}

void wpa3_auth_join_callback (cy_wcm_event_t event, cy_wcm_event_data_t *event_data)
{
    wpa3_supplicant_workspace_t* wksp = NULL;

    wksp = wpa3_sae_get_workspace();
    switch(event)
    {
        case CY_WCM_EVENT_CONNECTING:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_CONNECTING\n"));
            if ( wksp != NULL )
            {
                wpa3_sae_cleanup_workspace();
            }
            break;
        case CY_WCM_EVENT_CONNECTED:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_CONNECTED\n"));
            break;
        case CY_WCM_EVENT_CONNECT_FAILED:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_CONNECT_FAILED\n"));
            if (wksp != NULL )
            {
                wpa3_sae_cleanup_workspace();
            }
            break;
        case CY_WCM_EVENT_RECONNECTED:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_RECONNECTED\n"));
            break;
        case CY_WCM_EVENT_DISCONNECTED:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_DISCONNECTED\n"));
            break;
        case CY_WCM_EVENT_IP_CHANGED:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_CONNECTING\n"));
            break;
        case CY_WCM_EVENT_INITIATED_RETRY:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_INITIATED_RETRY\n"));
            break;
        case CY_WCM_EVENT_STA_JOINED_SOFTAP:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_STA_JOINED_SOFTAP\n"));
            break;
        case CY_WCM_EVENT_STA_LEFT_SOFTAP:
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:CY_WCM_EVENT_STA_LEFT_SOFTAP\n"));
            break;
        default:
            break;
    }
}

void wpa3_sae_timer_expiry(cy_timer_callback_arg_t arg)
{
    wpa3_supplicant_event_message_t msg;
    wpa3_supplicant_workspace_t* wksp = NULL;

    wksp = wpa3_sae_get_workspace();

    if ( wksp == NULL )
    {
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_timer_expiry wksp is NULL\n"));
        return;
    }

    /* increment Sync and Sc */
    wksp->wpa3_sae_context_info.sc++;
    wksp->wpa3_sae_sync++;

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_timer_expiry Expired timer retry=%ld sc=%d\n", wksp->wpa3_sae_sync, wksp->wpa3_sae_context_info.sc));

    cy_rtos_stop_timer(&(wksp->wpa3_sae_timer));

    if (( wksp->wpa3_sae_rx_handshake_fail == true ) || ( wksp->wpa3_sae_handshake_success == true ))
    {
        /* delete all context info */
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_timer_expiry handshake failed/success \n"));
        wksp->wpa3_sae_rx_handshake_fail = false;
        if ( wpa3_sae_supplicant_deinit_done == false)
        {
            wpa3_supplicant_deinit_workspace(wksp);
        }
    }
    else if ( ( wksp->wpa3_sae_sync >= WPA3_SAE_MAX_RETRANS_ATTEMTPS) &&
            ( wpa3_sae_supplicant_deinit_done == false ))
    {
       /* delete all context info */
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_timer_expiry exhausted max retries \n"));
        wpa3_sae_cleanup_workspace();
    }
    else if ( wpa3_sae_supplicant_deinit_done == false)
    {
        /* restart the timer */
        cy_rtos_start_timer(&(wksp->wpa3_sae_timer), WPA3_SAE_HANDSHAKE_TIMEOUT_MS);
        /* post timeout event */
        msg.length = 0;
        msg.data = NULL;
        msg.event_type = WPA3_SUPPLICANT_EVENT_TIMEOUT;
        wpa3_supplicant_send_event(wksp, &msg);
    }
}

wpa3_supplicant_workspace_t* wpa3_sae_get_workspace (void )
{
    return g_workspace;
}

void wpa3_sae_set_workspace( wpa3_supplicant_workspace_t *wksp)
{
    g_workspace = wksp;
}

static void wpa3_sae_scan_callback(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    wpa3_supplicant_workspace_t* wksp = (wpa3_supplicant_workspace_t*)user_data;

    if ( wpa3_sae_scan_in_progress == true )
    {
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback Entered status=%d result_ptr=%p\n", status, result_ptr));

        if ( result_ptr != NULL )
        {
            whd_scan_result_t *scan_result = *result_ptr;
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback status=%d \n", status));

            result =  memcmp(scan_result->SSID.value, wksp->wpa3_sae_auth_info.ssid, wksp->wpa3_sae_auth_info.ssid_len );
            if ( result == CY_RSLT_SUCCESS)
            {
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback AP BSSID:%02x:%02x:%02x:%02x:%02x:%02x",
                                 scan_result->BSSID.octet[0], scan_result->BSSID.octet[1], scan_result->BSSID.octet[2],
                                 scan_result->BSSID.octet[3], scan_result->BSSID.octet[4], scan_result->BSSID.octet[5]));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback security=%x", scan_result->security));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback band=%d", scan_result->band));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback ccode=%x%x", scan_result->ccode[0], scan_result->ccode[1]));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback channel=%x", scan_result->channel));
                if ( (scan_result->flags) & WHD_SCAN_RESULT_FLAG_SAE_H2E )
                {
                    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:H2E Capable AP:%x", scan_result->flags));
                    wksp->wpa3_h2e_capable = true;
                    wksp->wpa3_h2e_method = true;
                }
                else
                {
                    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:HnP Capable AP:%x", scan_result->flags));
                }
           }
        }

       if ( ( status == WHD_SCAN_COMPLETED_SUCCESSFULLY ) || (status == WHD_SCAN_ABORTED))
       {
            result = cy_rtos_set_semaphore(&wpa3_scan_semaphore, false);
            if (result != CY_RSLT_SUCCESS)
            {
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:wpa3_sae_scan_callback unable to set semaphore \n"));
            }
            wpa3_sae_scan_in_progress = false;
       }
    }
}
