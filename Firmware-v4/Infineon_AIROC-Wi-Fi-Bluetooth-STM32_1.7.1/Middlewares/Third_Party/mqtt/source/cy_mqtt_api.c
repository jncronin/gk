/*
 * Copyright 2023, Cypress Semiconductor Corporation (an Infineon company) or
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
 *  Implements MQTT wrapper APIs to perform MQTT CONNECT, DISCONNET, PUBLISH, SUBSCRIBE, and UNSUBSCRIBE.
 *
 */
#include <string.h>
#include <stdlib.h>
#include "cy_mqtt_api.h"
#include "cyabs_rtos.h"
/******************************************************
 *                      Macros
 ******************************************************/
#ifdef ENABLE_MQTT_LOGS
#define cy_mqtt_log_msg cy_log_msg
#else
#define cy_mqtt_log_msg(a,b,c,...)
#endif

#if defined(ENABLE_MULTICORE_CONN_MW) && defined(USE_VIRTUAL_API)

#include "cy_mqtt_api_internal.h"
#include "cy_vcm_internal.h"
#include "cyhal_ipc.h"

typedef struct mqtt_cb_data_base
{
    cy_mqtt_t            mqtt_handle;
    cy_mqtt_callback_t   mqtt_usr_cb;
    void*                mqtt_usr_data;
} mqtt_cb_data_base_t;

static mqtt_cb_data_base_t  mqtt_handle_cb_database[ CY_MQTT_MAX_HANDLE ]; /* Database to store mapping of mqtt handle and the registered user callback and data for it. */
static int mqtt_handle_index = 0;                                          /* Next available index in the database to store callback info for a handle. */
static cy_mutex_t cb_database_mutex;                                       /* Mutex to provide thread-safe access to the callback databse. */
static bool is_mqtt_virtual_library_initialized = false;                   /* Indicates whether the MQTT library is initialized or not in the secondary core. */


static void virtual_event_handler(void *arg)
{
    cy_rslt_t res;
    cy_mqtt_callback_params_t  *mqtt_event_params;
    cy_mqtt_t mqtt_handle;
    int i = 0;

    mqtt_event_params = (cy_mqtt_callback_params_t *)arg;
    mqtt_handle = mqtt_event_params->mqtt_handle;

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Acquiring Mutex %p \n", cb_database_mutex );
    res = cy_rtos_get_mutex( &cb_database_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", cb_database_mutex, (unsigned int)res );
        /* Freeing the memory allocated in local_mqtt_event_cb function in VCM */
        cy_vcm_free((void*)mqtt_event_params->event.data.pub_msg.received_message.payload);
        cy_vcm_free((void*)mqtt_event_params->event.data.pub_msg.received_message.topic);
        return;
    }

    for( i = 0; i < CY_MQTT_MAX_HANDLE; i++ )
    {
        if( mqtt_handle_cb_database[i].mqtt_handle == mqtt_handle )
        {
            cy_mqtt_callback_t evt_cb = mqtt_handle_cb_database[i].mqtt_usr_cb;
            if( evt_cb != NULL )
            {
                evt_cb(mqtt_event_params->mqtt_handle, mqtt_event_params->event, mqtt_handle_cb_database[i].mqtt_usr_data);
                break;
            }
        }
    }

    /* Freeing the memory allocated in local_mqtt_event_cb function in VCM */
    cy_vcm_free((void*)mqtt_event_params->event.data.pub_msg.received_message.payload);
    cy_vcm_free((void*)mqtt_event_params->event.data.pub_msg.received_message.topic);

    res = cy_rtos_set_mutex( &cb_database_mutex );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", cb_database_mutex, (unsigned int)res );
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Releasing Mutex %p \n", cb_database_mutex );
}

/* This section is virtual-only implementation.
 * The below APIs send the API request to the other core via IPC using the Virtual Connectivity Manager (VCM) library.
 */

cy_rslt_t cy_mqtt_init( void )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    int i = 0;

    if ( is_mqtt_virtual_library_initialized == true )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is already initialized. \n", res );
        return res;
    }

    res = cy_rtos_init_mutex2(&cb_database_mutex, false);
    if ( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Failed to initialize mutex. Res: 0x%x\n", res );
        return res;
    }

    for ( i = 0; i < CY_MQTT_MAX_HANDLE; i++ )
    {
        (void)memset(&mqtt_handle_cb_database[i], 0x00, sizeof(mqtt_cb_data_base_t));
    }

    mqtt_handle_index = 0;
    is_mqtt_virtual_library_initialized = true;

    return res;
}

cy_rslt_t cy_mqtt_get_handle( cy_mqtt_t *mqtt_handle, char *descriptor )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_rslt_t *api_res = NULL;

    cy_vcm_request_t api_req;
    cy_vcm_response_t api_resp;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_get_handle_params_t params;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_t _handle;

    if( mqtt_handle == NULL || descriptor == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_get_handle()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( strlen(descriptor) > CY_MQTT_DESCP_MAX_LEN )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Descriptor length is greater than maximum permissible length: %u!\n", (uint16_t)CY_MQTT_DESCP_MAX_LEN );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    params.mqtt_handle = &(_handle);
    memcpy( &params.descriptor, descriptor, strlen(descriptor) + 1 );

    /* Set API request */
    memset(&api_req, 0, sizeof(cy_vcm_request_t));
    api_req.api_id = CY_VCM_API_MQTT_GET_HANDLE;
    api_req.params = &params;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_req, &api_resp);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_vcm_send_api_request failed for cy_mqtt_get_handle. Res: %u \n", res);
        *mqtt_handle = NULL;
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    /* Get API result */
    api_res = (cy_rslt_t*)(api_resp.result);
    if( api_res == NULL )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_mqtt_get_handle API response is NULL! \n");
        *mqtt_handle = NULL;
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    *mqtt_handle = _handle;
    return *api_res;
}

cy_rslt_t cy_mqtt_register_event_callback( cy_mqtt_t mqtt_handle,
                                           cy_mqtt_callback_t event_callback,
                                           void *user_data )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_rslt_t *api_res = NULL;
    int i = 0;
    bool handle_found = false;

    cy_vcm_request_t api_req;
    cy_vcm_response_t api_resp;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_register_event_callback_params_t params;

    if( mqtt_handle == NULL || event_callback == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_register_event_callback()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n cy_mqtt_register_event_callback - Acquiring Mutex %p \n", cb_database_mutex );
    res = cy_rtos_get_mutex( &cb_database_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", cb_database_mutex, (unsigned int)res );
        return res;
    }

    if( mqtt_handle_index == (CY_MQTT_MAX_HANDLE) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nExceeded maximum number of MQTT handles!\n" );
        cy_rtos_set_mutex( &cb_database_mutex );
        return CY_RSLT_MODULE_MQTT_NOMEM;
    }

    /*Look for the MQTT handle. If already present, replace the previously registered callback. */
    for( i = 0; i < mqtt_handle_index; i++ )
    {
        if( mqtt_handle_cb_database[i].mqtt_handle == mqtt_handle )
        {
            mqtt_handle_cb_database[i].mqtt_usr_cb = event_callback;
            mqtt_handle_cb_database[i].mqtt_usr_data = user_data;

            handle_found = true;
            break;
        }
    }

    /* If MQTT handle is not found in the callback database, add a new entry for this handle. */
    if( handle_found == false )
    {
        mqtt_handle_cb_database[mqtt_handle_index].mqtt_handle = mqtt_handle;
        mqtt_handle_cb_database[mqtt_handle_index].mqtt_usr_cb = event_callback;
        mqtt_handle_cb_database[mqtt_handle_index].mqtt_usr_data = user_data;

        mqtt_handle_index++;
    }

    res = cy_rtos_set_mutex( &cb_database_mutex );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", cb_database_mutex, (unsigned int)res );
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n cy_mqtt_register_event_callback - Releasing Mutex %p \n", cb_database_mutex );

    if( handle_found == true )
    {
        return CY_RSLT_SUCCESS;
    }

    params.mqtt_handle = mqtt_handle;
    params.event_callback = virtual_event_handler;
    params.user_data = user_data;

    /* Set API request */
    memset(&api_req, 0, sizeof(cy_vcm_request_t));
    api_req.api_id = CY_VCM_API_MQTT_REG_EVENT_CB;
    api_req.params = &params;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_req, &api_resp);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_vcm_send_api_request failed for cy_mqtt_register_event_callback. Res: %u \n", res);
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    /* Get API result */
    api_res = (cy_rslt_t*)(api_resp.result);
    if( api_res == NULL )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_mqtt_register_event_callback API response is NULL! \n");
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    return *api_res;
}

cy_rslt_t cy_mqtt_deregister_event_callback( cy_mqtt_t mqtt_handle,
                                             cy_mqtt_callback_t event_callback)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_rslt_t *api_res = NULL;
    int i, j;
    bool cb_found = false;

    cy_vcm_request_t api_req;
    cy_vcm_response_t api_resp;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_deregister_event_callback_params_t params;

    if( mqtt_handle == NULL || event_callback == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_deregister_event_callback()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    if( mqtt_handle_index == 0 )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n No callback registered!\n" );
        cy_rtos_set_mutex( &cb_database_mutex );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n cy_mqtt_deregister_event_callback - Acquiring Mutex %p \n", cb_database_mutex );
    res = cy_rtos_get_mutex( &cb_database_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", cb_database_mutex, (unsigned int)res );
        return res;
    }

    /*Look for the MQTT handle. */
    for( i = 0; i < mqtt_handle_index; i++ )
    {
        if( mqtt_handle_cb_database[i].mqtt_handle == mqtt_handle && mqtt_handle_cb_database[i].mqtt_usr_cb == event_callback )
        {
            /* If callback is found, move the entries after it by 1 to the left. */
            for( j = i+1; j < mqtt_handle_index; j++ )
            {
                mqtt_handle_cb_database[j-1].mqtt_usr_cb = mqtt_handle_cb_database[j].mqtt_usr_cb;
                mqtt_handle_cb_database[j-1].mqtt_usr_data = mqtt_handle_cb_database[j].mqtt_usr_data;
                mqtt_handle_cb_database[j-1].mqtt_handle = mqtt_handle_cb_database[j].mqtt_handle ;
            }
            /* Set last entry to NULL. */
            mqtt_handle_cb_database[mqtt_handle_index].mqtt_usr_cb = NULL;
            mqtt_handle_cb_database[mqtt_handle_index].mqtt_usr_data = NULL;
            mqtt_handle_cb_database[mqtt_handle_index].mqtt_handle = NULL;

            mqtt_handle_index -= 1;
            cb_found = true;
            break;
        }
    }

    res = cy_rtos_set_mutex( &cb_database_mutex );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", cb_database_mutex, (unsigned int)res );
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n cy_mqtt_register_event_callback - Releasing Mutex %p \n", cb_database_mutex );

    /* If callback is not found in the callback database, return error. */
    if( cb_found == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_deregister_event_callback(). Callback not found for handle: %p!\n", mqtt_handle );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    params.mqtt_handle = mqtt_handle;
    params.event_callback = virtual_event_handler;

    /* Set API request */
    memset(&api_req, 0, sizeof(cy_vcm_request_t));
    api_req.api_id = CY_VCM_API_MQTT_DEREG_EVENT_CB;
    api_req.params = &params;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_req, &api_resp);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_vcm_send_api_request failed for cy_mqtt_register_event_callback. Res: %u \n", res);
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    /* Get API result */
    api_res = (cy_rslt_t*)(api_resp.result);
    if( api_res == NULL )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_mqtt_register_event_callback API response is NULL! \n");
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    return *api_res;
}

cy_rslt_t cy_mqtt_publish( cy_mqtt_t mqtt_handle, cy_mqtt_publish_info_t *pubmsg )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_rslt_t *api_res = NULL;
    cy_vcm_request_t api_req;
    cy_vcm_response_t api_resp;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_publish_params_t params;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_publish_info_t _pubmsg;

    if( (mqtt_handle == NULL) || (pubmsg == NULL) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_publish()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    memcpy(&(_pubmsg), pubmsg, sizeof(cy_mqtt_publish_info_t));
    params.mqtt_handle = mqtt_handle;
    params.pub_msg = &(_pubmsg);

    /* Set API request */
    memset(&api_req, 0, sizeof(cy_vcm_request_t));
    api_req.api_id = CY_VCM_API_MQTT_PUBLISH;
    api_req.params = &params;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_req, &api_resp);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_vcm_send_api_request failed for cy_mqtt_publish. Res: %u \n", res);
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    /* Get API result */
    api_res = (cy_rslt_t*)(api_resp.result);
    if( api_res == NULL )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_mqtt_publish API response is NULL! \n");
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    return *api_res;
}

cy_rslt_t cy_mqtt_subscribe( cy_mqtt_t mqtt_handle, cy_mqtt_subscribe_info_t *sub_info, uint8_t sub_count  )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_rslt_t *api_res = NULL;
    cy_vcm_request_t api_req;
    cy_vcm_response_t api_resp;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_subscribe_params_t params;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_subscribe_info_t _sub_info;

    if( (mqtt_handle == NULL) || (sub_info == NULL) || (sub_count < 1) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_subscribe()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    memcpy(&(_sub_info), sub_info, sizeof(cy_mqtt_subscribe_info_t));
    params.mqtt_handle = mqtt_handle;
    params.sub_info = &_sub_info;
    params.sub_count = sub_count;

    /* Set API request */
    memset(&api_req, 0, sizeof(cy_vcm_request_t));
    api_req.api_id = CY_VCM_API_MQTT_SUBSCRIBE;
    api_req.params = &params;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_req, &api_resp);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_vcm_send_api_request failed for cy_mqtt_subscribe. Res: %u \n", res);
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    /* Get API result */
    api_res = (cy_rslt_t*)(api_resp.result);
    if( api_res == NULL )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_mqtt_subscribe API response is NULL! \n");
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    return *api_res;
}

cy_rslt_t cy_mqtt_unsubscribe( cy_mqtt_t mqtt_handle, cy_mqtt_unsubscribe_info_t *unsub_info, uint8_t unsub_count )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_rslt_t *api_res = NULL;
    cy_vcm_request_t api_req;
    cy_vcm_response_t api_resp;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_unsubscribe_params_t params;
    CY_SECTION_SHAREDMEM
    static cy_mqtt_unsubscribe_info_t _unsub_info;

    if( (mqtt_handle == NULL) || (unsub_info == NULL) || (unsub_count < 1) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_unsubscribe()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    memcpy(&(_unsub_info), unsub_info, sizeof(cy_mqtt_unsubscribe_info_t));

    params.mqtt_handle = mqtt_handle;
    params.unsub_info = &(_unsub_info);
    params.unsub_count = unsub_count;

    /* Set API request */
    memset(&api_req, 0, sizeof(cy_vcm_request_t));
    api_req.api_id = CY_VCM_API_MQTT_UNSUBSCRIBE;
    api_req.params = &params;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_req, &api_resp);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_vcm_send_api_request failed for cy_mqtt_unsubscribe. Res: %u \n", res);
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    /* Get API result */
    api_res = (cy_rslt_t*)(api_resp.result);
    if( api_res == NULL )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : cy_mqtt_unsubscribe API response is NULL! \n");
        return CY_RSLT_MODULE_MQTT_VCM_ERROR;
    }

    return *api_res;
}

cy_rslt_t cy_mqtt_deinit( void )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if ( is_mqtt_virtual_library_initialized == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT library is not initialized on secondary core. Call cy_mqtt_init() first. \n", res );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    res = cy_rtos_deinit_mutex( &cb_database_mutex );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_deinit_mutex failed with Error : [0x%X] \n", (unsigned int)res );
        return res;
    }

    is_mqtt_virtual_library_initialized = false;

    return res;
}

#elif !defined(ENABLE_MULTICORE_CONN_MW) || (defined(ENABLE_MULTICORE_CONN_MW) && !defined(USE_VIRTUAL_API))
/* This section is full-stack implementation. */

#include "cy_utils.h"


/**
 * Timeout for receiving CONNACK packet in milliseconds.
 */
#define CY_MQTT_CONNACK_RECV_TIMEOUT_MS                      ( 2000U )

/**
 * Network socket receive timeout in milliseconds.
 */
#ifdef COMPONENT_CAT5
/* On CAT5 devices(H1-CP), the tick is configured for 10ms,
 * so the smallest unit of time that can be configured is 10ms.
 */
#define CY_MQTT_SOCKET_RECEIVE_TIMEOUT_MS                    ( 10U )
#else
#define CY_MQTT_SOCKET_RECEIVE_TIMEOUT_MS                    ( 1U )
#endif

/**
 * Timeout in milliseconds for ProcessLoop.
 */
#define CY_MQTT_RECEIVE_DATA_TIMEOUT_MS                      ( 0U )

/**
 * Receive thread sleep time in milliseconds.
 */
#define CY_MQTT_SEC_TO_MSEC_CONVERTOR                         ( 1000 )

#define CY_MQTT_EVENT_THREAD_PRIORITY                        ( CY_RTOS_PRIORITY_NORMAL )

#define CY_MQTT_EVENT_QUEUE_SIZE                             ( 30U )

#define CY_MQTT_EVENT_QUEUE_TIMEOUT_IN_MSEC                  ( 500UL )

#define CY_MQTT_MAGIC_HEADER                                 ( 0xbdefacbd )
#define CY_MQTT_MAGIC_FOOTER                                 ( 0xefbcdbfd )

#define CY_MQTT_MAX_EVENT_CALLBACKS                          (2)
/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/
/**
 * MQTT socket event types.
 */
typedef enum cy_mqtt_socket_event
{
    CY_MQTT_SOCKET_EVENT_DATA_RECEIVE                  = 0, /**< Data receive event from socket */
    CY_MQTT_SOCKET_EVENT_DISCONNECT                    = 1, /**< Disconnection event from socket */
    CY_MQTT_SOCKET_EVENT_PING_REQ                      = 2, /**< MQTT ping timer event */
    CY_MQTT_SOCKET_EVENT_EXIT_THREAD                   = 3  /**< Terminate mqtt_event_processing_thread event from mqtt_deinit */
} cy_mqtt_socket_event_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/**
 * Structure to keep the MQTT PUBLISH packets until an ACK is received
 * for QoS1 and QoS2 publishes.
 */
typedef struct publishpackets
{
    uint16_t               packetid;
    MQTTPublishInfo_t      pubinfo;
} cy_mqtt_pubpack_t;

/**
 * Structure to keep the MQTT PUBLISH packet ACK information
 * for QoS1 and QoS2 publishes.
 */
typedef struct publishpacket_ackstatus
{
    uint16_t      packetid;
    bool          puback_status;
} cy_mqtt_pub_ack_status_t;

/*
 * MQTT handle
 */
typedef struct mqtt_object
{
    uint32_t                        mqtt_magic_header;         /**< Magic header to verify the mqtt object */
    bool                            mqtt_secure_mode;          /**< MQTT secured mode. True if secure connection; false otherwise. */
    bool                            mqtt_session_established;  /**< MQTT client session establishment status. */
    bool                            broker_session_present;    /**< Broker session status. */
    bool                            mqtt_conn_status;          /**< MQTT network connect status. */
    uint8_t                         mqtt_obj_index;            /**< MQTT object index in mqtt_handle_database. */
    NetworkContext_t                network_context;           /**< MQTT Network context. */
    MQTTContext_t                   mqtt_context;              /**< MQTT context. */
    cy_awsport_server_info_t        server_info;               /**< MQTT broker info. */
    cy_awsport_ssl_credentials_t    security;                  /**< MQTT secure connection credentials. */
    cy_mqtt_callback_t              mqtt_event_cb[ CY_MQTT_MAX_EVENT_CALLBACKS]; /**< MQTT application callback for events. */
    MQTTSubAckStatus_t              sub_ack_status[ CY_MQTT_MAX_OUTGOING_SUBSCRIBES ]; /**< MQTT SUBSCRIBE command ACK status. */
    uint8_t                         num_of_subs_in_req;        /**< Number of subscription messages in outstanding MQTT subscribe request. */
    bool                            unsub_ack_received;        /**< Status of unsubscribe acknowledgment. */
    cy_mqtt_pub_ack_status_t        pub_ack_status;            /**< MQTT PUBLISH packetack received status. */
    uint16_t                        sent_packet_id;            /**< MQTT packet ID. */
    cy_mqtt_pubpack_t               outgoing_pub_packets[ CY_MQTT_MAX_OUTGOING_PUBLISHES ]; /**< MQTT PUBLISH packet. */
    cy_mutex_t                      process_mutex;             /**< Mutex for synchronizing MQTT object members. */
    cy_timer_t                      mqtt_timer;                /**< RTOS timer to handle the MQTT ping request */
    cy_timer_t                      mqtt_ping_resp_timer;      /**< RTOS timer to handle the MQTT ping response timeout */
    void                            *user_data[ CY_MQTT_MAX_EVENT_CALLBACKS ];                /**< User data which needs to be sent while calling registered app callback. */
    uint16_t                        keepAliveSeconds;          /**< MQTT keep alive timeout in seconds. */
    uint32_t                        mqtt_magic_footer;         /**< Magic footer to verify the mqtt object */
    char                            mqtt_descriptor[ CY_MQTT_DESCP_MAX_LEN + 1 ]; /**< Descriptor used by the application to identify the mqtt handle. */
} cy_mqtt_object_t ;

/*
 * MQTT handle database
 */
typedef struct mqtt_data_base
{
    cy_mqtt_t       *mqtt_handle;
    MQTTContext_t   *mqtt_context;
} mqtt_data_base_t ;

/*
 * Structure to store the socket event and mqtt_obj which will be
 * processed by mqtt_event_processing_thread
 */
typedef struct cy_mqtt_callback_event
{
    cy_mqtt_socket_event_t       socket_event;      /**< Socket event */
    cy_mqtt_object_t             *mqtt_obj;         /**< MQTT Object */
} cy_mqtt_callback_event_t;

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/
static mqtt_data_base_t  mqtt_handle_database[ CY_MQTT_MAX_HANDLE ];
static uint8_t           mqtt_handle_count = 0;
static cy_mutex_t        mqtt_db_mutex;
static bool              mqtt_lib_init_status = false;
static bool              mqtt_db_mutex_init_status = false;
static cy_thread_t       mqtt_event_process_thread = NULL;
static cy_queue_t        mqtt_event_queue;
/******************************************************
 *               Function Definitions
 ******************************************************/
 /* Check validity of the mqtt_obj by verifying mqtt_magic_header and mqtt_magic_footer associated with the object */
static bool is_mqtt_obj_valid( cy_mqtt_object_t *mqtt_obj )
{
    if( (mqtt_obj->mqtt_magic_header == CY_MQTT_MAGIC_HEADER) && (mqtt_obj->mqtt_magic_footer == CY_MQTT_MAGIC_FOOTER) )
    {
        return true;
    }
    return false;
}

/* stop_timer must be protected under mqtt_obj->process_mutex */
static cy_rslt_t stop_timer( cy_mqtt_object_t *mqtt_obj )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    bool      state = false;

    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to stop_timer \n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }
    if( mqtt_obj->keepAliveSeconds != 0 )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nStopping the timer\n" );
        result = cy_rtos_is_running_timer( &mqtt_obj->mqtt_timer, &state );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_rtos_is_running_timer failed\n" );
            return result;
        }
        if( state == true )
        {
            /* Timer running */
            result = cy_rtos_stop_timer( &mqtt_obj->mqtt_timer );
            if( result != CY_RSLT_SUCCESS )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_rtos_stop_timer failed\n" );
            }
        }
    }
    return result;
}

/* start_timer must be protected under mqtt_obj->process_mutex */
static cy_rslt_t start_timer( cy_mqtt_object_t *mqtt_obj )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint8_t   retry_count = 0;

    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to start_timer \n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }
    if( mqtt_obj->keepAliveSeconds != 0 )
    {
        do
        {
            result = cy_rtos_start_timer( &mqtt_obj->mqtt_timer, mqtt_obj->keepAliveSeconds * CY_MQTT_SEC_TO_MSEC_CONVERTOR );
            if( result == CY_RSLT_SUCCESS )
            {
                return result;
            }
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_start_timer failed with Error : [0x%X] \n", (unsigned int)result );
            cy_rtos_delay_milliseconds(10);
            retry_count++;
        }
        while( retry_count < CY_MQTT_MAX_RETRY_VALUE );
    }
    return result;
}

/*
 * start_mqtt_ping_resp_timer
 *
 * Start reponse timer for mqtt ping response.
 */
static cy_rslt_t start_mqtt_ping_resp_timer( cy_mqtt_object_t *mqtt_obj )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to start_mqtt_ping_resp_timer \n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    result = cy_rtos_start_timer( &mqtt_obj->mqtt_ping_resp_timer, MQTT_PINGRESP_TIMEOUT_MS );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR,
                "\nMqtt ping resp timer start failed with Error : [0x%X] \n", (unsigned int)result );
    }
    return result;
}

/*
 * stop_mqtt_ping_resp_timer
 *
 * Stop mqtt ping reponse timer.
 */
static cy_rslt_t stop_mqtt_ping_resp_timer( cy_mqtt_object_t *mqtt_obj )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    bool      state = false;

    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to stop_mqtt_ping_resp_timer \n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }
    /* stop response timer */
    result = cy_rtos_is_running_timer( &mqtt_obj->mqtt_ping_resp_timer, &state );
    if( result == CY_RSLT_SUCCESS && state == true )
    {
        result = cy_rtos_stop_timer( &mqtt_obj->mqtt_ping_resp_timer );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPing response timer stop failed\n" );
        }
    }
    return result;
}

/*
 * mqtt_pingresp_timeout_callback
 *
 * Callback if no ping response received in time.
 */
static void mqtt_pingresp_timeout_callback( void *arg )
{
    cy_rslt_t                res = CY_RSLT_SUCCESS;
    cy_mqtt_callback_event_t event;
    cy_mqtt_object_t         *mqtt_obj = NULL;

    if( arg == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO,
                "\nInvalid handle to mqtt_awsport_network_receive_callback, so nothing to do..!\n" );
        return;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO,
                    "\nMQTT Keepalive Timeout\n" );

    mqtt_obj = ( cy_mqtt_object_t * )arg;

    /* Queue Disconnect event */
    event.socket_event = CY_MQTT_SOCKET_EVENT_DISCONNECT;
    event.mqtt_obj = mqtt_obj;

    res = cy_rtos_put_queue( &mqtt_event_queue, (void *)&event, CY_MQTT_EVENT_QUEUE_TIMEOUT_IN_MSEC, false );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nPushing to MQTT event to mqtt_event_queue failed with Error : [0x%X] \n",
                        (unsigned int)res );
    }
    return;
}

static void mqtt_ping_request_callback( cy_mqtt_object_t *mqtt_obj )
{
    cy_rslt_t                  result = CY_RSLT_SUCCESS;
    cy_mqtt_callback_event_t   event;

    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to mqtt_ping_request_callback \n" );
        return;
    }

    event.socket_event = CY_MQTT_SOCKET_EVENT_PING_REQ;
    event.mqtt_obj = mqtt_obj;

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPushing mqtt_ping_request event to the mqtt_event_queue. \n" );

    result = cy_rtos_put_queue( &mqtt_event_queue, (void *)&event, CY_MQTT_EVENT_QUEUE_TIMEOUT_IN_MSEC, false );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nPushing mqtt_ping_request event to the mqtt_event_queue failed with Error : [0x%X] \n", (unsigned int)result );
    }
    return;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_cleanup_outgoing_publish( cy_mqtt_object_t *mqtt_obj, uint8_t index )
{
    if( index >= CY_MQTT_MAX_OUTGOING_PUBLISHES )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to mqtt_cleanup_outgoing_publish.\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }
    /* Clear the outgoing PUBLISH packet. */
    ( void ) memset( &( mqtt_obj->outgoing_pub_packets[ index ] ), 0x00, sizeof( mqtt_obj->outgoing_pub_packets[ index ] ) );
    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_cleanup_outgoing_publish_with_packet_id( cy_mqtt_object_t *mqtt_obj, uint16_t packetid )
{
    cy_rslt_t  result = CY_RSLT_SUCCESS;
    uint8_t    index;

    if( (mqtt_obj == NULL) || (packetid == MQTT_PACKET_ID_INVALID) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to mqtt_cleanup_outgoing_publish_with_packet_id.\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    /* Clean up all saved outgoing PUBLISH packets. */
    for( index = 0; index < CY_MQTT_MAX_OUTGOING_PUBLISHES; index++ )
    {
        if( mqtt_obj->outgoing_pub_packets[ index ].packetid == packetid )
        {
            result = mqtt_cleanup_outgoing_publish( mqtt_obj, index );
            if( result != CY_RSLT_SUCCESS )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_cleanup_outgoing_publish failed with Error : [0x%X] \n", (unsigned int)result );
                break;
            }
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nCleaned up outgoing PUBLISH packet with packet id %u.\n", packetid );
            break;
        }
    }
    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_update_suback_status( cy_mqtt_object_t *mqtt_obj, MQTTPacketInfo_t *packet_info )
{
    uint8_t        *payload = NULL, i = 0;
    size_t         num_of_subscriptions = 0;
    MQTTStatus_t   mqttStatus = MQTTSuccess;

    mqttStatus = MQTT_GetSubAckStatusCodes( packet_info, &payload, &num_of_subscriptions );
    if( (mqttStatus != MQTTSuccess) || (num_of_subscriptions != mqtt_obj->num_of_subs_in_req) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT_GetSubAckStatusCodes failed with status = %s.\n", MQTT_Status_strerror( mqttStatus ) );
        /* SubAckStatusCodes are not available for outstanding subscription messages waiting for acknowledgment. So Setting num_of_subs_in_req to 0.*/
        mqtt_obj->num_of_subs_in_req = 0;
        return CY_RSLT_MODULE_MQTT_ERROR;
    }
    ( void ) mqttStatus;

    for( i = 0; i < mqtt_obj->num_of_subs_in_req; i++ )
    {
        mqtt_obj->sub_ack_status[i] = (MQTTSubAckStatus_t)payload[ i ];
    }
    /* All outstanding subscription message acknowledgment status is updated. So Setting num_of_subs_in_req to 0. */
    mqtt_obj->num_of_subs_in_req = 0;
    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_get_next_free_index_for_publish( cy_mqtt_object_t *mqtt_obj, uint8_t *pindex )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint8_t   index  = 0;

    if( (mqtt_obj == NULL) || (pindex == NULL) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to mqtt_get_next_free_index_for_publish.\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_get_next_free_index_for_publish - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        return result;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_get_next_free_index_for_publish - Acquired Mutex %p \n", mqtt_obj->process_mutex );

    for( index = 0; index < CY_MQTT_MAX_OUTGOING_PUBLISHES; index++ )
    {
        /* A free index is marked by the invalid packet ID.
         * Check if the the index has a free slot. */
        if( mqtt_obj->outgoing_pub_packets[ index ].packetid == MQTT_PACKET_ID_INVALID )
        {
            result = CY_RSLT_SUCCESS;
            mqtt_obj->outgoing_pub_packets[ index ].packetid = MQTT_GetPacketId( &(mqtt_obj->mqtt_context) );
            *pindex = index;
            break;
        }
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_get_next_free_index_for_publish - Releasing Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );

        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_get_next_free_index_for_publish - Released Mutex %p \n", mqtt_obj->process_mutex );

    if( index >= CY_MQTT_MAX_OUTGOING_PUBLISHES )
    {
        result = CY_RSLT_MODULE_MQTT_ERROR;
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_cleanup_outgoing_publishes( cy_mqtt_object_t *mqtt_obj )
{
    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to mqtt_cleanup_outgoing_publishes.\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    /* Clean up all outgoing PUBLISH packets. */
    ( void ) memset( mqtt_obj->outgoing_pub_packets, 0x00, sizeof( mqtt_obj->outgoing_pub_packets ) );
    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_handle_publish_resend( cy_mqtt_object_t *mqtt_obj )
{
    cy_rslt_t         result = CY_RSLT_SUCCESS;
    MQTTStatus_t      mqttStatus = MQTTSuccess;
    uint8_t           index = 0U;
    MQTTStateCursor_t cursor = MQTT_STATE_CURSOR_INITIALIZER;
    uint16_t          packetid_to_resend = MQTT_PACKET_ID_INVALID;
    bool              found_packetid = false;

    /* MQTT_PublishToResend() provides a packet ID of the next PUBLISH packet
     * that should be resent. In accordance with the MQTT v3.1.1 spec,
     * MQTT_PublishToResend() preserves the ordering of when the original
     * PUBLISH packets were sent. The outgoing_pub_packets array is searched
     * through for the associated packet ID. */
    packetid_to_resend = MQTT_PublishToResend( &(mqtt_obj->mqtt_context), &cursor );
    while( packetid_to_resend != MQTT_PACKET_ID_INVALID )
    {
        found_packetid = false;

        for( index = 0U; index < CY_MQTT_MAX_OUTGOING_PUBLISHES; index++ )
        {
            if( mqtt_obj->outgoing_pub_packets[ index ].packetid == packetid_to_resend )
            {
                found_packetid = true;
                if( mqtt_obj->outgoing_pub_packets[ index ].pubinfo.qos != MQTTQoS0 )
                {
                    mqtt_obj->outgoing_pub_packets[ index ].pubinfo.dup = true;

                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nSending duplicate PUBLISH with packet id %u.\n",
                                     mqtt_obj->outgoing_pub_packets[ index ].packetid );
                    mqttStatus = MQTT_Publish( &(mqtt_obj->mqtt_context), &(mqtt_obj->outgoing_pub_packets[ index ].pubinfo),
                                               mqtt_obj->outgoing_pub_packets[ index ].packetid );
                    if( mqttStatus != MQTTSuccess )
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nSending duplicate PUBLISH for packet id %u failed with status %s.\n",
                                         mqtt_obj->outgoing_pub_packets[ index ].packetid, MQTT_Status_strerror( mqttStatus ) );
                        result = CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
                        break;
                    }
                    else
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nSent duplicate PUBLISH successfully for packet id %u.\n\n",
                                         mqtt_obj->outgoing_pub_packets[ index ].packetid );
                    }
                }
                else
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nResending PUBLISH packet id %u. is not required as its having QoS0\n\n",
                                     mqtt_obj->outgoing_pub_packets[ index ].packetid );
                }

            }
        }

        if( found_packetid == false )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nPacket id %u requires resend, but was not found in outgoing_pub_packets.\n",
                             packetid_to_resend );
            result = CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
            break;
        }
        else
        {
            /* Get the next packetID to be resent. */
            packetid_to_resend = MQTT_PublishToResend( &(mqtt_obj->mqtt_context), &cursor );
        }
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

static void mqtt_awsport_network_disconnect_callback( void *arg )
{
    cy_rslt_t                result = CY_RSLT_SUCCESS;
    cy_mqtt_callback_event_t event;
    cy_mqtt_object_t         *mqtt_obj = NULL;

    if( arg == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid argument for mqtt_awsport_network_disconnect_callback, so nothing to do..!\n" );
        return;
    }

    mqtt_obj = ( cy_mqtt_object_t * )arg;

    event.socket_event = CY_MQTT_SOCKET_EVENT_DISCONNECT;
    event.mqtt_obj = mqtt_obj;

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\n Network disconnection notification from socket layer.\n" );

    result = cy_rtos_put_queue( &mqtt_event_queue, (void *)&event, CY_MQTT_EVENT_QUEUE_TIMEOUT_IN_MSEC, false );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nPushing disconnect event to the mqtt_event_queue failed with Error : [0x%X] \n", (unsigned int)result );
    }

    return;
}

/*----------------------------------------------------------------------------------------------------------*/

static void call_registered_event_callbacks(cy_mqtt_t handle, cy_mqtt_event_t event)
{
    int i = 0;
    cy_mqtt_object_t *mqtt_obj = ( cy_mqtt_object_t * )handle;
    cy_mqtt_callback_t event_cb;

    for ( i = 0; i < CY_MQTT_MAX_EVENT_CALLBACKS; i++ )
    {
        if( mqtt_obj->mqtt_event_cb[i] != NULL )
        {
            event_cb = mqtt_obj->mqtt_event_cb[i];
            event_cb( handle, event, mqtt_obj->user_data[i] );
        }
    }
}

static void mqtt_event_callback( MQTTContext_t *param_mqtt_context,
                                 MQTTPacketInfo_t *param_packet_info,
                                 MQTTDeserializedInfo_t *param_deserialized_info )
{
    cy_rslt_t         result = CY_RSLT_SUCCESS;
    uint16_t          packet_id;
    cy_mqtt_object_t  *mqtt_obj = NULL;
    cy_mqtt_t         handle = NULL;
    uint8_t           index = 0;
    cy_mqtt_event_t   event;

    if( (param_mqtt_context == NULL) || (param_packet_info == NULL) || (param_deserialized_info == NULL) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to mqtt_event_callback.\n" );
        return;
    }

    memset( &event, 0x00, sizeof(cy_mqtt_event_t) );

    for( index = 0;index < CY_MQTT_MAX_HANDLE;index++ )
    {
        if( mqtt_handle_database[index].mqtt_context ==  param_mqtt_context )
        {
            handle = mqtt_handle_database[index].mqtt_handle;
            break;
        }
    }

    if( handle == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Invalid MQTT Context..\n" );
        return;
    }

    mqtt_obj = ( cy_mqtt_object_t * )handle;
    packet_id = param_deserialized_info->packetIdentifier;

    /* Handle incoming PUBLISH packets. The lower 4 bits of the PUBLISH packet
     * type is used for the dup, QoS, and retain flags. Therefore, masking
     * out the lower bits to check whether the packet is a PUBLISH packet. */
    if( ( param_packet_info->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
    {
        if( param_deserialized_info->pPublishInfo != NULL )
        {
            /* Handle incoming PUBLISH packets. */
            event.type = CY_MQTT_EVENT_TYPE_PUBLISH_RECEIVE;
            event.data.pub_msg.packet_id = packet_id;
            event.data.pub_msg.received_message.dup =  param_deserialized_info->pPublishInfo->dup;
            event.data.pub_msg.received_message.payload = (const char *) (param_deserialized_info->pPublishInfo->pPayload);
            event.data.pub_msg.received_message.payload_len = param_deserialized_info->pPublishInfo->payloadLength;

            if( param_deserialized_info->pPublishInfo->qos == MQTTQoS0 )
            {
                event.data.pub_msg.received_message.qos = CY_MQTT_QOS0;
            }
            else if( param_deserialized_info->pPublishInfo->qos == MQTTQoS1 )
            {
                event.data.pub_msg.received_message.qos = CY_MQTT_QOS1;
            }
            else
            {
                event.data.pub_msg.received_message.qos = CY_MQTT_QOS2;
            }
            event.data.pub_msg.received_message.retain = param_deserialized_info->pPublishInfo->retain;
            event.data.pub_msg.received_message.topic = param_deserialized_info->pPublishInfo->pTopicName;
            event.data.pub_msg.received_message.topic_len = param_deserialized_info->pPublishInfo->topicNameLength;

            call_registered_event_callbacks(handle, event);
        }
        else
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Invalid pPublishInfo..\n" );
            return;
        }
    }
    else
    {
        /* Handle other packets. */
        switch( param_packet_info->type )
        {
            case MQTT_PACKET_TYPE_SUBACK:

                /* Make sure that the ACK packet identifier matches with the Request packet identifier. */
                if( mqtt_obj->sent_packet_id != packet_id )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nSUBACK packet identifier does not matches with Request packet identifier.\n" );
                }
                else
                {
                    /* A SUBACK from the broker, containing the server response to our subscription request, has been received.
                     * It contains the status code indicating server approval/rejection for the subscription to the single topic
                     * requested. The SUBACK will be parsed to obtain the status code; this status code will be stored in the MQTT object
                     * member 'sub_ack_status'. */
                    result = mqtt_update_suback_status( mqtt_obj, param_packet_info );
                    if( result != CY_RSLT_SUCCESS )
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n mqtt_update_suback_status failed..!\n" );
                    }
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nSUBACK packet identifier matches with Request packet identifier.\n" );
                }
                break;

            case MQTT_PACKET_TYPE_UNSUBACK:
                /* Make sure that the UNSUBACK packet identifier matches with the Request packet identifier. */
                if( mqtt_obj->sent_packet_id != packet_id )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nUNSUBACK packet identifier does not matches with Request packet identifier.\n" );
                    mqtt_obj->unsub_ack_received = false;
                }
                else
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nUNSUBACK packet identifier matches with Request packet identifier.\n" );
                    mqtt_obj->unsub_ack_received = true;
                }
                break;

            case MQTT_PACKET_TYPE_PINGRESP:
                if( param_deserialized_info->deserializationResult != MQTTSuccess )
                {
                    memset( &event, 0x00, sizeof(cy_mqtt_event_t) );
                    event.type = CY_MQTT_EVENT_TYPE_DISCONNECT;
                    event.data.reason = CY_MQTT_DISCONN_TYPE_BROKER_DOWN;

                    call_registered_event_callbacks(handle, event);

                    mqtt_obj->mqtt_session_established = false;
                }
                else
                {
                    /* stop response timer */
                    stop_mqtt_ping_resp_timer(mqtt_obj);
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPing response received.\n" );
                break;

            case MQTT_PACKET_TYPE_PUBREC:
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPUBREC received for packet id %u.\n\n", packet_id );
                if( param_deserialized_info->deserializationResult != MQTTSuccess )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nPUBREC received with status %s.\n", MQTT_Status_strerror( param_deserialized_info->deserializationResult ) );
                }
                else
                {
                    if( packet_id == mqtt_obj->pub_ack_status.packetid )
                    {
                        mqtt_obj->pub_ack_status.puback_status = true;
                    }
                    else
                    {
                        mqtt_obj->pub_ack_status.puback_status = false;
                    }
                }
                /* Clean up the PUBLISH packet when a PUBREC is received. */
                (void)mqtt_cleanup_outgoing_publish_with_packet_id( mqtt_obj, packet_id );
                break;

            case MQTT_PACKET_TYPE_PUBREL:
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPUBREL received for packet id %u.\n", packet_id );
                break;

            case MQTT_PACKET_TYPE_PUBCOMP:
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPUBCOMP received for packet id %u.\n\n", packet_id );
                break;

            case MQTT_PACKET_TYPE_PUBACK:
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPUBACK received for packet id %u.\n\n", packet_id );
                if( param_deserialized_info->deserializationResult != MQTTSuccess )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nPUBACK received with status %s.\n", MQTT_Status_strerror( param_deserialized_info->deserializationResult ) );
                }
                else
                {
                    if( packet_id == mqtt_obj->pub_ack_status.packetid )
                    {
                        mqtt_obj->pub_ack_status.puback_status = true;
                    }
                    else
                    {
                        mqtt_obj->pub_ack_status.puback_status = false;
                    }
                }
                /* Clean up the PUBLISH packet when a PUBACK is received. */
                (void)mqtt_cleanup_outgoing_publish_with_packet_id( mqtt_obj, packet_id );
                break;

            case MQTT_PACKET_TYPE_DISCONNECT:
                /* Because this is user-initiated disconnection, no need to notify the application.*/
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nDisconnect packet received:(%02x).\n\n", param_packet_info->type );
                break;

            /* Any other packet type is invalid. */
            default:
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nUnknown packet type received:(%02x).\n\n", param_packet_info->type );
        }
    }
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_establish_session( cy_mqtt_object_t *mqtt_obj,
                                         MQTTConnectInfo_t *connect_info,
                                         MQTTPublishInfo_t *will_msg,
                                         bool create_clean_session,
                                         bool *session_present )
{
    cy_rslt_t         result = CY_RSLT_SUCCESS;
    MQTTStatus_t      mqttStatus = MQTTSuccess;

    /* Establish an MQTT session by sending a CONNECT packet. */

    /* Send an MQTT CONNECT packet to the broker. */
    mqttStatus = MQTT_Connect( &(mqtt_obj->mqtt_context), connect_info, will_msg, CY_MQTT_CONNACK_RECV_TIMEOUT_MS, session_present );
    if( mqttStatus != MQTTSuccess )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nConnection with MQTT broker failed with status %s.\n", MQTT_Status_strerror( mqttStatus ) );
        return CY_RSLT_MODULE_MQTT_CONNECT_FAIL;
    }
    else
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nMQTT connection successfully established with broker.\n\n" );
        mqtt_obj->mqtt_session_established = true;
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/
int32_t mqtt_awsport_network_receive( NetworkContext_t *network_context, void *buffer, size_t bytes_recv )
{
    int32_t bytes_received = 0, total_received = 0;
    size_t entryTimeMs = 0U, exitTimeMs = 0, remainingTimeMs = 0, elapsedTimeMs = 0U;;
    size_t bytestoread = 0;

    remainingTimeMs = CY_MQTT_MESSAGE_RECEIVE_TIMEOUT_MS;

    do
    {
        bytestoread = (size_t)bytes_recv - total_received;
        entryTimeMs = Clock_GetTimeMs();
        bytes_received = cy_awsport_network_receive( network_context, (void *)((char *)buffer + total_received), bytestoread );
        exitTimeMs = Clock_GetTimeMs();
        elapsedTimeMs = exitTimeMs - entryTimeMs;
        if( bytes_received < 0 )
        {
            return bytes_received;
        }
        else if( bytes_received == 0 )
        {
            if( total_received == 0 )
            {
                /* No data in the socket, so return. */
                break;
            }
        }
        else
        {
            total_received = total_received + bytes_received;
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n Total Bytes Received = %u\n", (unsigned int)total_received );
            /* Reset the wait time as some data is received. */
            elapsedTimeMs = 0;
            remainingTimeMs = CY_MQTT_MESSAGE_RECEIVE_TIMEOUT_MS;
        }
        remainingTimeMs = remainingTimeMs - elapsedTimeMs;
    } while( (total_received < bytes_recv) && (remainingTimeMs > 0) );

    return total_received;
}

/*----------------------------------------------------------------------------------------------------------*/

static cy_rslt_t mqtt_initialize_core_lib( MQTTContext_t *param_mqtt_context,
                                           NetworkContext_t *param_network_context,
                                           uint8_t *networkbuff, uint32_t buff_len )
{
    cy_rslt_t            result = CY_RSLT_SUCCESS;
    MQTTStatus_t         mqttStatus = MQTTSuccess;
    MQTTFixedBuffer_t    networkBuffer;
    TransportInterface_t transport;

    if( (param_mqtt_context == NULL) || (param_network_context == NULL) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Bad arguments to mqtt_initialize_core_lib.\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    memset( &networkBuffer, 0x00, sizeof(MQTTFixedBuffer_t) );
    memset( &transport, 0x00, sizeof(TransportInterface_t) );

    /* Fill in TransportInterface send and receive function pointers. */
    transport.pNetworkContext = param_network_context;
    transport.send = (TransportSend_t)&cy_awsport_network_send;
    transport.recv = (TransportRecv_t)&mqtt_awsport_network_receive;

    /* Fill the values for the network buffer. */
    networkBuffer.pBuffer = networkbuff;
    networkBuffer.size = buff_len;

    /* Initialize the MQTT library. */
    mqttStatus = MQTT_Init( param_mqtt_context, &transport, Clock_GetTimeMs,
                            mqtt_event_callback, &networkBuffer );
    if( mqttStatus != MQTTSuccess )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT init failed with Status = %s.\n", MQTT_Status_strerror( mqttStatus ) );
        result = CY_RSLT_MODULE_MQTT_INIT_FAIL;
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

static void mqtt_event_processing_thread( cy_thread_arg_t arg )
{
    cy_rslt_t                  result = CY_RSLT_SUCCESS;
    cy_mqtt_object_t           *mqtt_obj = NULL;
    cy_mqtt_event_t            event;
    cy_mqtt_callback_event_t   socket_event;
    MQTTStatus_t               mqtt_status = MQTTSuccess;
    bool                       connect_status = true;
    bool                       mqtt_ping_resp_wait;
   (void)arg;
    int                        index = 0;

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nStarting mqtt_event_processing_thread...\n" );

    while( true )
    {
        memset( &socket_event, 0x00, sizeof( cy_mqtt_callback_event_t ) );
        result = cy_rtos_get_queue( &mqtt_event_queue, (void *)&socket_event, CY_RTOS_NEVER_TIMEOUT, false );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_queue failed with Error :[0x%X]\n", (unsigned int)result );
            continue;
        }

        if( socket_event.socket_event == CY_MQTT_SOCKET_EVENT_EXIT_THREAD )
        {
            break;
        }

        memset( &event, 0x00, sizeof( cy_mqtt_event_t ) );
        if( socket_event.mqtt_obj == NULL )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid mqtt handle...!\n" );
            continue;
        }

        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquiring Mutex %p \n", mqtt_db_mutex );
        result = cy_rtos_get_mutex( &mqtt_db_mutex, CY_RTOS_NEVER_TIMEOUT );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
            continue;
        }

        /* Check if the mqtt_obj is still present in mqtt_handle_database  */
        for( index = 0;index < CY_MQTT_MAX_HANDLE;index++ )
        {
            if( mqtt_handle_database[index].mqtt_handle == (void *)socket_event.mqtt_obj )
            {
                /* The mqtt_obj is present in mqtt_handle_database */
                break;
            }
        }
        if( index == CY_MQTT_MAX_HANDLE )
        {
            /* The mqtt_obj is no longer available. Hence do not process the events related to this object */
            result = cy_rtos_set_mutex( &mqtt_db_mutex );
            if( result != CY_RSLT_SUCCESS )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
            }
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_db_mutex );
            continue;
        }

        switch (socket_event.socket_event)
        {
            case CY_MQTT_SOCKET_EVENT_DATA_RECEIVE:
            {
                mqtt_obj = (cy_mqtt_object_t *)socket_event.mqtt_obj;
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
                result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
                    result = cy_rtos_set_mutex( &mqtt_db_mutex );
                    if( result != CY_RSLT_SUCCESS )
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
                    }
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_db_mutex );
                    continue;
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquired Mutex %p \n", mqtt_obj->process_mutex );
                /* Stop MQTT Ping Timer */
                result = stop_timer( mqtt_obj );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstop_timer failed\n" );
                }

                connect_status = mqtt_obj->mqtt_session_established;
                if( connect_status )
                {
                    mqtt_ping_resp_wait = mqtt_obj->mqtt_context.waitingForPingResp;
                    mqtt_status = MQTT_ProcessLoop( &(mqtt_obj->mqtt_context), CY_MQTT_RECEIVE_DATA_TIMEOUT_MS );
                    if( mqtt_status != MQTTSuccess )
                    {
                        if( (mqtt_status == MQTTRecvFailed)  || (mqtt_status == MQTTSendFailed) ||
                            (mqtt_status == MQTTBadResponse) || (mqtt_status == MQTTKeepAliveTimeout) ||
                            (mqtt_status == MQTTIllegalState ) )
                        {
                            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nmqtt_event_processing_thread MQTT_ProcessLoop failed with status %s \n", MQTT_Status_strerror(mqtt_status) );
                            memset( &event, 0x00, sizeof(cy_mqtt_event_t) );
                            event.type = CY_MQTT_EVENT_TYPE_DISCONNECT;

                            if( mqtt_status == MQTTKeepAliveTimeout )
                            {
                                event.data.reason = CY_MQTT_DISCONN_TYPE_BROKER_DOWN;
                            }
                            else if( (mqtt_status == MQTTRecvFailed) || (mqtt_status == MQTTSendFailed) )
                            {
                                event.data.reason = CY_MQTT_DISCONN_TYPE_SND_RCV_FAIL;
                            }
                            else
                            {
                                event.data.reason = CY_MQTT_DISCONN_TYPE_BAD_RESPONSE;
                            }

                            /* Call registered application callbacks */
                            call_registered_event_callbacks((cy_mqtt_t)mqtt_obj, event);

                            mqtt_obj->mqtt_session_established = false;
                        }
                    }
                    else
                    {
                        if( ( mqtt_ping_resp_wait == true ) && ( mqtt_obj->mqtt_context.waitingForPingResp == false ) )
                        {
                            /* we have received the mqtt response. Stop timer */
                            stop_mqtt_ping_resp_timer( mqtt_obj );
                        }
                    }
                }

                /* Start MQTT Ping Timer */
                result = start_timer( mqtt_obj );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstart_timer failed\n" );
                }

                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Releasing Mutex %p \n", mqtt_obj->process_mutex );
                result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_obj->process_mutex );

                break;
            }

            case CY_MQTT_SOCKET_EVENT_DISCONNECT:
            {
                mqtt_obj = (cy_mqtt_object_t *)socket_event.mqtt_obj;
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
                result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
                    result = cy_rtos_set_mutex( &mqtt_db_mutex );
                    if( result != CY_RSLT_SUCCESS )
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
                    }
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_db_mutex );
                    continue;
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquired Mutex %p \n", mqtt_obj->process_mutex );

                /* Stop MQTT Ping Timer */
                result = stop_timer( mqtt_obj );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstop_timer failed\n" );
                }
                result = stop_mqtt_ping_resp_timer( mqtt_obj );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstop mqtt ping resp timer failed\n" );
                }

                /* Reset keepAliveSeconds to 0 to avoid timer activity after disconnection */
                mqtt_obj->keepAliveSeconds = 0;
                event.type = CY_MQTT_EVENT_TYPE_DISCONNECT;

                if( mqtt_obj->mqtt_session_established == true )
                {
                    event.data.reason = CY_MQTT_DISCONN_TYPE_NETWORK_DOWN;

                    call_registered_event_callbacks((cy_mqtt_t)mqtt_obj, event);

                    mqtt_obj->mqtt_session_established = false;
                }

                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Releasing Mutex %p \n", mqtt_obj->process_mutex );
                result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_obj->process_mutex );

                break;
            }

            case CY_MQTT_SOCKET_EVENT_PING_REQ:
            {
                mqtt_obj = (cy_mqtt_object_t *)socket_event.mqtt_obj;
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
                result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
                    result = cy_rtos_set_mutex( &mqtt_db_mutex );
                    if( result != CY_RSLT_SUCCESS )
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
                    }
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_db_mutex );
                    continue;
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Acquired Mutex %p \n", mqtt_obj->process_mutex );
                /* Stop MQTT Ping Timer */
                result = stop_timer( mqtt_obj );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstop_timer failed\n" );
                }

                connect_status = mqtt_obj->mqtt_session_established;
                if( connect_status )
                {
                    mqtt_status = MQTT_Ping( &(mqtt_obj->mqtt_context) );
                    if( mqtt_status != MQTTSuccess )
                    {
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT_Ping failed with Error : [0x%X] ", (unsigned int)mqtt_status );
                        if( mqtt_obj->mqtt_session_established == true )
                        {
                            event.data.reason = CY_MQTT_DISCONN_TYPE_NETWORK_DOWN;

                            call_registered_event_callbacks((cy_mqtt_t)mqtt_obj, event);

                            mqtt_obj->mqtt_session_established = false;
                        }
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Releasing Mutex %p ", mqtt_obj->process_mutex );
                        result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
                        if( result != CY_RSLT_SUCCESS )
                        {
                            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] ", (unsigned int)result );
                        }
                        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p ", mqtt_obj->process_mutex );

                        break;
                    }
                    else
                    {
                        /* Start ping response timer */
                        start_mqtt_ping_resp_timer(mqtt_obj);
                    }
                }
                /* Start MQTT Ping Timer */
                result = start_timer( mqtt_obj );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstart_timer failed\n" );
                }

                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Releasing Mutex %p \n", mqtt_obj->process_mutex );
                result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
                if( result != CY_RSLT_SUCCESS )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
                }
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_obj->process_mutex );

                break;
            }
            default:
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid event type \n" );
                break;
            }
        }
        result = cy_rtos_set_mutex( &mqtt_db_mutex );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        }
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_event_processing_thread - Released Mutex %p \n", mqtt_db_mutex );
    }
    result = cy_rtos_exit_thread();
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_exit_thread failed with Error :[0x%X]\n", (unsigned int)result );
    }
}

/*----------------------------------------------------------------------------------------------------------*/
static void mqtt_awsport_network_receive_callback( void *arg )
{
    cy_rslt_t                res = CY_RSLT_SUCCESS;
    cy_mqtt_callback_event_t event;
    cy_mqtt_object_t         *mqtt_obj = NULL;

    if( arg == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nInvalid handle to mqtt_awsport_network_receive_callback, so nothing to do..!\n" );
        return;
    }

    mqtt_obj = ( cy_mqtt_object_t * )arg;

    event.socket_event = CY_MQTT_SOCKET_EVENT_DATA_RECEIVE;
    event.mqtt_obj = mqtt_obj;

    res = cy_rtos_put_queue( &mqtt_event_queue, (void *)&event, CY_MQTT_EVENT_QUEUE_TIMEOUT_IN_MSEC, false );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nPushing to MQTT event to mqtt_event_queue failed with Error : [0x%X] \n", (unsigned int)res );
    }
    return;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_init( void )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( mqtt_lib_init_status == true )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nMQTT library is already initialized. Number of MQTT client instance : [%d] \n", mqtt_handle_count );
        return result;
    }

    result = cy_rtos_init_mutex2( &mqtt_db_mutex, false );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nCreating new mutex %p. failed\n", mqtt_db_mutex );
        return result;
    }
    mqtt_db_mutex_init_status = true;

    result = cy_awsport_network_init();
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_init failed with Error : [0x%X] \n", (unsigned int)result );
        (void)cy_rtos_deinit_mutex( &mqtt_db_mutex );
        mqtt_db_mutex_init_status = false;
        return result;
    }

    /*
     * Initialize the queue for mqtt events.
     */
    result = cy_rtos_init_queue( &mqtt_event_queue, CY_MQTT_EVENT_QUEUE_SIZE, sizeof(cy_mqtt_callback_event_t) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_init_queue failed with Error : [0x%X] \n", (unsigned int)result );
        (void)cy_rtos_deinit_mutex( &mqtt_db_mutex );
        (void)cy_awsport_network_deinit();
        mqtt_db_mutex_init_status = false;
        return result;
    }

    result = cy_rtos_create_thread( &mqtt_event_process_thread, mqtt_event_processing_thread, "MQTTEventProcessingThread", NULL,
                                    CY_MQTT_EVENT_THREAD_STACK_SIZE, CY_MQTT_EVENT_THREAD_PRIORITY, 0 );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_create_thread failed with Error : [0x%X] \n", (unsigned int)result );
        (void)cy_rtos_deinit_mutex( &mqtt_db_mutex );
        (void)cy_awsport_network_deinit();
        (void)cy_rtos_deinit_queue( &mqtt_event_queue );
        mqtt_db_mutex_init_status = false;
        return result;
    }

    mqtt_lib_init_status = true;
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_awsport_network_init successful.\n" );

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_create( uint8_t *buffer, uint32_t bufflen,
                          cy_awsport_ssl_credentials_t *security,
                          cy_mqtt_broker_info_t *broker_info,
                          char *descriptor,
                          cy_mqtt_t *mqtt_handle )
{
    cy_rslt_t         result = CY_RSLT_SUCCESS;
    cy_mqtt_object_t  *mqtt_obj = NULL;
    uint8_t           slot_index;
    bool              slot_found;
    bool              process_mutex_init_status = false;
    cy_mqtt_t         handle;

    if( (broker_info == NULL) || (mqtt_handle == NULL) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_create()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( buffer == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid network buffer..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( bufflen < CY_MQTT_MIN_NETWORK_BUFFER_SIZE )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBuffer length is less then minimun network buffer size : %u..!\n", (uint16_t)CY_MQTT_MIN_NETWORK_BUFFER_SIZE );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( descriptor == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid descriptor..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( strlen(descriptor) > CY_MQTT_DESCP_MAX_LEN )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Descriptor length is greater than maximum permissible length: %u!\n", (uint16_t)CY_MQTT_DESCP_MAX_LEN );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    result = cy_rtos_get_mutex( &mqtt_db_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_create - Acquired Mutex %p \n", mqtt_db_mutex );

    if( mqtt_handle_count >= CY_MQTT_MAX_HANDLE )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nNumber of created mqtt object exceeds %d..!\n", CY_MQTT_MAX_HANDLE );
        (void)cy_rtos_set_mutex( &mqtt_db_mutex );
        return CY_RSLT_MODULE_MQTT_CREATE_FAIL;
    }

    result = cy_rtos_set_mutex( &mqtt_db_mutex );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_create - Released Mutex %p \n", mqtt_db_mutex );

    result = cy_mqtt_get_handle( &handle, descriptor );
    if( result == CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nDescriptor is not unique. \n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    mqtt_obj = (cy_mqtt_object_t *)malloc( sizeof( cy_mqtt_object_t ) );
    if( mqtt_obj == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMemory not available to create MQTT object..!\n" );
        return CY_RSLT_MODULE_MQTT_NOMEM;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_obj : %p..!\n", mqtt_obj );

    /* Clear the MQTT handle data. */
    memset( mqtt_obj, 0x00, sizeof( cy_mqtt_object_t ) );

    if( security != NULL )
    {
        mqtt_obj->security.alpnprotos = security->alpnprotos;
        mqtt_obj->security.alpnprotoslen = security->alpnprotoslen;
        mqtt_obj->security.sni_host_name = security->sni_host_name;
        mqtt_obj->security.sni_host_name_size = security->sni_host_name_size;
        mqtt_obj->security.username = security->username;
        mqtt_obj->security.username_size = security->username_size;
        mqtt_obj->security.password = security->password;
        mqtt_obj->security.password_size = security->password_size;

        mqtt_obj->security.client_cert = security->client_cert;
        mqtt_obj->security.client_cert_size = security->client_cert_size;
        mqtt_obj->security.private_key = security->private_key;
        mqtt_obj->security.private_key_size = security->private_key_size;
        mqtt_obj->security.root_ca = security->root_ca;
        mqtt_obj->security.root_ca_size = security->root_ca_size;
        mqtt_obj->security.root_ca_verify_mode = security->root_ca_verify_mode;
        mqtt_obj->security.root_ca_location = security->root_ca_location;
        mqtt_obj->security.cert_key_location = security->cert_key_location;
        mqtt_obj->mqtt_secure_mode = true;
    }
    else
    {
        mqtt_obj->mqtt_secure_mode = false;
    }

    mqtt_obj->server_info.host_name = broker_info->hostname;
    mqtt_obj->server_info.port = broker_info->port;

    result = cy_rtos_init_mutex2( &(mqtt_obj->process_mutex), false );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nCreating new mutex %p. failed\n", mqtt_obj->process_mutex );
        goto exit;
    }

    process_mutex_init_status = true;

    result = mqtt_initialize_core_lib( &(mqtt_obj->mqtt_context), &(mqtt_obj->network_context), buffer, bufflen );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nmqtt_initialize_core_lib failed with Error : [0x%X] \n", (unsigned int)result );
        goto exit;
    }
    else
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nmqtt_initialize_core_lib successful.\n" );
    }

    mqtt_obj->network_context.disconnect_info.cbf = mqtt_awsport_network_disconnect_callback;
    mqtt_obj->network_context.disconnect_info.user_data = ( void * )mqtt_obj;

    mqtt_obj->network_context.receive_info.cbf = mqtt_awsport_network_receive_callback;
    mqtt_obj->network_context.receive_info.user_data = ( void * )mqtt_obj;

    result = cy_rtos_get_mutex( &mqtt_db_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        goto exit;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_create - Acquired Mutex %p \n", mqtt_db_mutex );

    slot_index = 0;
    slot_found = false;

    while( slot_index < CY_MQTT_MAX_HANDLE )
    {
        if( mqtt_handle_database[slot_index].mqtt_handle == NULL )
        {
            mqtt_handle_database[slot_index].mqtt_handle = (void *)mqtt_obj;
            mqtt_handle_database[slot_index].mqtt_context = &(mqtt_obj->mqtt_context);
            slot_found = true;
            break;
        }
        slot_index++;
    }

    if( slot_found == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Free slot not available for new handle..!\n" );
        (void)cy_rtos_set_mutex( &mqtt_db_mutex );
        result = CY_RSLT_MODULE_MQTT_CREATE_FAIL;
        goto exit;
    }

    /* Initialize timer to handle MQTT ping timeout events */
    result = cy_rtos_init_timer( &mqtt_obj->mqtt_timer, CY_TIMER_TYPE_ONCE, ( cy_timer_callback_t )mqtt_ping_request_callback, ( cy_timer_callback_arg_t )mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_init_timer failed with Error : [0x%X] \n", (unsigned int)result );
        result = cy_rtos_set_mutex( &mqtt_db_mutex );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        }
        goto exit;
    }
    result = cy_rtos_init_timer( &mqtt_obj->mqtt_ping_resp_timer, CY_TIMER_TYPE_ONCE, ( cy_timer_callback_t )mqtt_pingresp_timeout_callback, ( cy_timer_callback_arg_t )mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR,
                "\nPing Resp Timer: cy_rtos_init_timer failed with Error : [0x%X] \n", (unsigned int)result );
        result = cy_rtos_set_mutex( &mqtt_db_mutex );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR,
                    "\nPing Resp Timer: cy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        }
        goto exit;
    }
    memcpy(mqtt_obj->mqtt_descriptor, descriptor, strlen(descriptor)+1);

    mqtt_obj->mqtt_magic_header = CY_MQTT_MAGIC_HEADER;
    mqtt_obj->mqtt_magic_footer = CY_MQTT_MAGIC_FOOTER;
    mqtt_obj->mqtt_obj_index = slot_index;
    *mqtt_handle = (void *)mqtt_obj;
    mqtt_handle_count++;

    result = cy_rtos_set_mutex( &mqtt_db_mutex );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        goto exit;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_create - Released Mutex %p \n", mqtt_db_mutex );

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nMQTT object created successfully..\n" );

    return CY_RSLT_SUCCESS;

exit :
    if( mqtt_obj != NULL )
    {
        if( process_mutex_init_status == true )
        {
            (void)cy_rtos_deinit_mutex( &(mqtt_obj->process_mutex) );
            process_mutex_init_status = false;
        }
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n Free mqtt_obj : %p..!\n", mqtt_obj );
        free( mqtt_obj );
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_connect( cy_mqtt_t mqtt_handle, cy_mqtt_connect_info_t *connect_info )
{
    cy_rslt_t                     result = CY_RSLT_SUCCESS;
    cy_rslt_t                     res = CY_RSLT_SUCCESS;
    RetryUtilsParams_t            reconnectParams;
    MQTTStatus_t                  mqttStatus = MQTTSuccess;
    RetryUtilsStatus_t            retryUtilsStatus = RetryUtilsSuccess;
    cy_mqtt_object_t              *mqtt_obj;
    bool                          create_clean_session = false;
    MQTTConnectInfo_t             connect_details;
    MQTTPublishInfo_t             will_msg_details;
    MQTTPublishInfo_t             *will_msg_ptr = NULL;
    cy_awsport_ssl_credentials_t  *security = NULL;

    if( mqtt_handle == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_connect()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_connect - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        return result;
    }

    memset( &connect_details, 0x00, sizeof( MQTTConnectInfo_t ) );
    memset( &will_msg_details, 0x00, sizeof( MQTTPublishInfo_t ) );

    /* Connect Information */
    connect_details.cleanSession = connect_info->clean_session;
    connect_details.keepAliveSeconds = connect_info->keep_alive_sec;
    connect_details.pClientIdentifier = connect_info->client_id;
    connect_details.clientIdentifierLength = connect_info->client_id_len;
    connect_details.pPassword = connect_info->password;
    connect_details.passwordLength = connect_info->password_len;
    connect_details.pUserName = connect_info->username;
    connect_details.userNameLength = connect_info->username_len;

    /* Store the keepAlivetimeout */
    mqtt_obj->keepAliveSeconds = connect_info->keep_alive_sec;

    if( connect_info->will_info != NULL )
    {
        /* Will information. */
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nWill info is not NULL ..!\n" );

        if( connect_info->will_info->qos > CY_MQTT_QOS2 )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid Will msg QoS..!\n" );
            result = CY_RSLT_MODULE_MQTT_CONNECT_FAIL;
            goto exit;
        }
        if( (connect_info->will_info->dup != true) && (connect_info->will_info->dup != false) )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid Will msg dup..!\n" );
            result = CY_RSLT_MODULE_MQTT_CONNECT_FAIL;
            goto exit;
        }
        if( (connect_info->will_info->retain != true) && (connect_info->will_info->retain != false) )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid Will msg retain..!\n" );
            result = CY_RSLT_MODULE_MQTT_CONNECT_FAIL;
            goto exit;
        }

        if( connect_info->will_info->qos == CY_MQTT_QOS0 )
        {
            will_msg_details.qos = MQTTQoS0;
        }
        else if( connect_info->will_info->qos == CY_MQTT_QOS1 )
        {
            will_msg_details.qos = MQTTQoS1;
        }
        else
        {
            will_msg_details.qos = MQTTQoS2;
        }

        will_msg_details.dup = connect_info->will_info->dup;
        will_msg_details.retain = connect_info->will_info->retain;
        will_msg_details.pTopicName = connect_info->will_info->topic;
        will_msg_details.topicNameLength = connect_info->will_info->topic_len;
        will_msg_details.pPayload = connect_info->will_info->payload;
        will_msg_details.payloadLength = connect_info->will_info->payload_len;
        will_msg_ptr = &will_msg_details;
    }
    else
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nWill info is NULL ..!\n" );
        will_msg_ptr = NULL;
    }

    /* Initialize the reconnect attempts and interval. */
    RetryUtils_ParamsReset( &reconnectParams );

    if( mqtt_obj->mqtt_secure_mode == true )
    {
        security = &(mqtt_obj->security);
    }
    else
    {
        security = NULL;
    }

    /* Attempt to connect to an MQTT broker. If connection fails, retry after
     * a timeout. The timeout value will exponentially increase until the maximum
     * attempts are reached.
     */
    do
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nCreating MQTT socket..\n" );
        result = cy_awsport_network_create( &(mqtt_obj->network_context), &(mqtt_obj->server_info), security, &(mqtt_obj->network_context.disconnect_info), &(mqtt_obj->network_context.receive_info) );
        if ( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_create failed with Error : [0x%X] \n", (unsigned int)result );
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nConnection to the broker failed. Retrying connection with backoff and jitter.\n" );
            retryUtilsStatus = RetryUtils_BackoffAndSleep( &reconnectParams );
        }
        else
        {
            /* Establish a TLS session with the MQTT broker. */
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "Establishing a TLS session to %.*s:%d.\n",
                             strlen(mqtt_obj->server_info.host_name), mqtt_obj->server_info.host_name, mqtt_obj->server_info.port );
            result = cy_awsport_network_connect( &(mqtt_obj->network_context),
                                                 CY_MQTT_MESSAGE_SEND_TIMEOUT_MS,
                                                 CY_MQTT_SOCKET_RECEIVE_TIMEOUT_MS );
            if( result != CY_RSLT_SUCCESS )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nConnection to the broker failed. Retrying connection with backoff and jitter.\n" );

                retryUtilsStatus = RetryUtils_BackoffAndSleep( &reconnectParams );
                (void)cy_awsport_network_delete( &(mqtt_obj->network_context) );
                /*
                 * In case of an unexpected network disconnection, the cy_awsport_network_delete API always returns failure. Therefore,
                 * the return value of the cy_awsport_network_delete API is not checked here.
                 */
                /* Fall-through. */
            }

            if( retryUtilsStatus == RetryUtilsRetriesExhausted )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nConnection to the broker failed, all attempts exhausted.\n" );
                result = CY_RSLT_MODULE_MQTT_CONNECT_FAIL;
            }
        }

    } while( ( result != CY_RSLT_SUCCESS ) && ( retryUtilsStatus == RetryUtilsSuccess ) );

    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nTLS connection failed with Error : [0x%X] \n", (unsigned int)result );
        goto exit;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nTLS connection established ..\n" );

    create_clean_session = (connect_details.cleanSession == true ) ? true : false;
    if( create_clean_session == true )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nCreating clean session ..\n" );
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nCreating an MQTT connection to %.*s.\n",
                     strlen(mqtt_obj->server_info.host_name), mqtt_obj->server_info.host_name );

    /* Sends an MQTT Connect packet using the established TLS session. */
    result = mqtt_establish_session( mqtt_obj, &connect_details, will_msg_ptr, create_clean_session, &(mqtt_obj->broker_session_present) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nEstablish MQTT session failed with Error : [0x%X] \n", (unsigned int)result );
        goto exit;
    }
    else
    {

        if( (mqtt_obj->broker_session_present == true) && (create_clean_session == false) )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nMQTT session with broker is re-established. Resending unacked publishes.\n" );
            /* Handle all resend of PUBLISH messages. */
            result = mqtt_handle_publish_resend( mqtt_obj );
            if( result != CY_RSLT_SUCCESS )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nHandle all the resend of PUBLISH messages failed with Error : [0x%X] \n", (unsigned int)result );
                goto exit;
            }
        }
        else
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\n A clean MQTT connection is established. Cleaning up all the stored outgoing publishes.\n" );

            /* Clean up the outgoing PUBLISH packets and wait for ack because this new
             * connection does not re-establish an existing session. */
            result = mqtt_cleanup_outgoing_publishes( mqtt_obj );
            if( result != CY_RSLT_SUCCESS )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nCleaning of PUBLISH messages failed with Error : [0x%X] \n", (unsigned int)result );
                goto exit;
            }
        }
    }

    mqtt_obj->mqtt_conn_status = true;

    result = start_timer( mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstart_timer failed with Error : [0x%X] \n", (unsigned int)result );
        goto exit;
    }

    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        goto exit;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_connect - Released Mutex %p \n", mqtt_obj->process_mutex );

    return result;

exit :
    if( mqtt_obj->mqtt_session_established == true )
    {
        mqttStatus = MQTT_Disconnect( &(mqtt_obj->mqtt_context) );
        if( mqttStatus != MQTTSuccess )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Sending MQTT DISCONNECT failed with status=%s.\n",
                             MQTT_Status_strerror( mqttStatus ) );
            /*
             * In case of an unexpected network disconnection, the MQTT_Disconnect API always returns failure. Therefore,
             * the return value of the MQTT_Disconnect API is not checked here.
             */
            /* Fall-through. */
        }
        mqtt_obj->mqtt_session_established = false;
    }

    res = cy_awsport_network_disconnect( &(mqtt_obj->network_context) );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_disconnect failed with Error : [0x%X] \n", (unsigned int)result );
        /*
         * In case of an unexpected network disconnection, the cy_awsport_network_disconnect API always returns failure. Therefore,
         * the return value of the cy_awsport_network_disconnect API is not checked here.
         */
        /* Fall-through. */
    }
    res = cy_awsport_network_delete( &(mqtt_obj->network_context) );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_delete failed with Error : [0x%X] \n", (unsigned int)result );
        /*
         * In case of an unexpected network disconnection, the cy_awsport_network_delete API always returns failure. Therefore,
         * the return value of the cy_awsport_network_delete API is not checked here.
         */
        /* Fall-through. */
    }

    res = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)res );
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_connect - Released Mutex %p \n", mqtt_obj->process_mutex );

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_publish( cy_mqtt_t mqtt_handle, cy_mqtt_publish_info_t *pubmsg )
{
    cy_rslt_t        result = CY_RSLT_SUCCESS;
    cy_rslt_t        timer_result = CY_RSLT_SUCCESS;
    MQTTStatus_t     mqttStatus = MQTTSuccess;
    uint8_t          publishIndex = CY_MQTT_MAX_OUTGOING_PUBLISHES;
    cy_mqtt_object_t *mqtt_obj;
    uint8_t          retry = 0;
    uint32_t         timeout = 0;

    if( (mqtt_handle == NULL) || (pubmsg == NULL) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_publish()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    if( mqtt_obj->mqtt_session_established == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT client session not present..!\n" );
        return CY_RSLT_MODULE_MQTT_NOT_CONNECTED;
    }

    /* Get the next free index for the outgoing PUBLISH packets. All QoS2 outgoing
     * PUBLISH packets are stored until a PUBREC is received. These messages are
     * stored for supporting a resend if a network connection is broken before
     * receiving a PUBREC. */
    result = mqtt_get_next_free_index_for_publish( mqtt_obj, &publishIndex );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nUnable to find a free spot for outgoing PUBLISH message.\n" );
        return CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
    }
    else
    {
        if( (pubmsg->qos == CY_MQTT_QOS0) || (pubmsg->qos == CY_MQTT_QOS1) || (pubmsg->qos == CY_MQTT_QOS2) )
        {
            mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.qos = (MQTTQoS_t)pubmsg->qos;
        }
        else
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nQoS level not supported..!\n" );
            return CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
        }
        mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.pTopicName = pubmsg->topic;
        mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.topicNameLength = pubmsg->topic_len;
        mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.pPayload = pubmsg->payload;
        mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.payloadLength = pubmsg->payload_len;

        result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
            return result;
        }
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_publish - Acquired Mutex %p \n", mqtt_obj->process_mutex );

        /* Get a new packet ID. */
        mqtt_obj->pub_ack_status.packetid = mqtt_obj->outgoing_pub_packets[ publishIndex ].packetid;

        /* Stop MQTT Ping Timer */
        result = stop_timer( mqtt_obj );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nstop_timer failed\n" );
        }

        /* Publish retry loop. */
        do
        {
            mqtt_obj->pub_ack_status.puback_status = false;
            timeout = CY_MQTT_ACK_RECEIVE_TIMEOUT_MS;

            /* Send the PUBLISH packet. */
            mqttStatus = MQTT_Publish( &(mqtt_obj->mqtt_context),
                                       &(mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo),
                                       mqtt_obj->outgoing_pub_packets[ publishIndex ].packetid );
            if( mqttStatus != MQTTSuccess )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to send PUBLISH packet to broker with error = %s.\n",
                                 MQTT_Status_strerror( mqttStatus ) );
                result = CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
            }
            else
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nPUBLISH sent for topic %.*s to broker with packet ID %u.\n",
                                 pubmsg->topic_len, pubmsg->topic, mqtt_obj->outgoing_pub_packets[ publishIndex ].packetid );
                /* Process the incoming packet from the broker.
                 * Acknowledgment for PUBLISH ( PUBACK ) will be received here. */
                if( mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.qos != MQTTQoS0 )
                {
                    do
                    {
                        mqttStatus = MQTT_ProcessLoop( &(mqtt_obj->mqtt_context), CY_MQTT_RECEIVE_DATA_TIMEOUT_MS );
                        if( (mqttStatus != MQTTSuccess) && (mqttStatus != MQTTNoDataAvailable) )
                        {
                            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT_ProcessLoop returned with status = %s.\n", MQTT_Status_strerror( mqttStatus ) );
                            result = CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
                            break;
                        }
                        else
                        {
                            if( mqtt_obj->pub_ack_status.puback_status == true )
                            {
                                result = CY_RSLT_SUCCESS;
                                break;
                            }
                        }

                        timeout = timeout - CY_MQTT_SOCKET_RECEIVE_TIMEOUT_MS;

                    } while( timeout > 0 );

                    /* Assign the MQTT Status to an error in case of PUBACK/PUBREC receive failure to retry publish. */
                    if( mqtt_obj->pub_ack_status.puback_status == false )
                    {
                        result = CY_RSLT_MODULE_MQTT_PUBLISH_FAIL;
                        mqttStatus = MQTTRecvFailed;
                    }
                }
                else
                {
                    result = CY_RSLT_SUCCESS;
                }
                mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.dup = true;
            }
            retry++;
        } while( (mqttStatus != MQTTSuccess) && (retry < CY_MQTT_MAX_RETRY_VALUE) );

        /* Start MQTT Ping Timer */
        timer_result = start_timer( mqtt_obj );
        if( timer_result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstart_timer failed with Error : [0x%X] \n", (unsigned int)timer_result );
            mqtt_cleanup_outgoing_publish( mqtt_obj, publishIndex );
            (void)cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
            return timer_result;
        }

        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nFailed to send PUBLISH packet to broker with max retry..!\n " );
            mqtt_cleanup_outgoing_publish( mqtt_obj, publishIndex );
            (void)cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
            return result;
        }

        if( mqtt_obj->outgoing_pub_packets[ publishIndex ].pubinfo.qos == MQTTQoS0 )
        {
            /* Clean up outgoing_pub_packets for QoS0 PUBLISH packets.*/
            (void)mqtt_cleanup_outgoing_publish( mqtt_obj, publishIndex );
        }

        result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
            return result;
        }
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_publish - Released Mutex %p \n", mqtt_obj->process_mutex );
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_subscribe( cy_mqtt_t mqtt_handle, cy_mqtt_subscribe_info_t *sub_info, uint8_t sub_count  )
{
    cy_rslt_t              result = CY_RSLT_SUCCESS;
    cy_rslt_t              timer_result = CY_RSLT_SUCCESS;
    MQTTStatus_t           mqttStatus;
    cy_mqtt_object_t       *mqtt_obj;
    uint8_t                index = 0, retry = 0;
    MQTTSubscribeInfo_t    *sub_list = NULL;
    uint32_t               timeout = 0;

    if( (mqtt_handle == NULL) || (sub_info == NULL) || (sub_count < 1) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_subscribe()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    if( mqtt_obj->mqtt_session_established == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT client session not present..!\n" );
        return CY_RSLT_MODULE_MQTT_NOT_CONNECTED;
    }

    if( sub_count > CY_MQTT_MAX_OUTGOING_SUBSCRIBES )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMax number of supported subscription count in single request is %d\n", (int)CY_MQTT_MAX_OUTGOING_SUBSCRIBES );
        return CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL;
    }

    sub_list = (MQTTSubscribeInfo_t *)malloc( (sizeof(MQTTSubscribeInfo_t) * sub_count) );
    if( sub_list == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMemory not available to create sub_list..!\n" );
        return CY_RSLT_MODULE_MQTT_NOMEM;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nsub_list : %p..!\n", sub_list );

    for( index = 0;index < sub_count;index++ )
    {
        if( sub_info[index].qos == CY_MQTT_QOS0 )
        {
            sub_list[ index ].qos = MQTTQoS0;
        }
        else if( sub_info[index].qos == CY_MQTT_QOS1 )
        {
            sub_list[ index ].qos = MQTTQoS1;
        }
        else if( sub_info[index].qos == CY_MQTT_QOS2 )
        {
            sub_list[ index ].qos = MQTTQoS2;
        }
        else
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nQoS not supported..!\n" );
            free( sub_list );
            return CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL;
        }
        sub_info[ index ].allocated_qos = CY_MQTT_QOS_INVALID;
        sub_list[ index ].pTopicFilter = sub_info[index].topic;
        sub_list[ index ].topicFilterLength = sub_info[index].topic_len;
    }

    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        free( sub_list );
        return result;
    }

    /* Generate the packet identifier for the SUBSCRIBE packet. */
    mqtt_obj->sent_packet_id = MQTT_GetPacketId( &(mqtt_obj->mqtt_context) );
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_subscribe - Acquired Mutex %p \n", mqtt_obj->process_mutex );

    /* Stop MQTT Ping Timer */
    result = stop_timer( mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nstop_timer failed\n" );
    }

    do
    {
        timeout = CY_MQTT_ACK_RECEIVE_TIMEOUT_MS;
        result = CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL;
        memset( &mqtt_obj->sub_ack_status, 0x00, sizeof(mqtt_obj->sub_ack_status) );

        /*
         * num_of_subs_in_req is initialized with number of subscribe messages in one MQTT subscribe request.
         * Once after receiving the subscription acknowledgment this variable is set to zero.
         * So num_of_subs_in_req == 0 refers that there is no outstanding subscription messages waiting for acknowledgment. */
        mqtt_obj->num_of_subs_in_req = sub_count;

        /* Send the SUBSCRIBE packet. */
        mqttStatus = MQTT_Subscribe( &(mqtt_obj->mqtt_context),
                                     sub_list,
                                     sub_count,
                                     mqtt_obj->sent_packet_id );
        if( mqttStatus != MQTTSuccess )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nFailed to send SUBSCRIBE packet to broker with error = %s.\n",
                             MQTT_Status_strerror( mqttStatus ) );
            result = CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL;
        }
        else
        {
            for( index = 0; index < sub_count; index++ )
            {
                mqtt_obj->sub_ack_status[index] = MQTTSubAckFailure;
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nSUBSCRIBE sent for topic %.*s to broker.\n",
                                 sub_list[ index ].topicFilterLength,
                                 sub_list[ index ].pTopicFilter );
            }
            do
            {
                /* Process the incoming packet from the broker.
                 * Acknowledgment for subscription ( SUBACK ) will be received here. */
                mqttStatus = MQTT_ProcessLoop( &(mqtt_obj->mqtt_context), CY_MQTT_RECEIVE_DATA_TIMEOUT_MS );
                if( mqttStatus != MQTTSuccess )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT_ProcessLoop returned with status = %s.\n", MQTT_Status_strerror( mqttStatus ) );
                    result = CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL;
                    break;
                }

                /* if suback status is updated then num_of_subs_in_req will be set to 0 in mqtt_event_callback.*/
                if( mqtt_obj->num_of_subs_in_req == 0 )
                {
                    result = CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL; /* Initialize result with failure. */
                    for( index = 0; index < sub_count; index++ )
                    {
                        if( mqtt_obj->sub_ack_status[index] == MQTTSubAckFailure )
                        {
                            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nMQTT broker rejected SUBSCRIBE request for topic %.*s .\n",
                                             sub_list[ index ].topicFilterLength,
                                             sub_list[ index ].pTopicFilter );
                            sub_info[ index ].allocated_qos = CY_MQTT_QOS_INVALID;
                        }
                        else
                        {
                            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nSUBSCRIBE accepted for topic %.*s with QoS %d .\n",
                                             sub_list[ index ].topicFilterLength,
                                             sub_list[ index ].pTopicFilter, mqtt_obj->sub_ack_status[index] );
                            if( mqtt_obj->sub_ack_status[index] == MQTTSubAckSuccessQos0 )
                            {
                                sub_info[ index ].allocated_qos = CY_MQTT_QOS0;
                            }
                            else if( mqtt_obj->sub_ack_status[index] == MQTTSubAckSuccessQos1 )
                            {
                                sub_info[ index ].allocated_qos = CY_MQTT_QOS1;
                            }
                            else
                            {
                                sub_info[ index ].allocated_qos = CY_MQTT_QOS2;
                            }
                            result = CY_RSLT_SUCCESS; /* Update with success if at least one subscription is successful. */
                        }
                    }
                    break; /* Received the ack. So exit timeout do loop */
                }
                timeout = timeout - CY_MQTT_SOCKET_RECEIVE_TIMEOUT_MS;
            } while( timeout > 0 );

            if( mqtt_obj->num_of_subs_in_req != 0 )
            {
                result = CY_RSLT_MODULE_MQTT_SUBSCRIBE_FAIL;
                mqttStatus = MQTTRecvFailed; /* Assign error value to retry subscribe. */
            }
        }
        retry++;
    } while( (mqttStatus != MQTTSuccess) && (retry < CY_MQTT_MAX_RETRY_VALUE) );

    /* Start MQTT Ping Timer */
    timer_result = start_timer( mqtt_obj );
    if( timer_result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstart_timer failed with Error : [0x%X] \n", (unsigned int)timer_result );
        goto exit;
    }

    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nSubscription ack status is MQTTSubAckFailure..!\n" );
        goto exit;
    }

    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        free( sub_list );
        return result;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_subscribe - Released Mutex %p \n", mqtt_obj->process_mutex );
    free( sub_list );
    return CY_RSLT_SUCCESS;

exit :
    (void)cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    /* Free sub_list */
    if( sub_list != NULL )
    {
        free( sub_list );
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_unsubscribe( cy_mqtt_t mqtt_handle, cy_mqtt_unsubscribe_info_t *unsub_info, uint8_t unsub_count )
{
    cy_rslt_t              result = CY_RSLT_SUCCESS;
    cy_rslt_t              timer_result = CY_RSLT_SUCCESS;
    cy_mqtt_object_t       *mqtt_obj;
    MQTTStatus_t           mqttStatus;
    uint8_t                index = 0, retry = 0;
    MQTTSubscribeInfo_t    *unsub_list = NULL;
    uint32_t               timeout = 0;

    if( (mqtt_handle == NULL) || (unsub_info == NULL) || (unsub_count < 1) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_unsubscribe()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    if( mqtt_obj->mqtt_session_established == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT client session not present..!\n" );
        return CY_RSLT_MODULE_MQTT_NOT_CONNECTED;
    }

    if( unsub_count > CY_MQTT_MAX_OUTGOING_SUBSCRIBES )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMax number of supported unsubscription count in single request is %d\n", (int)CY_MQTT_MAX_OUTGOING_SUBSCRIBES );
        return CY_RSLT_MODULE_MQTT_UNSUBSCRIBE_FAIL;
    }

    unsub_list = (MQTTSubscribeInfo_t *)malloc( (sizeof(MQTTSubscribeInfo_t) * unsub_count) );
    if( unsub_list == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMemory not available to create unsub_list..!\n" );
        return CY_RSLT_MODULE_MQTT_NOMEM;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nsub_list : %p..!\n", unsub_list );

    for( index = 0; index < unsub_count; index++ )
    {
        if( unsub_info[index].qos == CY_MQTT_QOS0 )
        {
            unsub_list[ index ].qos = MQTTQoS0;
        }
        else if( unsub_info[index].qos == CY_MQTT_QOS1 )
        {
            unsub_list[ index ].qos = MQTTQoS1;
        }
        else if( unsub_info[index].qos == CY_MQTT_QOS2 )
        {
            unsub_list[ index ].qos = MQTTQoS2;
        }
        else
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nQoS level not supported...\n" );
            free( unsub_list );
            return CY_RSLT_MODULE_MQTT_UNSUBSCRIBE_FAIL;
        }
        unsub_list[ index ].pTopicFilter = unsub_info[index].topic;
        unsub_list[ index ].topicFilterLength = unsub_info[index].topic_len;
    }

    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        free( unsub_list );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_unsubscribe - Acquired Mutex %p \n", mqtt_obj->process_mutex );

    /* Generate the packet identifier for the UNSUBSCRIBE packet. */
    mqtt_obj->sent_packet_id = MQTT_GetPacketId( &(mqtt_obj->mqtt_context) );

    /* Stop MQTT Ping Timer */
    result = stop_timer( mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nstop_timer failed" );
    }

    do
    {
        timeout = CY_MQTT_ACK_RECEIVE_TIMEOUT_MS;
        mqtt_obj->unsub_ack_received = false;
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "UNSUBSCRIBE sent for topic %.*s to broker.\n\n", unsub_info->topic_len, unsub_info->topic );
        /* Send the UNSUBSCRIBE packet. */
        mqttStatus = MQTT_Unsubscribe( &(mqtt_obj->mqtt_context),
                                       unsub_list,
                                       unsub_count,
                                       mqtt_obj->sent_packet_id );
        if( mqttStatus != MQTTSuccess )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to send UNSUBSCRIBE packet to broker with error = %s.\n",
                        MQTT_Status_strerror( mqttStatus ) );
            result = CY_RSLT_MODULE_MQTT_UNSUBSCRIBE_FAIL;
        }
        else
        {
            do
            {
                /* Process the  incoming packet from the broker.
                 * Acknowledgment for UNSUBSCRIBE ( UNSUBACK ) will be received here. */
                mqttStatus = MQTT_ProcessLoop( &(mqtt_obj->mqtt_context), CY_MQTT_RECEIVE_DATA_TIMEOUT_MS );
                if( mqttStatus != MQTTSuccess )
                {
                    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT_ProcessLoop returned with status = %s.\n", MQTT_Status_strerror( mqttStatus ) );
                    result = CY_RSLT_MODULE_MQTT_UNSUBSCRIBE_FAIL;
                    break;
                }
                if( mqtt_obj->unsub_ack_received == true )
                {
                    result = CY_RSLT_SUCCESS;
                    break;
                }
                timeout = timeout - CY_MQTT_SOCKET_RECEIVE_TIMEOUT_MS;
            } while( timeout > 0 );

            if( mqtt_obj->unsub_ack_received == false )
            {
                cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nNot received unsuback before timeout %u millisecond \n", (unsigned int)CY_MQTT_ACK_RECEIVE_TIMEOUT_MS );
                result = CY_RSLT_MODULE_MQTT_UNSUBSCRIBE_FAIL;
                mqttStatus = MQTTRecvFailed; /* Assign error value to retry subscribe. */
            }
        }
        retry++;
    } while( (mqttStatus != MQTTSuccess) && (retry < CY_MQTT_MAX_RETRY_VALUE) );

    /* Start MQTT Ping Timer */
    timer_result = start_timer( mqtt_obj );
    if( timer_result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nstart_timer failed with Error : [0x%X] \n", (unsigned int)timer_result );
        goto exit;
    }

    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nSubscription ack status is MQTTSubAckFailure..!\n" );
        goto exit;
    }

    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        free( unsub_list );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_unsubscribe - Released Mutex %p \n", mqtt_obj->process_mutex );

    /* Free unsub_list. */
    free( unsub_list );
    return CY_RSLT_SUCCESS;

exit :
    (void)cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    /* Free unsub_list. */
    if( unsub_list != NULL )
    {
        free( unsub_list );
    }

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_disconnect( cy_mqtt_t mqtt_handle )
{
    cy_rslt_t         result = CY_RSLT_SUCCESS;
    cy_mqtt_object_t  *mqtt_obj;
    MQTTStatus_t      mqttStatus = MQTTSuccess;

    if( mqtt_handle == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_disconnect()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_disconnect - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_disconnect - Acquired Mutex %p \n", mqtt_obj->process_mutex );

    result = stop_timer( mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_rtos_stop_timer failed\n" );
    }

    result = stop_mqtt_ping_resp_timer( mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nfailed to stop mqtt ping response timer\n" );
    }

    if( mqtt_obj->mqtt_conn_status == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nMQTT client not connected..!\n" );
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_disconnect - Releasing Mutex %p \n", mqtt_obj->process_mutex );
        result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
            return result;
        }
        return CY_RSLT_MODULE_MQTT_NOT_CONNECTED;
    }

    /* Send DISCONNECT. */
    mqttStatus = MQTT_Disconnect( &(mqtt_obj->mqtt_context) );
    if( mqttStatus != MQTTSuccess )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Sending MQTT DISCONNECT failed with status=%s.\n",
                         MQTT_Status_strerror( mqttStatus ) );
        /*
         * In case of an unexpected network disconnection, the MQTT_Disconnect API always returns failure. Therefore,
         * the return value of the MQTT_Disconnect API is not checked here.
         */
        /* Fall-through. */
    }

    mqtt_obj->mqtt_session_established = false;
    result = cy_awsport_network_disconnect( &(mqtt_obj->network_context) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_disconnect failed with Error : [0x%X] \n", (unsigned int)result );
        /*
         * In case of an unexpected network disconnection, the cy_awsport_network_disconnect API always returns failure. Therefore,
         * the return value of the cy_awsport_network_disconnect API is not checked here.
         */
        /* Fall-through. */
    }

    result = cy_awsport_network_delete( &(mqtt_obj->network_context) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_delete failed with Error : [0x%X] \n", (unsigned int)result );
        /*
         * In case of an unexpected network disconnection, the cy_awsport_network_delete API always returns failure. Therefore,
         * the return value of the cy_awsport_network_delete API is not checked here.
         */
        /* Fall-through. */
    }
    mqtt_obj->mqtt_conn_status = false;

    /* Reset keepAliveSeconds to 0 to avoid timer activity after disconnection */
    mqtt_obj->keepAliveSeconds = 0;

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_disconnect - Releasing Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_disconnect - Released Mutex %p \n", mqtt_obj->process_mutex );

    return result;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_delete( cy_mqtt_t mqtt_handle )
{
    cy_rslt_t         result = CY_RSLT_SUCCESS;
    cy_mqtt_object_t  *mqtt_obj;

    if( mqtt_handle == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_delete()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    result = cy_rtos_get_mutex( &mqtt_db_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_delete - Acquired Mutex %p \n", mqtt_db_mutex );

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_delete - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        result = cy_rtos_set_mutex( &mqtt_db_mutex );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        }
        return result;
    }

    /* Stop the MQTT Ping timer if still running and deinit timer */
    result = stop_timer( mqtt_obj );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nstop_timer failed\n" );
    }

    result = cy_rtos_deinit_timer( &mqtt_obj->mqtt_timer );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_deinit_timer failed with Error : [0x%X] \n", (unsigned int)result );
    }
    result = cy_rtos_deinit_timer( &mqtt_obj->mqtt_ping_resp_timer );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_deinit_timer failed with Error : [0x%X] \n", (unsigned int)result );
    }


    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        result = cy_rtos_set_mutex( &mqtt_db_mutex );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        }
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_delete - Released Mutex %p \n", mqtt_obj->process_mutex );

    (void)cy_rtos_deinit_mutex( &(mqtt_obj->process_mutex) );

    /* Clear entry in THE MQTT object-mqtt context table. */
    mqtt_handle_database[mqtt_obj->mqtt_obj_index].mqtt_handle = NULL;
    mqtt_handle_database[mqtt_obj->mqtt_obj_index].mqtt_context = NULL;
    mqtt_handle_count--;

    /* Clear the MQTT handle info. */
    ( void ) memset( mqtt_obj, 0x00, sizeof( cy_mqtt_object_t ) );

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n Free mqtt_obj : %p..!\n", mqtt_obj );
    free( mqtt_obj );
    mqtt_handle = NULL;

    result = cy_rtos_set_mutex( &mqtt_db_mutex );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_delete - Released Mutex %p \n", mqtt_db_mutex );

    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_deinit( void )
{
    cy_rslt_t                  result = CY_RSLT_SUCCESS;
    cy_mqtt_callback_event_t   event;

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    if( mqtt_handle_count != 0 )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nMQTT library is deinit cannot be done. Number of MQTT client instance : [%d] \n", mqtt_handle_count );
        return result;
    }

    result = cy_awsport_network_deinit();
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_awsport_network_deinit failed with Error : [0x%X] \n", (unsigned int)result );
        return result;
    }
    else
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_awsport_network_deinit successful.\n" );
    }

    if( mqtt_event_process_thread != NULL )
    {
        event.socket_event = CY_MQTT_SOCKET_EVENT_EXIT_THREAD;
        event.mqtt_obj = NULL;
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nPushing event to terminate mqtt_event_processing_thread \n" );

        result = cy_rtos_put_queue( &mqtt_event_queue, (void *)&event, CY_MQTT_EVENT_QUEUE_TIMEOUT_IN_MSEC, false );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nPushing event to terminate mqtt_event_processing_thread failed with Error : [0x%X] \n", (unsigned int)result );
        }

        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\nJoining MQTT event process thread %p..!\n", mqtt_event_process_thread );
        result = cy_rtos_join_thread( &mqtt_event_process_thread );
        if( result != CY_RSLT_SUCCESS )
        {
            cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nJoin MQTT event process thread failed with Error : [0x%X] \n", (unsigned int)result );
            return result;
        }
        mqtt_event_process_thread = NULL;
    }

    result = cy_rtos_deinit_mutex( &mqtt_db_mutex );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_deinit_mutex failed with Error : [0x%X] \n", (unsigned int)result );
        return result;
    }

    mqtt_db_mutex_init_status = false;

    result = cy_rtos_deinit_queue( &mqtt_event_queue );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_deinit_queue failed with Error : [0x%X] \n", (unsigned int)result );
        return result;
    }
    else
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_rtos_deinit_queue successful.\n" );
    }

    mqtt_lib_init_status = false;
    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_register_event_callback( cy_mqtt_t mqtt_handle,
                                           cy_mqtt_callback_t event_callback,
                                           void *user_data )
{
    cy_rslt_t         result    = CY_RSLT_SUCCESS;
    cy_mqtt_object_t  *mqtt_obj = NULL;
    int i = 0;

    if( mqtt_handle == NULL || event_callback == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_register_event_callback()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( user_data == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\nArgument user_data is NULL..!\n" );
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_register_event_callback - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_register_event_callback - Acquired Mutex %p \n", mqtt_obj->process_mutex );

    for ( i = 0; i < CY_MQTT_MAX_EVENT_CALLBACKS; i++ )
    {
        if ( mqtt_obj->mqtt_event_cb[i] == NULL )
        {
            mqtt_obj->mqtt_event_cb[i] = event_callback;
            mqtt_obj->user_data[i] = user_data;
            break;
        }
    }
    if( i == CY_MQTT_MAX_EVENT_CALLBACKS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Maximum number of callbacks already registered! \r\n");
        (void)cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
        return CY_RSLT_MODULE_MQTT_NOMEM;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_register_event_callback - Releasing Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_register_event_callback - Released Mutex %p \n", mqtt_obj->process_mutex );

    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_deregister_event_callback( cy_mqtt_t mqtt_handle,
                                             cy_mqtt_callback_t event_callback)
{
    cy_rslt_t         result    = CY_RSLT_SUCCESS;
    cy_mqtt_object_t  *mqtt_obj = NULL;
    int i = 0;

    if( mqtt_handle == NULL || event_callback == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_deregister_event_callback()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    if( (mqtt_lib_init_status == false) || (mqtt_db_mutex_init_status == false) )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nLibrary init is not done/Global mutex is not initialized..!\n " );
        return CY_RSLT_MODULE_MQTT_NOT_INITIALIZED;
    }

    mqtt_obj = (cy_mqtt_object_t *)mqtt_handle;

    if( is_mqtt_obj_valid( mqtt_obj ) == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nInvalid MQTT handle..!\n" );
        return CY_RSLT_MODULE_MQTT_INVALID_HANDLE;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_deregister_event_callback - Acquiring Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_get_mutex( &(mqtt_obj->process_mutex), CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_obj->process_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_deregister_event_callback - Acquired Mutex %p \n", mqtt_obj->process_mutex );

    for ( i = 0; i < CY_MQTT_MAX_EVENT_CALLBACKS; i++ )
    {
        if ( mqtt_obj->mqtt_event_cb[i] == event_callback )
        {
            mqtt_obj->mqtt_event_cb[i] = NULL;
            mqtt_obj->user_data[i] = NULL;
            break;
        }
    }
    if( i == CY_MQTT_MAX_EVENT_CALLBACKS )
    {
        cy_mqtt_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Event callback not found! \r\n");
        (void)cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
        return CY_RSLT_MODULE_MQTT_NOMEM;
    }

    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_deregister_event_callback - Releasing Mutex %p \n", mqtt_obj->process_mutex );
    result = cy_rtos_set_mutex( &(mqtt_obj->process_mutex) );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_deregister_event_callback - Released Mutex %p \n", mqtt_obj->process_mutex );

    return CY_RSLT_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/

cy_rslt_t cy_mqtt_get_handle( cy_mqtt_t *mqtt_handle, char *descriptor )
{
    cy_rslt_t result;
    int handle_index = 0;
    bool handle_found = false;
    cy_mqtt_object_t  *mqtt_obj = NULL;

    if( mqtt_handle == NULL || descriptor == NULL )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\nBad arguments to cy_mqtt_get_handle()..!\n" );
        return CY_RSLT_MODULE_MQTT_BADARG;
    }

    result = cy_rtos_get_mutex( &mqtt_db_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_get_handle - Acquired Mutex %p \n", mqtt_db_mutex );

    while( handle_index < mqtt_handle_count )
    {
        mqtt_obj = (cy_mqtt_object_t*)mqtt_handle_database[handle_index].mqtt_handle;
        if( mqtt_obj != NULL )
        {
            if( strcmp(mqtt_obj->mqtt_descriptor, descriptor) == 0 )
            {
                *mqtt_handle = (void*)mqtt_obj;
                handle_found = true;
                break;
            }
        }
        handle_index++;
    }

    if( handle_found == false )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n MQTT handle for descriptor: %s not found..!\n", descriptor );
        (void)cy_rtos_set_mutex( &mqtt_db_mutex );
        return CY_RSLT_MODULE_MQTT_HANDLE_NOT_FOUND;
    }

    result = cy_rtos_set_mutex( &mqtt_db_mutex );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] \n", mqtt_db_mutex, (unsigned int)result );
        return result;
    }
    cy_mqtt_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\ncy_mqtt_get_handle - Released Mutex %p \n", mqtt_db_mutex );

    return CY_RSLT_SUCCESS;
}

#endif
