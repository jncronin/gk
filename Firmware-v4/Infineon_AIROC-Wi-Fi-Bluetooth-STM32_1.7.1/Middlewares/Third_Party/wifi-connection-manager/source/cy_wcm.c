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
* @file cy_wcm.c
* @brief Wi-Fi connection manager (WCM) provides set of APIs that are useful
* to establish and monitor Wi-Fi connection on Cypress platforms that support Wi-Fi connectivity.
* WCM APIs are easy to use compared to WHD APIs and also provides additional features.
* Refer individual APIs for more details.
*/

#include <stdlib.h>

#include "cy_wcm.h"
#include "cy_wcm_log.h"
#include "cy_wcm_error.h"
#include "whd.h"
#include "cyabs_rtos.h"
#include "whd_chip_constants.h"
#include "whd_wlioctl.h"

/* Chip identifiers for WLAN CPUs */
#define CY_WCM_WLAN_CHIP_ID_43907        (43907u)    /**< Chip Identifier for 43907 */
#define CY_WCM_WLAN_CHIP_ID_43909        (43909u)    /**< Chip Identifier for 43909 */
#define CY_WCM_WLAN_CHIP_ID_54907        (54907u)    /**< Chip Identifier for 54907 */


/* Deepsleep time for WLAN CPU */
#ifndef DEFAULT_PM2_SLEEP_RET_TIME
#define DEFAULT_PM2_SLEEP_RET_TIME       (200)
#endif

#if defined(ENABLE_MULTICORE_CONN_MW) && defined(USE_VIRTUAL_API)

#include <stdbool.h>
#include "cy_wcm_internal.h"
#include "cy_vcm_internal.h"

/* This section is virtual-only implementation.
 * The below APIs send the API request to the other core via IPC using the Virtual Connectivity Manager (VCM) library.
 */

static bool                      is_wcm_initialized = false;
static cy_wcm_event_callback_t   virtual_wcm_event_handler[CY_WCM_MAXIMUM_CALLBACKS_COUNT];
static bool                      is_virtual_event_handler_registered = false;
static cy_mutex_t                event_handler_mutex;

static void virtual_event_handler(void *arg)
{
    cy_rslt_t                       res;
    uint8_t                         i;
    cy_wcm_event_callback_t         event_cb;
    cy_wcm_event_callback_params_t  *wcm_event;

    wcm_event = (cy_wcm_event_callback_params_t *)arg;

    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Acquiring Mutex %p ", event_handler_mutex );
    res = cy_rtos_get_mutex( &event_handler_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] ", event_handler_mutex, (unsigned int)res );
        return;
    }

    /* Call the WCM callbacks registered by the application */
    for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
    {
        if ( virtual_wcm_event_handler[i] != NULL )
        {
            event_cb = virtual_wcm_event_handler[i];
            event_cb(wcm_event->event, wcm_event->event_data);
        }
    }

    res = cy_rtos_set_mutex( &event_handler_mutex );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] ", event_handler_mutex, (unsigned int)res );
    }
    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Releasing Mutex %p ", event_handler_mutex );
}

static cy_rslt_t register_virtual_wcm_event_handler()
{
    CY_SECTION_SHAREDMEM
    static cy_wcm_register_event_callback_params_t params;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_vcm_request_t   api_request;
    cy_vcm_response_t  api_response;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "First event callback registration from the secondary core \n");

    params.event_callback = virtual_event_handler;
    memset(&api_request, 0, sizeof(cy_vcm_request_t));
    api_request.api_id = CY_VCM_API_WCM_REG_EVENT_CB;
    api_request.params = &params;

    result = cy_vcm_send_api_request(&api_request, &api_response);
    if( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_vcm_send_api_request failed for cy_wcm_register_event_callback \n");
        return CY_RSLT_WCM_VCM_ERROR;
    }
    if(api_response.result == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "api_response.result is NULL\n");
        return CY_RSLT_WCM_VCM_ERROR;
    }

    is_virtual_event_handler_registered = true;

    return (*((cy_rslt_t *)api_response.result));
}

static cy_rslt_t deregister_virtual_wcm_event_handler()
{
    CY_SECTION_SHAREDMEM
    static cy_wcm_deregister_event_callback_params_t params;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_vcm_request_t   api_request;
    cy_vcm_response_t  api_response;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "De-register the callback with the secondary core as all WCM callbacks are removed \r\n");

    params.event_callback = virtual_event_handler;
    memset(&api_request, 0, sizeof(cy_vcm_request_t));
    api_request.api_id = CY_VCM_API_WCM_DEREG_EVENT_CB;
    api_request.params = &params;

    result = cy_vcm_send_api_request(&api_request, &api_response);
    if( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_vcm_send_api_request failed for cy_wcm_deregister_event_callback \n");
        return CY_RSLT_WCM_VCM_ERROR;
    }
    if(api_response.result == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "api_response.result is NULL\n");
        return CY_RSLT_WCM_VCM_ERROR;
    }
    is_virtual_event_handler_registered = false;

    return (*((cy_rslt_t *)api_response.result));
}

cy_rslt_t cy_wcm_init(cy_wcm_config_t *config)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    int i;

    UNUSED_PARAMETER(config);

    if ( is_wcm_initialized == true )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "\n WCM library is already initialized. \n" );
        return res;
    }

    res = cy_rtos_init_mutex2(&event_handler_mutex, false);
    if ( res != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n Failed to initialize mutex. Res: 0x%x\n", res );
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
    {
        virtual_wcm_event_handler[i] = NULL;
    }

    is_wcm_initialized = true;

    return res;
}

uint8_t cy_wcm_is_connected_to_ap(void)
{
    cy_vcm_request_t   api_request;
    cy_vcm_response_t  api_response;
    cy_rslt_t          res = CY_RSLT_SUCCESS;
    uint8_t            is_connected = 0;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_wcm_is_connected_to_ap starts in secondary core\n");

    if( !is_wcm_initialized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init() \n");
        return is_connected;
    }

    /* Set API request */
    memset(&api_request, 0, sizeof(cy_vcm_request_t));
    api_request.api_id = CY_VCM_API_WCM_IS_CONNECTED_AP;
    api_request.params = NULL;

    /* Send API request */
    res = cy_vcm_send_api_request(&api_request, &api_response);
    if( res != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_vcm_send_api_request failed for cy_wcm_is_connected_to_ap\n");
        return is_connected;
    }
    if( api_response.result == NULL )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "api_response.result is NULL\n");
        return is_connected;
    }
    is_connected = (*(uint8_t*)(api_response.result));
    return is_connected;
}

cy_rslt_t cy_wcm_register_event_callback(cy_wcm_event_callback_t event_callback)
{
    cy_rslt_t         result;
    uint8_t           i;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_wcm_register_event_callback starts in secondary core\n");

    if( !is_wcm_initialized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if( event_callback == NULL )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad arguments to cy_wcm_register_event_callback(). \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Acquiring Mutex %p ", event_handler_mutex );
    result = cy_rtos_get_mutex( &event_handler_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] ", event_handler_mutex, (unsigned int)result );
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    if( is_virtual_event_handler_registered == false )
    {
        /* First register event api_request */
        virtual_wcm_event_handler[0] = event_callback;

        result = register_virtual_wcm_event_handler();
        if( result != CY_RSLT_SUCCESS)
        {
            goto exit;
        }
    }
    else
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Event callback registration from the secondary core \n");
        for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
        {
            if ( virtual_wcm_event_handler[i] == NULL )
            {
                virtual_wcm_event_handler[i] = event_callback;
                result = CY_RSLT_SUCCESS;
                break;
            }
        }
        if(i == CY_WCM_MAXIMUM_CALLBACKS_COUNT)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Out of CY_WCM_MAXIMUM_CALLBACKS_COUNT \r\n");
            result = CY_RSLT_WCM_OUT_OF_MEMORY;
            goto exit;
        }
    }

exit:
    if( cy_rtos_set_mutex( &event_handler_mutex ) != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] ", event_handler_mutex, (unsigned int)result );
    }
    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Releasing Mutex %p ", event_handler_mutex );

    return result;
}

cy_rslt_t cy_wcm_deregister_event_callback(cy_wcm_event_callback_t event_callback)
{
    cy_rslt_t         result;
    uint8_t           i;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_wcm_deregister_event_callback starts in secondary core\n");

    if( !is_wcm_initialized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if( event_callback == NULL )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad arguments to cy_wcm_deregister_event_callback(). \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Acquiring Mutex %p ", event_handler_mutex );
    result = cy_rtos_get_mutex( &event_handler_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] ", event_handler_mutex, (unsigned int)result );
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    if(is_virtual_event_handler_registered == false)
    {
        /* No callbacks to remove */
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to find callback to deregister \r\n");
        result = CY_RSLT_WCM_BAD_ARG;
        goto exit;
    }
    else
    {
        /* Remove the callback from the list if it exists */
        for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
        {
            if ( virtual_wcm_event_handler[i] == event_callback )
            {
                virtual_wcm_event_handler[i] = NULL;
                result = CY_RSLT_SUCCESS;
                break;
            }
        }

        if(i == CY_WCM_MAXIMUM_CALLBACKS_COUNT)
        {
            /* If i == CY_WCM_MAXIMUM_CALLBACKS_COUNT then the callback pointer does not exist in the list */
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to find callback to deregister \r\n");
            result = CY_RSLT_WCM_BAD_ARG;
            goto exit;
        }
        else
        {
            /* Check if the list is empty after removing the callback */
            for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
            {
                if ( virtual_wcm_event_handler[i] != NULL )
                {
                    break;
                }
            }
            /* If i == CY_WCM_MAXIMUM_CALLBACKS_COUNT then the list is empty. So de-register the callback */
            if(i == CY_WCM_MAXIMUM_CALLBACKS_COUNT)
            {
                result = deregister_virtual_wcm_event_handler();
            }
        }
    }

exit:
    if( cy_rtos_set_mutex( &event_handler_mutex ) != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] ", event_handler_mutex, (unsigned int)result );
    }
    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n virtual_event_handler - Releasing Mutex %p ", event_handler_mutex );

    return result;
}

cy_rslt_t cy_wcm_deinit( void )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if ( is_wcm_initialized == false )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    res = cy_rtos_deinit_mutex( &event_handler_mutex );
    if( res != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_deinit_mutex failed with Error : [0x%X] ", (unsigned int)res );
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    is_wcm_initialized = false;

    return res;
}

cy_rslt_t cy_wcm_get_whd_interface(cy_wcm_interface_t interface_type, whd_interface_t *whd_iface)
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_wcm_get_whd_interface is not supported on secondary core \n");
    return CY_RSLT_WCM_UNSUPPORTED_API;
}

#elif (!defined(ENABLE_MULTICORE_CONN_MW)) || (defined(ENABLE_MULTICORE_CONN_MW) && !defined(USE_VIRTUAL_API))
/* This section is full-stack implementation. */

#include "cybsp_wifi.h"
#include "cy_worker_thread.h"
#include "cy_network_mw_core.h"
#include "cy_chip_constants.h"

/* Wi-Fi Host driver includes. */
#include "whd_wifi_api.h"
#include "whd_network_types.h"
#include "whd_buffer_api.h"
#include "whd_wlioctl.h"
#include "whd_types.h"

#include "whd_debug.h"
#include "cy_nw_helper.h"

extern cy_rslt_t wpa3_supplicant_sae_start (uint8_t *ssid, uint8_t ssid_len, uint8_t *passphrase, uint8_t passphrase_len);
/**
 *  Macro for comparing MAC addresses
 */
#define CMP_MAC( a, b )  (((((unsigned char*)a)[0])==(((unsigned char*)b)[0]))&& \
                          ((((unsigned char*)a)[1])==(((unsigned char*)b)[1]))&& \
                          ((((unsigned char*)a)[2])==(((unsigned char*)b)[2]))&& \
                          ((((unsigned char*)a)[3])==(((unsigned char*)b)[3]))&& \
                          ((((unsigned char*)a)[4])==(((unsigned char*)b)[4]))&& \
                          ((((unsigned char*)a)[5])==(((unsigned char*)b)[5])))


/**
 *  Macro for checking for NULL MAC addresses
 */
#define NULL_MAC(a)  ( ( ( ( (unsigned char *)a )[0] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[1] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[2] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[3] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[4] ) == 0 ) && \
                       ( ( ( (unsigned char *)a )[5] ) == 0 ) )


#define CY_WCM_MAX_MUTEX_WAIT_TIME_MS               (120000)
#define CY_WCM_INTERFACE_TYPE_UNKNOWN               (4)
#define CY_WCM_DEFAULT_STA_CHANNEL                  (0)
#define WCM_WORKER_THREAD_PRIORITY                  (CY_RTOS_PRIORITY_ABOVENORMAL)
#ifndef WCM_WORKER_THREAD_STACK_SIZE
#define WCM_WORKER_THREAD_STACK_SIZE                (10 * 1024)
#endif
#define WCM_HANDSHAKE_TIMEOUT_MS                    (3000)
#define WLC_EVENT_MSG_LINK                          (0x01)
#define JOIN_RETRY_ATTEMPTS                         (3)
#define DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS         (1000)
#define MAX_RETRY_BACKOFF_TIMEOUT_IN_MS             (DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS * 32)
#define DHCP_TIMEOUT_COUNT                          (6000) /* 6000 times */
#define UNKNOWN_BAND_WIDTH                          (0)
#define ARP_WAIT_TIME_IN_MSEC                       (30000)
#define ARP_CACHE_CHECK_INTERVAL_IN_MSEC            (5)
#define PING_ID                                     (0xAFAF)  /** ICMP Identifier for PING */
#define PING_DATA_SIZE                              (64)      /** ping additional data size to include in the packet */
#define MAX_WHD_INTERFACE                           (2)
#define MAX_STA_CLIENTS                             (3)
#define TX_BIT_RATE_CONVERTER                       (500)
#define PING_IF_NAME_LEN                            (6)
#define PING_RESPONSE_LEN                           (64)
#define SCAN_BSSID_ARR_LENGTH                       (50)
#define MAX_SCAN_RETRY                              (20)

/* Macro for 43012 statistics */
#define WL_CNT_VER_30                               (30)
#define WL_CNT_VER_10                               (10)
#define CHK_CNTBUF_DATALEN(cntbuf, ioctl_buflen) do { \
if (((wl_cnt_info_t *)cntbuf)->datalen + \
OFFSETOF(wl_cnt_info_t, data) > ioctl_buflen) \
printf("%s: IOVAR buffer short!\n", __FUNCTION__); \
} while (0)

#define NEVER_TIMEOUT                              ((uint32_t) 0xFFFFFFFF)
#define MAX_SEMA_COUNT                             (1)
#define XTLV_OPTION_ALIGN32                        0x0001 /* 32bit alignment of type.len.data */
#define XTLV_OPTION_IDU8                           0x0002 /* shorter id */
#define XTLV_OPTION_LENU8                          0x0004 /* shorted length */
#define WCM_ALIGN_SIZE(size, boundary) (((size) + (boundary) - 1) \
                                             & ~((boundary) - 1))
#define OFFSETOF(type, member)  ( (uintptr_t)&( (type *)0 )->member )
#define _LTOH16_UA(cp) ((cp)[0] | ((cp)[1] << 8))

/* TWT definitions */
#define CY_WCM_TWT_SETUP_CMD                        TWT_SETUP_CMD_SUGGEST_TWT
#define CY_WCM_TWT_WAKE_DURATION_DEAFULT            (8)
#define CY_WCM_TWT_WAKE_DURATION_IDLE_PROF_5G_6G    (2)
#define CY_WCM_TWT_WAKE_DURATION_ACTIVE_PROF        (31)

#define CY_WCM_TWT_MANTISSA                         (600)
#define CY_WCM_TWT_EXPONENT                         (10)

#define CY_WCM_TWT_MANTISSA_ACTIVE_PROF             (50000)
#define CY_WCM_TWT_EXPONENT_ACTIVE_PROF             (0)


/******************************************************
 *             Structures
 ******************************************************/
typedef struct
{
    cy_wcm_scan_result_callback_t p_scan_calback;       /* Callback handler to be invoked to inform caller */
    void*                         user_data;            /* Argument to be passed back to the user while invoking the callback */
    whd_scan_result_t             scan_res;             /* Scan result */
    whd_scan_status_t             scan_status;          /* Scan status */
    cy_wcm_scan_filter_t          scan_filter;          /* Scan filtering type */
    bool                          is_scanning;          /* Indicates if scanning is ongoing */
    bool                          is_stop_scan_req;     /* Indicates if stop scan was requested */
    bool                          get_security_type;    /* Indicates if cy_wcm_connect_ap is trying to find the security type */
}wcm_internal_scan_t;

static wcm_internal_scan_t scan_handler;

typedef struct
{
    whd_ssid_t                    SSID;
    whd_mac_t                     sta_mac;
    cy_wcm_wifi_band_t            band;
    uint8_t                       key[CY_WCM_MAX_PASSPHRASE_LEN];
    uint8_t                       keylen;
    whd_security_t                security;
    cy_network_static_ip_addr_t   static_ip;
}wcm_ap_details;

static wcm_ap_details connected_ap_details;

typedef struct
{
  cy_wcm_event_t   event;
  cy_wcm_mac_t     mac_addr;
} wcm_ap_link_event;


typedef struct xtlv
{
    uint16_t    id;
    uint16_t    len;
    uint8_t     data[1];
} xtlv_t;

/******************************************************
 *               Enumerations
 ******************************************************/
/* Chanspec values. */
typedef enum
{
    WCM_WIFI_CHANSPEC_5GHZ   = 0xC0,    /**< 5-GHz radio band.   */
    WCM_WIFI_CHANSPEC_2_4GHZ = 0x00,    /**< 2.4-GHz radio band. */
    WCM_WIFI_CHANSPEC_6GHZ   = 0x80     /**< 6-GHz radio band.   */
} wcm_wifi_band_chanspec_t;

/******************************************************
 *               Variable Definitions
 ******************************************************/
/* Fixme: Static is removed as STA interface is used for WPS in cy_wcm_wps.c, once get_whd_interface API is available
 * it will be made to static again */

/* Array containing STA Wi-Fi driver and AP Wi-Fi driver */
whd_interface_t whd_ifs[MAX_WHD_INTERFACE];

bool is_wcm_initalized                 = false;
bool is_tcp_initialized                = false;
static cy_mutex_t wcm_mutex;
static cy_wcm_interface_t                current_interface;
static bool wcm_sta_link_up            = false;
static bool is_soft_ap_up              = false;
static bool is_sta_network_up          = false;
static bool is_ap_network_up           = false;
static bool is_sta_interface_created   = false;
static whd_security_t                    sta_security_type;
static cy_worker_thread_info_t           cy_wcm_worker_thread;
static bool is_olm_initialized         = false;
static void *olm_instance              = NULL;
static bool is_disconnect_triggered    = false;
static bool is_connect_triggered       = false;

whd_scan_result_t scan_result;
cy_wcm_scan_result_callback_t p_scan_calback;
cy_wcm_scan_filter_t *p_scan_filter = NULL;

static cy_timer_t sta_handshake_timer;
static cy_timer_t sta_retry_timer;
static cy_wcm_event_callback_t wcm_event_handler[CY_WCM_MAXIMUM_CALLBACKS_COUNT];
static uint16_t sta_event_handler_index   = 0xFF;
static uint16_t ap_event_handler_index    = 0xFF;
static const whd_event_num_t  sta_link_events[] = {WLC_E_LINK, WLC_E_DEAUTH_IND, WLC_E_DISASSOC_IND, WLC_E_PSK_SUP, WLC_E_CSA_COMPLETE_IND, WLC_E_NONE};
static const whd_event_num_t  ap_link_events[]  = {WLC_E_DISASSOC_IND, WLC_E_DEAUTH_IND, WLC_E_ASSOC_IND, WLC_E_REASSOC_IND, WLC_E_AUTHORIZED, WLC_E_NONE};
static bool too_many_ie_error          = false;
static bool link_up_event_received     = false;
static uint32_t retry_backoff_timeout  = DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS;

static whd_mac_t *mac_addr_arr = NULL;
static int current_bssid_arr_length = 0;
static cy_semaphore_t stop_scan_semaphore;
static cy_semaphore_t security_type_start_scan_semaphore;
static cy_wcm_security_t ap_security;
static cy_network_interface_context *nw_ap_if_ctx;
static cy_network_interface_context *nw_sta_if_ctx;

typedef uint16_t xtlv_opts_t;

/******************************************************
 *               Static Function Declarations
 ******************************************************/
static cy_rslt_t check_ap_credentials(const cy_wcm_connect_params_t *connect_params);
static cy_rslt_t convert_connect_params(const cy_wcm_connect_params_t *connect_params, whd_ssid_t *ssid, whd_mac_t *bssid, uint8_t **key, uint8_t *keylen, whd_security_t *security, cy_network_static_ip_addr_t *static_ip_addr);
static bool is_connected_to_same_ap(const cy_wcm_connect_params_t *connect_params);
static bool check_wcm_security(cy_wcm_security_t sec);
static void internal_scan_callback(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status);
static void *link_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header, const uint8_t *event_data, void *handler_user_data);
static void link_up(void);
static void link_down(uint32_t reason);
static void handshake_timeout_handler(cy_timer_callback_arg_t arg);
static void handshake_error_callback(void *arg);
static void lwip_ip_change_callback(cy_network_interface_context *iface_context, void *user_data);
static bool check_if_platform_supports_band(whd_interface_t interface, cy_wcm_wifi_band_t requested_band);
static void sta_link_down_handler(void* arg);
static void sta_link_up_handler(void* arg);
static void sta_link_up_renew_handler(void* arg);
void notify_ip_change(void *arg);
static cy_rslt_t network_up(whd_interface_t interface, cy_network_hw_interface_type_t iface_type, cy_network_static_ip_addr_t *static_ip_ptr);
static void network_down(whd_interface_t interface, cy_network_hw_interface_type_t iface_type);
static void hanshake_retry_timer(cy_timer_callback_arg_t arg);
static void invoke_app_callbacks(cy_wcm_event_t event_type, cy_wcm_event_data_t* arg);
static cy_wcm_bss_type_t  whd_to_wcm_bss_type(whd_bss_type_t bss_type);
static cy_wcm_wifi_band_t whd_to_wcm_band(whd_802_11_band_t band);
static whd_security_t wcm_to_whd_security(cy_wcm_security_t sec);
static uint16_t channel_to_bandwidth(wl_chanspec_t chanspec);
static void notify_connection_status(void* arg);
static cy_rslt_t check_soft_ap_config(const cy_wcm_ap_config_t *ap_config_params);
static void read_ap_config(const cy_wcm_ap_config_t *ap_config, whd_ssid_t *ssid, uint8_t **key, uint8_t *keylen, whd_security_t *security, cy_network_static_ip_addr_t *static_ip_addr);
static void* ap_link_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header, const uint8_t *event_data, void *handler_user_data);
static cy_rslt_t init_whd_wifi_interface(cy_wcm_interface_t iface_type);
static bool check_if_ent_auth_types(cy_wcm_security_t auth_type);

static void unpack_xtlv_buf(const uint8_t *tlv_buf, uint16_t buflen,cy_wcm_wlan_statistics_t *stat);
static cy_rslt_t wl_counters(const uint8_t *data, cy_wcm_wlan_statistics_t *stat);
static void xtlv_unpack_xtlv(const xtlv_t *xtlv, uint16_t *type, uint16_t *len, const uint8_t **data, xtlv_opts_t opts);
static int xtlv_hdr_size(xtlv_opts_t opts, const uint8_t **data);
static int xtlv_id(const xtlv_t *elt, xtlv_opts_t opts);
static int xtlv_len(const xtlv_t *elt, xtlv_opts_t opts);
static int xtlv_size_for_data(int dlen, xtlv_opts_t opts, const uint8_t **data);
static int ltoh16_ua(const uint8_t * bytes);
static void process_scan_data(void *arg);
static void notify_scan_completed(void *arg);

cy_wcm_security_t whd_to_wcm_security(whd_security_t sec);

/******************************************************
 *               Function Definitions
 ******************************************************/
/** Offload Manager create
 *
 * @param[in]  ifp : This is whd_interface_t pointer
 *
 * @param[in]  oflds_list: This is offload list managed by Offload Manager.
 *
 * @return @ref CY_RSLT_SUCCESS if OLM create is success or failure code.
 *
 */
CYPRESS_WEAK cy_rslt_t cy_olm_create(void *ifp, void *oflds_list)
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s \n", __func__);
    return CY_RSLT_SUCCESS;
}

/* Offload Manager de-init called once STA disconnect with AP
 *
 * @param[in] olm: olm instance
 */
CYPRESS_WEAK void cy_olm_deinit_ols(void *olm)
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s \n", __func__);
}

/*
 *  Offload Manager initialization.
 *
 *  @param[in] olm : olm instance
 *
 *  @param[in] whd : whd_interface_t
 *
 *  @param[in] ip  : pointer to ipv4 address
 *
 *  @return @ref CY_RSLT_SUCCESS if success or failure code
 *
 */

CYPRESS_WEAK cy_rslt_t cy_olm_init_ols(void *olm, void *whd, void *ip)
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s \n", __func__);
    return CY_RSLT_SUCCESS;
}

/*
 * Function returns the Offload Manager instance
 *
 *  @return olm: olm pointer
 *
 */
CYPRESS_WEAK void *cy_get_olm_instance( void )
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s \n", __func__);
    return NULL;
}

/*
 *  Function for starting WPA3 EXT SAE supplicant
 */
CYPRESS_WEAK cy_rslt_t wpa3_supplicant_sae_start (uint8_t *ssid, uint8_t ssid_len, uint8_t *passphrase, uint8_t passphrase_len)
{
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "%s \n", __func__);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_init(cy_wcm_config_t *config)
{
    cy_worker_thread_params_t params;
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if((config == NULL) ||
       ((config->interface != CY_WCM_INTERFACE_TYPE_AP_STA) && (config->interface  != CY_WCM_INTERFACE_TYPE_STA) && (config->interface != CY_WCM_INTERFACE_TYPE_AP)))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : WCM init failed due to bad arguments \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "WCM is already initialized \n");
        return CY_RSLT_SUCCESS;
    }

    /** Initialize network stack **/
    if(!is_tcp_initialized)
    {
        res = cy_network_init();
        if(res != CY_RSLT_SUCCESS)
        {
            return CY_RSLT_WCM_NW_INIT_ERROR;
        }
        is_tcp_initialized = true;
    }

    if (cy_rtos_init_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    /* Initialize semaphore */
    if(cy_rtos_init_semaphore(&stop_scan_semaphore, MAX_SEMA_COUNT, 0) != CY_RSLT_SUCCESS)
    {
        cy_rtos_deinit_mutex(&wcm_mutex);
        return CY_RSLT_WCM_SEMAPHORE_ERROR;
    }

    if(cy_rtos_init_semaphore(&security_type_start_scan_semaphore, MAX_SEMA_COUNT, 0) != CY_RSLT_SUCCESS)
    {
        cy_rtos_deinit_semaphore(&stop_scan_semaphore);
        cy_rtos_deinit_mutex(&wcm_mutex);
        return CY_RSLT_WCM_SEMAPHORE_ERROR;
    }

    if((res = init_whd_wifi_interface(config->interface)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : Initializing Wi-Fi interface \n");
        cy_rtos_deinit_semaphore(&security_type_start_scan_semaphore);
        cy_rtos_deinit_semaphore(&stop_scan_semaphore);
        cy_rtos_deinit_mutex(&wcm_mutex);
        return res;
    }

    memset(&params, 0, sizeof(params));
    params.name = "WCM- Worker";
    params.priority = WCM_WORKER_THREAD_PRIORITY;
    params.stack = NULL;
    params.stack_size = WCM_WORKER_THREAD_STACK_SIZE;
    params.num_entries = 0;
    /* create a worker thread */
    if(cy_worker_thread_create(&cy_wcm_worker_thread, &params) != CY_RSLT_SUCCESS)
    {
        cy_rtos_deinit_semaphore(&security_type_start_scan_semaphore);
        cy_rtos_deinit_semaphore(&stop_scan_semaphore);
        cy_rtos_deinit_mutex(&wcm_mutex);
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    if(config->interface != CY_WCM_INTERFACE_TYPE_AP)
    {
        cy_rtos_init_timer(&sta_handshake_timer, CY_TIMER_TYPE_ONCE, handshake_timeout_handler, 0);
        cy_rtos_init_timer(&sta_retry_timer, CY_TIMER_TYPE_ONCE, hanshake_retry_timer, 0);
        scan_handler.is_scanning = false;
        olm_instance = cy_get_olm_instance();
        if(olm_instance != NULL)
        {
            /* Offload Manager create used only for STA interface
             * not required for Soft AP interface
             */
            cy_olm_create(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], NULL);
        }
        else
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Off loads not configured \n");
        }
        /* Register to ip change callback from LwIP, all other internal callbacks are in WCM */
        cy_network_register_ip_change_cb(nw_sta_if_ctx, lwip_ip_change_callback, NULL);
    }

    memset(wcm_event_handler, 0, sizeof(wcm_event_handler));
    current_interface = config->interface;
    is_wcm_initalized = true;
    return res;
}

cy_rslt_t cy_wcm_deinit()
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    /** Check if there are any active connections and disconnect **/
    if ((res = cy_wcm_disconnect_ap()) != CY_RSLT_SUCCESS)
    {
        wcm_sta_link_up = false;
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while disconnecting  \n");
        res = CY_RSLT_WCM_STA_DISCONNECT_ERROR;
    }

    if((res = cy_wcm_stop_scan()) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while stopping scan  \n");
    }

    if((res = cy_wcm_stop_ap()) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while stopping AP \n");
    }

    if((res = cy_rtos_deinit_semaphore(&stop_scan_semaphore)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while de initializing stop_scan_semaphore semaphore \n");
    }

    if((res = cy_rtos_deinit_semaphore(&security_type_start_scan_semaphore)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while de initializing security_type_start_scan_semaphore semaphore \n");
    }


    if((res = cy_rtos_deinit_mutex(&wcm_mutex)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while de initializing mutex \n");
    }

    if(current_interface != CY_WCM_INTERFACE_TYPE_AP_STA)
    {
        if ((res = cybsp_wifi_deinit(whd_ifs[current_interface])) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in bringing down the interface \n");
        }
    }
    else
    {
        /* In concurrent bringing down AP interface, followed by STA interface */
        if((res = whd_deinit(whd_ifs[CY_WCM_INTERFACE_TYPE_AP])) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in bringing down AP interface \n");
        }

        if ((res = cybsp_wifi_deinit(whd_ifs[CY_WCM_INTERFACE_TYPE_STA])) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in bringing down STA interface \n");
        }
    }

    if(current_interface != CY_WCM_INTERFACE_TYPE_AP)
    {
        cy_rtos_deinit_timer(&sta_handshake_timer);
        cy_rtos_deinit_timer(&sta_retry_timer);
        retry_backoff_timeout = DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS;
    }
    cy_worker_thread_delete(&cy_wcm_worker_thread);
    is_wcm_initalized = false;

    return res;
}

cy_rslt_t cy_wcm_start_scan(cy_wcm_scan_result_callback_t callback, void *user_data, cy_wcm_scan_filter_t *scan_filter)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    whd_ssid_t *ssid = NULL;
    whd_mac_t *mac = NULL;
    uint32_t band;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if (callback == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(scan_handler.get_security_type)
    {
        res = CY_RSLT_WCM_CONNECT_IN_PROGRESS;
        goto exit;
    }

    if(scan_handler.is_scanning)
    {
        res = CY_RSLT_WCM_SCAN_IN_PROGRESS;
        goto exit;
    }

    /* Reset mac_addr_arr before each scan and set current_bssid_arr_length to 0 */
    mac_addr_arr = (whd_mac_t *)malloc(SCAN_BSSID_ARR_LENGTH * sizeof(whd_mac_t) );
    if (mac_addr_arr == NULL)
    {
        res = CY_RSLT_WCM_OUT_OF_MEMORY;
        goto exit;
    }
    current_bssid_arr_length = 0;

    /* reset previous filter and by default set band to AUTO */
    memset(&scan_handler.scan_filter, 0, sizeof(cy_wcm_scan_filter_t));
    whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_AUTO);

    /* Store the scan callback and user data */
    scan_handler.p_scan_calback = callback;
    scan_handler.user_data = user_data;
    if(scan_filter != NULL)
    {
        /** copy the filter setting to reuse in the internal scan handler **/
        memcpy(&scan_handler.scan_filter, scan_filter, sizeof(cy_wcm_scan_filter_t));
        switch(scan_filter->mode)
        {
            case CY_WCM_SCAN_FILTER_TYPE_SSID:
                /** Copy the SSID **/
                ssid = (whd_ssid_t*)malloc(sizeof(whd_ssid_t));
                if(ssid == NULL)
                {
                    res =  CY_RSLT_WCM_OUT_OF_MEMORY;
                    goto exit;
                }
                ssid->length = strlen((char*)scan_filter->param.SSID);
                memcpy(ssid->value, scan_filter->param.SSID, ssid->length + 1);
                break;
            case CY_WCM_SCAN_FILTER_TYPE_MAC:
                /** Copy the MAC **/
                mac = (whd_mac_t*)malloc(sizeof(whd_mac_t));
                if(mac == NULL)
                {
                    res =  CY_RSLT_WCM_OUT_OF_MEMORY;
                    goto exit;
                }
                memcpy(mac->octet, scan_filter->param.BSSID, CY_WCM_MAC_ADDR_LEN);
                break;
            case CY_WCM_SCAN_FILTER_TYPE_BAND:
                 /* check if the platform supports the requested band */
                if(!check_if_platform_supports_band(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], scan_filter->param.band))
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "band not supported \n");
                    res = CY_RSLT_WCM_BAND_NOT_SUPPORTED;
                    goto exit;
                }
                if(scan_filter->param.band == CY_WCM_WIFI_BAND_5GHZ)
                {
                    band = WLC_BAND_5G;
                }
                else if(scan_filter->param.band == CY_WCM_WIFI_BAND_2_4GHZ)
                {
                    band = WLC_BAND_2G;
                }
                else if(scan_filter->param.band == CY_WCM_WIFI_BAND_6GHZ)
                {
                    band = WLC_BAND_6G;
                }
                else
                {
                    band = WLC_BAND_AUTO;
                }
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, band);
                break;
            case CY_WCM_SCAN_FILTER_TYPE_RSSI:
                    /**
                    * Do nothing, the scan filter is copied into scan_handler
                    * which will be used in worker thread to process the received RSSI
                    */
                break;
            default:
                break;
        }
    }

    res = whd_wifi_scan(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WHD_SCAN_TYPE_ACTIVE, WHD_BSS_TYPE_ANY,
                                 ssid, mac, NULL, NULL, internal_scan_callback, &scan_result, user_data);
    if(res != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_scan error. Result = %d \n", res);
        res = CY_RSLT_WCM_SCAN_ERROR;
        goto exit;
    }
    scan_handler.is_scanning = true;
    scan_handler.is_stop_scan_req = false;

exit:
    /* Free the allocated memory in case of SSID or MAC based scan */
    if(ssid != NULL)
    {
        free(ssid);
        ssid = NULL;
    }
    if(mac != NULL)
    {
        free(mac);
        mac = NULL;
    }
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

cy_rslt_t cy_wcm_stop_scan()
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    bool wait_on_sema = false;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(scan_handler.get_security_type)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_wcm_connect_ap has initiated an internal scan to find the security type of AP. No active user scan running. \n");
        res =  CY_RSLT_WCM_CONNECT_IN_PROGRESS;
        goto exit;
    }

    if(!scan_handler.is_scanning)
    {
        res =  CY_RSLT_WCM_NO_ACTIVE_SCAN;
        goto exit;
    }

    if((res = whd_wifi_stop_scan(whd_ifs[CY_WCM_INTERFACE_TYPE_STA]) != CY_RSLT_SUCCESS))
    {
        res = CY_RSLT_WCM_STOP_SCAN_ERROR;
        goto exit;
    }
    scan_handler.is_stop_scan_req = true;
    wait_on_sema = true;

exit:
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    if(wait_on_sema)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wait on stop scan semaphore %s %d\r\n", __FILE__, __LINE__);
        if(cy_rtos_get_semaphore(&stop_scan_semaphore, NEVER_TIMEOUT, false) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error unable to get semaphore \n");
            res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_SEMAPHORE_ERROR);
        }
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "obtained stop scan semaphore %s %d\r\n", __FILE__, __LINE__);
    }

    return res;
}

cy_rslt_t cy_wcm_register_event_callback(cy_wcm_event_callback_t event_callback)
{
    uint8_t i;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( !is_wcm_initalized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if( event_callback == NULL )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad arguments to cy_wcm_register_event_callback(). \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n wcm_mutex - Acquiring Mutex %p ", wcm_mutex );
    result = cy_rtos_get_mutex( &wcm_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] ", wcm_mutex, (unsigned int)result );
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
    {
        if ( wcm_event_handler[i] == NULL )
        {
            wcm_event_handler[i] = event_callback;
            break;
        }
    }

    if( i == CY_WCM_MAXIMUM_CALLBACKS_COUNT )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Out of CY_WCM_MAXIMUM_CALLBACKS_COUNT \r\n");
        result = CY_RSLT_WCM_OUT_OF_MEMORY;
        goto exit;
    }

exit:
    if( cy_rtos_set_mutex( &wcm_mutex ) != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] ", wcm_mutex, (unsigned int)result );
    }
    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n wcm_mutex - Releasing Mutex %p ", wcm_mutex );

    return result;
}

cy_rslt_t cy_wcm_deregister_event_callback(cy_wcm_event_callback_t event_callback)
{
    uint8_t i;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( !is_wcm_initalized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if( event_callback == NULL )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad arguments to cy_wcm_register_event_callback(). \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n wcm_mutex - Acquiring Mutex %p ", wcm_mutex );
    result = cy_rtos_get_mutex( &wcm_mutex, CY_RTOS_NEVER_TIMEOUT );
    if( result != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_get_mutex for Mutex %p failed with Error : [0x%X] ", wcm_mutex, (unsigned int)result );
        return CY_RSLT_WCM_MUTEX_ERROR;
    }

    for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
    {
        if ( wcm_event_handler[i] == event_callback )
        {
            memset( &wcm_event_handler[i], 0, sizeof( cy_wcm_event_callback_t ) );
            break;
        }
    }

    if( i == CY_WCM_MAXIMUM_CALLBACKS_COUNT )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to find callback to deregister \r\n");
        result = CY_RSLT_WCM_BAD_ARG;
        goto exit;
    }

exit:
    if( cy_rtos_set_mutex( &wcm_mutex ) != CY_RSLT_SUCCESS )
    {
        cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_set_mutex for Mutex %p failed with Error : [0x%X] ", wcm_mutex, (unsigned int)result );
    }
    cy_wcm_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n wcm_mutex - Releasing Mutex %p ", wcm_mutex );

    return result;
}

/* whd scan callback to find the security type when user has not
 * provided the security type to cy_wcm_connect_ap
 */
static void internal_scan_cb_get_security_type(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status)
{
    if(status == WHD_SCAN_ABORTED)
    {
        return;
    }
    if(status == WHD_SCAN_COMPLETED_SUCCESSFULLY)
    {
        goto exit;
    }
    else if(status == WHD_SCAN_INCOMPLETE)
    {
        if((**result_ptr).SSID.length != 0)
        {
            cy_wcm_connect_params_t *connect_params = (cy_wcm_connect_params_t *)user_data;

            if(strcmp((char *)(**result_ptr).SSID.value, (char *)connect_params->ap_credentials.SSID) != 0)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Scanned SSID did not match SSID passed by user \n");
                return;
            }
            if (connect_params->ap_credentials.security == CY_WCM_SECURITY_UNKNOWN)
            {
                connect_params->ap_credentials.security = whd_to_wcm_security((**result_ptr).security);
            }
        }
    }

exit:
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return;
    }

    scan_handler.is_scanning = false;
    scan_handler.get_security_type = false;

    if(cy_rtos_set_semaphore(&security_type_start_scan_semaphore, false) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "unable to set security_type_start_scan_semaphore \n");
    }

    /* We have received the security type of the interested SSID. It is possible that this
     * callback is called again with same SSID which will cause to trigger the semaphore again.
     * That will cause issues in cy_wcm_connect_ap. Setting 'result_ptr' NULL to inform WHD
     * to abort the scan.
     */
    if(result_ptr != NULL)
    {
        *result_ptr = NULL;
    }

    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlock error %s %d\r\n", __FILE__, __LINE__);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);
}

cy_rslt_t cy_wcm_scan_security_type(void *user_data)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_wcm_connect_params_t *connect_params = (cy_wcm_connect_params_t*)user_data;
    whd_ssid_t optional_ssid;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(scan_handler.is_scanning)
    {
        res = CY_RSLT_WCM_SCAN_IN_PROGRESS;
        goto exit;
    }

    optional_ssid.length = (uint8_t)strlen((char*)connect_params->ap_credentials.SSID);
    memcpy(optional_ssid.value, connect_params->ap_credentials.SSID, optional_ssid.length + 1);

    res = whd_wifi_scan(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WHD_SCAN_TYPE_ACTIVE, WHD_BSS_TYPE_ANY,
                        &optional_ssid , NULL, NULL, NULL, internal_scan_cb_get_security_type, &scan_result, user_data);
    if(res != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_scan error. Result = %d \n", res);
        res = CY_RSLT_WCM_SCAN_ERROR;
        goto exit;
    }
    scan_handler.is_scanning = true;
    scan_handler.get_security_type = true;

exit:
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlock error %s %d\r\n", __FILE__, __LINE__);
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}
cy_rslt_t cy_wcm_connect_ap(cy_wcm_connect_params_t *connect_params, cy_wcm_ip_address_t *ip_addr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    whd_ssid_t ssid;
    whd_mac_t bssid;
    uint8_t *key;
    uint8_t keylen;
    whd_security_t security;
    cy_network_static_ip_addr_t static_ip, *static_ip_ptr;
    static_ip_ptr = NULL;
    uint32_t retry_count = 0;
    cy_nw_ip_address_t ipv4_addr;
    uint32_t connection_status;
    uint8_t num_scan = 0;
#ifdef ENABLE_WCM_LOGS
    char ip_str[15];
#endif
    uint32_t ext_sae_support = 0;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    /* Security type not specified by user then scan to figure out the security type */
    if(connect_params->ap_credentials.security == CY_WCM_SECURITY_UNKNOWN)
    {
        while(connect_params->ap_credentials.security == CY_WCM_SECURITY_UNKNOWN && num_scan < MAX_SCAN_RETRY )
        {
            res = cy_wcm_scan_security_type(connect_params);
            if (res != CY_RSLT_SUCCESS)
            {
                if (res == CY_RSLT_WCM_SCAN_IN_PROGRESS)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Scan in progress...wait and try again \n");
                    cy_rtos_delay_milliseconds(500);
                    continue;
                }
                else
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Scan failed... \n");
                    return res;
                }
            }
            if(cy_rtos_get_semaphore(&security_type_start_scan_semaphore, NEVER_TIMEOUT, false) != CY_RSLT_SUCCESS)
            {
                 cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error unable to set the security type semaphore \n");
            }
            /* Stop scan before starting the next scan */
            whd_wifi_stop_scan(whd_ifs[CY_WCM_INTERFACE_TYPE_STA]);
            num_scan++;
        }

        if(connect_params->ap_credentials.security == CY_WCM_SECURITY_UNKNOWN)
        {
            /* Failed to get the security type of network */
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to get the security type of network \n");
            return CY_RSLT_WCM_SECURITY_NOT_FOUND;
        }
    }

    memset(&connected_ap_details, 0, sizeof(connected_ap_details));
    if((res = check_ap_credentials(connect_params)) != CY_RSLT_SUCCESS)
    {
        return res;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if (cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) == CY_RSLT_SUCCESS)
    {
        whd_scan_result_t ap;
        is_disconnect_triggered = false;
        is_connect_triggered = true;
        if (is_connected_to_same_ap(connect_params))
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "already connected to same AP \n");
            /* Store the IP address before returning */

            res = cy_network_get_ip_address(nw_sta_if_ctx, &ipv4_addr);
            if(res == CY_RSLT_SUCCESS)
            {
                ip_addr->version = CY_WCM_IP_VER_V4;
                ip_addr->ip.v4 = ipv4_addr.ip.v4;
            }
            else
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
                res = CY_RSLT_WCM_IP_ADDR_ERROR;
            }

            goto exit;
        }
        if (wcm_sta_link_up  && (cy_wcm_disconnect_ap() != CY_RSLT_SUCCESS))
        {
            /**
             *  Notify user disconnection error occurred and
             *  reset the is_wifi_connected flag to false
             */
            wcm_sta_link_up = false;
            res = CY_RSLT_WCM_STA_DISCONNECT_ERROR;
            goto exit;
        }

        convert_connect_params(connect_params, &ssid, &bssid, &key, &keylen, &security, &static_ip);
        sta_security_type = security;

        connection_status = CY_WCM_EVENT_CONNECTING;
        whd_wifi_get_fwcap(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &ext_sae_support);

        if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status)) != CY_RSLT_SUCCESS)
        {
             cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
             goto exit;
        }
        if ( ( ext_sae_support & (1 << WHD_FWCAP_SAE_EXT))
             && ((connect_params->ap_credentials.security == CY_WCM_SECURITY_WPA3_SAE)
             ||  (connect_params->ap_credentials.security == CY_WCM_SECURITY_WPA3_WPA2_PSK)))
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "calling wpa3_supplicant_sae_start\n");
            /* supplicant SAE Start */
            res = wpa3_supplicant_sae_start(ssid.value, ssid.length, key, keylen);
            if ( res != CY_RSLT_SUCCESS)
            {
                res = CY_RSLT_WCM_WPA3_SUPPLICANT_ERROR;
                goto exit;
            }
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wpa3_supplicant_sae_start returned res=%d\n", res);
        }

        if(!NULL_MAC(bssid.octet))
        {
            memset(&ap, 0, sizeof(whd_scan_result_t));
            ap.security = security;
            ap.SSID.length = ssid.length;
            ap.channel = CY_WCM_DEFAULT_STA_CHANNEL;
            memcpy(ap.SSID.value, ssid.value, ap.SSID.length + 1);
            memcpy(ap.BSSID.octet, bssid.octet, CY_WCM_MAC_ADDR_LEN);
            /*
             * If MAC address of a AP is know there is no need to populate channel or band
             * instead we can set the band to auto and invoke whd join
             */
            whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_AUTO);
            res = whd_wifi_join_specific(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &ap, key, keylen);
        }
        else
        {
            if(connect_params->band == CY_WCM_WIFI_BAND_2_4GHZ)
            {
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_2G);
            }
            else if((connect_params->band == CY_WCM_WIFI_BAND_5GHZ) || (connect_params->band == CY_WCM_WIFI_BAND_6GHZ))
            {
                /* check if this band is supported locally */
                if(check_if_platform_supports_band(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], connect_params->band))
                {
                    whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, connect_params->band);
                }
                else
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "band not supported \n");
                    res =  CY_RSLT_WCM_BAND_NOT_SUPPORTED;
                    connection_status = CY_WCM_EVENT_CONNECT_FAILED;
                    if((cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status)) != CY_RSLT_SUCCESS)
                    {
                        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                    }
                    goto exit;
                }
            }
            else
            {
                /* If band is not specified set the band to AUTO */
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_AUTO);
            }
            /** Join to Wi-Fi AP **/
            res = whd_wifi_join(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &ssid, security, key, keylen);
        }
        if (res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "whd_wifi join failed : %ld \n", res);
            connection_status = CY_WCM_EVENT_CONNECT_FAILED;
            if((cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status)) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
            }
            if(res == WHD_UNSUPPORTED)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Security type not supported, result = %d \n", res);
                res = CY_RSLT_WCM_SECURITY_NOT_SUPPORTED;
            }
            goto exit;
        }

        if (!is_sta_network_up)
        {
            if(connect_params->static_ip_settings != NULL)
            {
                static_ip_ptr = &static_ip;
            }

            if((res = network_up(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], CY_NETWORK_WIFI_STA_INTERFACE, static_ip_ptr)) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to bring up the network stack\n");
                res = CY_RSLT_WCM_STA_NETWORK_DOWN;
                connection_status = CY_WCM_EVENT_CONNECT_FAILED;
                if((cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status)) != CY_RSLT_SUCCESS)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                }
                goto exit;
            }

            /** wait in busy loop till dhcp starts and ip address gets assigned **/
            while (true)
            {
                res = cy_network_get_ip_address(nw_sta_if_ctx, &ipv4_addr);
                if (res == CY_RSLT_SUCCESS)
                {
#ifdef ENABLE_WCM_LOGS
                    cy_nw_ntoa(&ipv4_addr, ip_str);
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV4 Address %s assigned \n", ip_str);
#endif
                    if(ip_addr != NULL)
                    {
                        ip_addr->version = CY_WCM_IP_VER_V4;
                        ip_addr->ip.v4 = ipv4_addr.ip.v4;
                    }
                    break;
                }
                // TO DO : get ipv6 address
                /* Delay of 10 ms */
                cy_rtos_delay_milliseconds(10);
                /* Increment count for every 10 ms */
                retry_count++;
                /* Return DHCP Timeout Error when it exceeds 6000 * 10 ms = 60 seconds */
                if (retry_count > DHCP_TIMEOUT_COUNT)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "DHCP Timeout \n");
                    /* do disconnect to bring network down as DHCP failed */
                    if (cy_wcm_disconnect_ap() != CY_RSLT_SUCCESS)
                    {
                        /**
                         *  Notify user disconnection error occurred and
                         *  reset the is_wifi_connected flag to false
                         */
                        wcm_sta_link_up = false;
                        res = CY_RSLT_WCM_STA_DISCONNECT_ERROR;
                        connection_status = CY_WCM_EVENT_CONNECT_FAILED;
                        if(cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status) != CY_RSLT_SUCCESS)
                        {
                            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                        }
                        goto exit;
                    }
                    /* Return DHCP Timeout Error when DHCP discover failed and disconnect done properly */
                    res = CY_RSLT_WCM_DHCP_TIMEOUT;
                    connection_status = CY_WCM_EVENT_CONNECT_FAILED;
                    if(cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status) != CY_RSLT_SUCCESS)
                    {
                        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                    }
                    goto exit;
                }
            }

            /* Call Offload init after connect to AP */
            if ((is_olm_initialized == false) && ( olm_instance != NULL))
            {
                cy_olm_init_ols(olm_instance, whd_ifs[CY_WCM_INTERFACE_TYPE_STA], NULL);
                is_olm_initialized = true;
            }

            /* Register for Link events*/
            res = whd_management_set_event_handler(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], sta_link_events, link_events_handler, NULL, &sta_event_handler_index);
            if(res != CY_RSLT_SUCCESS)
            {
                /* bring down the network and leave */
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to register for Link events \n");
                network_down(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], CY_NETWORK_WIFI_STA_INTERFACE);
                whd_wifi_leave(whd_ifs[CY_WCM_INTERFACE_TYPE_STA]);
                connection_status = CY_WCM_EVENT_CONNECT_FAILED;
                if(cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status) != CY_RSLT_SUCCESS)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                }
                goto exit;
            }

            if(connect_params->itwt_profile != CY_WCM_ITWT_PROFILE_NONE)
            {
                wl_bss_info_t  bss_info;

                /* iTWT parameter values are based on the band, so first get the band */
                res = whd_wifi_get_bss_info(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &bss_info);
                if(res == CY_RSLT_SUCCESS)
                {
                    uint8_t band = 0;
                    whd_itwt_setup_params_t twt_params;

                    twt_params.setup_cmd     = CY_WCM_TWT_SETUP_CMD;
                    twt_params.trigger       = true;
                    twt_params.flow_type     = true;
                    twt_params.flow_id       = 0;
                    twt_params.wake_duration = CY_WCM_TWT_WAKE_DURATION_DEAFULT;
                    twt_params.exponent      = CY_WCM_TWT_EXPONENT;
                    twt_params.mantissa      = CY_WCM_TWT_MANTISSA;
                    twt_params.wake_time_h   = 0;
                    twt_params.wake_time_l   = 0;

                    band = (bss_info.chanspec & 0xC000) >> 8;
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "band : [0x%X]\n",band);

                    if(connect_params->itwt_profile == CY_WCM_ITWT_PROFILE_IDLE)
                    {
                        if( (band == WCM_WIFI_CHANSPEC_5GHZ) || (band == WCM_WIFI_CHANSPEC_6GHZ) )
                        {
                            twt_params.wake_duration = CY_WCM_TWT_WAKE_DURATION_IDLE_PROF_5G_6G;
                        }
                    }
                    else if(connect_params->itwt_profile == CY_WCM_ITWT_PROFILE_ACTIVE)
                    {
                        twt_params.wake_duration = CY_WCM_TWT_WAKE_DURATION_ACTIVE_PROF;
                        twt_params.exponent      = CY_WCM_TWT_EXPONENT_ACTIVE_PROF;
                        twt_params.mantissa      = CY_WCM_TWT_MANTISSA_ACTIVE_PROF;
                    }
                    res = whd_wifi_itwt_setup(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &twt_params);
                    if( res != CY_RSLT_SUCCESS)
                    {
                        if( res == WHD_WLAN_UNSUPPORTED )
                        {
                            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "iTWT Not supported for this capabilities\n");
                        }

                        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_itwt_setup failed %x\r\n", res);
                    }
                }
            }

            /* save current AP credentials to reuse during retry in case handshake fails occurs */
            connected_ap_details.security = security;
            connected_ap_details.SSID.length = ssid.length;
            connected_ap_details.keylen = keylen;
            connected_ap_details.band = connect_params->band;
            memcpy(connected_ap_details.key, key, keylen+1);
            memcpy(connected_ap_details.SSID.value, ssid.value, connected_ap_details.SSID.length+1);
            memcpy(connected_ap_details.sta_mac.octet, bssid.octet, CY_WCM_MAC_ADDR_LEN);
            memcpy(&connected_ap_details.static_ip, &static_ip, sizeof(static_ip));
            wcm_sta_link_up = true;
            connection_status = CY_WCM_EVENT_CONNECTED;
            if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status)) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                goto exit;
            }
            /* post IP change callback */
            if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_ip_change, NULL)) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : Failed to notify IP change. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                goto exit;
            }
        }
    }
    else
    {
        res = CY_RSLT_WCM_WAIT_TIMEOUT;
        connection_status = CY_WCM_EVENT_CONNECT_FAILED;
        if(cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
        }
    }

exit:
    is_connect_triggered = false;
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Mutex release error \n");
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

cy_rslt_t cy_wcm_disconnect_ap()
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    /* Deregister the link event handler */
    whd_wifi_deregister_event_handler(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], sta_event_handler_index);
    network_down(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], CY_NETWORK_WIFI_STA_INTERFACE);
    res = whd_wifi_leave(whd_ifs[CY_WCM_INTERFACE_TYPE_STA]);
    if (res != CY_RSLT_SUCCESS)
    {
        goto exit;
    }
    wcm_sta_link_up = false;

    /* Call Offload deinit after disconnect to AP */
    if ( (is_olm_initialized == true) && (olm_instance != NULL) )
    {
       cy_olm_deinit_ols(olm_instance);
       is_olm_initialized = false;
    }

exit:
    is_disconnect_triggered = true;
    is_connect_triggered = false;
    /* clear the saved ap credentials */
    memset(&connected_ap_details, 0, sizeof(connected_ap_details));
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

cy_rslt_t cy_wcm_get_ip_addr(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *ip_addr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_nw_ip_address_t ipv4_addr;
#ifdef ENABLE_WCM_LOGS
    char ip_str[15];
#endif
    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface_type != CY_WCM_INTERFACE_TYPE_AP_STA) && (interface_type  != CY_WCM_INTERFACE_TYPE_STA) && (interface_type != CY_WCM_INTERFACE_TYPE_AP))
    {
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(ip_addr == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(interface_type == CY_WCM_INTERFACE_TYPE_STA)
    {
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_STA_NETWORK_DOWN;
        }

        res = cy_network_get_ip_address(nw_sta_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        ip_addr->version = CY_WCM_IP_VER_V4;
        ip_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IP Address %s assigned \n", ip_str);
#endif
        return CY_RSLT_SUCCESS;
    }
    else if(interface_type == CY_WCM_INTERFACE_TYPE_AP)
    {
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_start_ap API to bring up Soft AP \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_AP_NOT_UP;
        }

        res = cy_network_get_ip_address(nw_ap_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        ip_addr->version = CY_WCM_IP_VER_V4;
        ip_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IP Address %s assigned \n", ip_str);
#endif
        return CY_RSLT_SUCCESS;
    }
    else
    {
        /* fill the 0th index with STA IP address and 1st index with AP IP address */
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
        }

        res = cy_network_get_ip_address(nw_sta_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        ip_addr->version = CY_WCM_IP_VER_V4;
        ip_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "STA IP Address %s assigned \n", ip_str);
#endif

        ip_addr++;
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
        }
        res = cy_network_get_ip_address(nw_ap_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        ip_addr->version = CY_WCM_IP_VER_V4;
        ip_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Soft AP IP Address %s assigned \n", ip_str);
#endif
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_get_ipv6_addr(cy_wcm_interface_t interface_type, cy_wcm_ipv6_type_t ipv6_addr_type, cy_wcm_ip_address_t *ip_addr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_nw_ip_address_t ipv6_addr;
    cy_network_ipv6_type_t ipv6_type;
#ifdef ENABLE_WCM_LOGS
    char ip_str[39];
#endif
    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface_type != CY_WCM_INTERFACE_TYPE_AP_STA) && (interface_type  != CY_WCM_INTERFACE_TYPE_STA) && (interface_type != CY_WCM_INTERFACE_TYPE_AP))
    {
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(ip_addr == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(ipv6_addr_type != CY_WCM_IPV6_LINK_LOCAL)
    {
        /*TODO : Need to add support for Global IPV6 address */
        return CY_RSLT_WCM_IPV6_GLOBAL_ADDRESS_NOT_SUPPORTED;
    }
    ipv6_type = CY_NETWORK_IPV6_LINK_LOCAL;

    if(interface_type == CY_WCM_INTERFACE_TYPE_STA)
    {
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_STA_NETWORK_DOWN;
        }

        res = cy_network_get_ipv6_address(nw_sta_if_ctx, ipv6_type, &ipv6_addr);
        if(res == CY_RSLT_SUCCESS)
        {
            ip_addr->version = CY_WCM_IP_VER_V6;
            ip_addr->ip.v6[0] = ipv6_addr.ip.v6[0];
            ip_addr->ip.v6[1] = ipv6_addr.ip.v6[1];
            ip_addr->ip.v6[2] = ipv6_addr.ip.v6[2];
            ip_addr->ip.v6[3] = ipv6_addr.ip.v6[3];

#ifdef ENABLE_WCM_LOGS
            cy_nw_ntoa_ipv6(&ipv6_addr, ip_str);
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV6 Address %s assigned \n", ip_str);
#endif
        }
        else
        {
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready \n");
            return CY_RSLT_WCM_IPV6_INTERFACE_NOT_READY;
        }
    }
    else if(interface_type == CY_WCM_INTERFACE_TYPE_AP)
    {
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_start_ap API to start SoftAP \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_AP_NOT_UP;
        }

        res = cy_network_get_ipv6_address(nw_ap_if_ctx, ipv6_type, &ipv6_addr);
        if(res == CY_RSLT_SUCCESS)
        {
            ip_addr->version = CY_WCM_IP_VER_V6;
            ip_addr->ip.v6[0] = ipv6_addr.ip.v6[0];
            ip_addr->ip.v6[1] = ipv6_addr.ip.v6[1];
            ip_addr->ip.v6[2] = ipv6_addr.ip.v6[2];
            ip_addr->ip.v6[3] = ipv6_addr.ip.v6[3];
#ifdef ENABLE_WCM_LOGS
            cy_nw_ntoa_ipv6(&ipv6_addr, ip_str);
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV6 Address %s assigned \n", ip_str);
#endif
        }
        else
        {
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready \n");
            return CY_RSLT_WCM_IPV6_INTERFACE_NOT_READY;
        }
    }
    else
    {
        /* fill the 0th index with STA IP address and 1st index with AP IP address */
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
        }

        res = cy_network_get_ipv6_address(nw_sta_if_ctx, ipv6_type, &ipv6_addr);
        if(res == CY_RSLT_SUCCESS)
        {
            ip_addr->version = CY_WCM_IP_VER_V6;
            ip_addr->ip.v6[0] = ipv6_addr.ip.v6[0];
            ip_addr->ip.v6[1] = ipv6_addr.ip.v6[1];
            ip_addr->ip.v6[2] = ipv6_addr.ip.v6[2];
            ip_addr->ip.v6[3] = ipv6_addr.ip.v6[3];

#ifdef ENABLE_WCM_LOGS
            cy_nw_ntoa_ipv6(&ipv6_addr, ip_str);
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV6 Address %s assigned \n", ip_str);
#endif
        }
        else
        {
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready \n");
        }

        ip_addr++;
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_start_ap API to start SoftAP \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
        }

        res = cy_network_get_ipv6_address(nw_ap_if_ctx, ipv6_type, &ipv6_addr);
        if(res == CY_RSLT_SUCCESS)
        {
            ip_addr->version = CY_WCM_IP_VER_V6;
            ip_addr->ip.v6[0] = ipv6_addr.ip.v6[0];
            ip_addr->ip.v6[1] = ipv6_addr.ip.v6[1];
            ip_addr->ip.v6[2] = ipv6_addr.ip.v6[2];
            ip_addr->ip.v6[3] = ipv6_addr.ip.v6[3];
#ifdef ENABLE_WCM_LOGS
            cy_nw_ntoa_ipv6(&ipv6_addr, ip_str);
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV6 Address %s assigned \n", ip_str);
#endif
        }
        else
        {
            memset(ip_addr, 0, sizeof(cy_wcm_ip_address_t));
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready \n");
        }
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_get_mac_addr(cy_wcm_interface_t interface_type, cy_wcm_mac_t *mac_addr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    whd_mac_t mac;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface_type != CY_WCM_INTERFACE_TYPE_AP_STA) && (interface_type  != CY_WCM_INTERFACE_TYPE_STA) && (interface_type != CY_WCM_INTERFACE_TYPE_AP))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Interface not supported \r\n");
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(mac_addr == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad argument \r\n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(((current_interface == CY_WCM_INTERFACE_TYPE_AP) && (interface_type == CY_WCM_INTERFACE_TYPE_STA)))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : Interface is not up \r\n");
        res = CY_RSLT_WCM_INTERFACE_NOT_UP;
        memset(mac_addr, 0, sizeof(mac));
        goto exit;
    }

    if((current_interface == CY_WCM_INTERFACE_TYPE_STA) && (interface_type == CY_WCM_INTERFACE_TYPE_AP))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : Interface is not up \r\n");
        res = CY_RSLT_WCM_INTERFACE_NOT_UP;
        memset(mac_addr, 0, sizeof(mac));
        goto exit;
    }

    memset(&mac, 0, sizeof(whd_mac_t));
    if((interface_type == CY_WCM_INTERFACE_TYPE_STA) || (interface_type == CY_WCM_INTERFACE_TYPE_AP))
    {
        if((res = whd_wifi_get_mac_address(whd_ifs[interface_type], &mac) != CY_RSLT_SUCCESS))
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in getting the mac address of the interface \r\n");
            memset(mac_addr, 0, sizeof(mac));
            goto exit;
        }
        memcpy(mac_addr, &mac, sizeof(mac));
    }
    else
    {
        memset(mac_addr, 0, (MAX_WHD_INTERFACE * sizeof(mac)));
        if((res = whd_wifi_get_mac_address(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &mac)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in getting the mac address of STA interface \r\n");
            goto exit;
        }
        else
        {
            memcpy(mac_addr, &mac, sizeof(mac));
        }

        mac_addr++;
        memset(&mac, 0, sizeof(whd_mac_t));

        if((res = whd_wifi_get_mac_address(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], &mac)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in getting the mac address of AP interface \r\n");
        }
        else
        {
            memcpy(mac_addr, &mac, sizeof(mac));
        }
    }
 exit:
    if(cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

cy_rslt_t cy_wcm_get_gateway_ip_address(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *gateway_addr)
{
    cy_nw_ip_address_t ipv4_addr;
    cy_rslt_t res = CY_RSLT_SUCCESS;
#ifdef ENABLE_WCM_LOGS
    char ip_str[15];
#endif

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface_type != CY_WCM_INTERFACE_TYPE_AP_STA) && (interface_type  != CY_WCM_INTERFACE_TYPE_STA) && (interface_type != CY_WCM_INTERFACE_TYPE_AP))
    {
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(gateway_addr == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(interface_type == CY_WCM_INTERFACE_TYPE_STA)
    {
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(gateway_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_STA_NETWORK_DOWN;
        }

        res = cy_network_get_gateway_ip_address(nw_sta_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the gateway address\n");
            return CY_RSLT_WCM_GATEWAY_ADDR_ERROR;
        }
        gateway_addr->version = CY_WCM_IP_VER_V4;
        gateway_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Gateway IP Address %s assigned \n", ip_str);
#endif
        return CY_RSLT_SUCCESS;
    }
    else if(interface_type == CY_WCM_INTERFACE_TYPE_AP)
    {
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(gateway_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_STA_NETWORK_DOWN;
        }
        res = cy_network_get_gateway_ip_address(nw_ap_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the gateway address\n");
            return CY_RSLT_WCM_GATEWAY_ADDR_ERROR;
        }
        gateway_addr->version = CY_WCM_IP_VER_V4;
        gateway_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Gateway IP Address %s assigned \n", ip_str);
#endif
        return CY_RSLT_SUCCESS;
    }
    else
    {
        /* fill the 0th index with STA IP address and 1st index with AP IP address */
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(gateway_addr, 0, sizeof(cy_wcm_ip_address_t));
        }
        res = cy_network_get_gateway_ip_address(nw_sta_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the gateway address\n");
            return CY_RSLT_WCM_GATEWAY_ADDR_ERROR;
        }
        gateway_addr->version = CY_WCM_IP_VER_V4;
        gateway_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "STA Gateway IP Address %s assigned \n", ip_str);
#endif

        gateway_addr++;
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(gateway_addr, 0, sizeof(cy_wcm_ip_address_t));
        }
        res = cy_network_get_gateway_ip_address(nw_ap_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the gateway address\n");
            return CY_RSLT_WCM_GATEWAY_ADDR_ERROR;
        }
        gateway_addr->version = CY_WCM_IP_VER_V4;
        gateway_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Soft AP IP Address %s assigned \n", ip_str);
#endif
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_get_ip_netmask(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *net_mask_addr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_nw_ip_address_t ipv4_addr;
#ifdef ENABLE_WCM_LOGS
    char ip_str[15];
#endif
    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface_type != CY_WCM_INTERFACE_TYPE_AP_STA) && (interface_type  != CY_WCM_INTERFACE_TYPE_STA) && (interface_type != CY_WCM_INTERFACE_TYPE_AP))
    {
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(net_mask_addr == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(interface_type == CY_WCM_INTERFACE_TYPE_STA)
    {
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(net_mask_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_STA_NETWORK_DOWN;
        }

        res = cy_network_get_netmask_address(nw_sta_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the netmask address\n");
            return CY_RSLT_WCM_NETMASK_ADDR_ERROR;
        }

        net_mask_addr->version = CY_WCM_IP_VER_V4;
        net_mask_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "net mask IP Address %s assigned \n", ip_str);
#endif
        return CY_RSLT_SUCCESS;
    }
    else if(interface_type == CY_WCM_INTERFACE_TYPE_AP)
    {
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(net_mask_addr, 0, sizeof(cy_wcm_ip_address_t));
            return CY_RSLT_WCM_STA_NETWORK_DOWN;
        }

        res = cy_network_get_netmask_address(nw_ap_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        net_mask_addr->version = CY_WCM_IP_VER_V4;
        net_mask_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "net mask Address %s assigned \n", ip_str);
#endif
        return CY_RSLT_SUCCESS;
    }
    else
    {
        /* fill the 0th index with STA IP address and 1st index with AP IP address */
        if(is_sta_network_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(net_mask_addr, 0, sizeof(cy_wcm_ip_address_t));
        }

        res = cy_network_get_netmask_address(nw_sta_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        net_mask_addr->version = CY_WCM_IP_VER_V4;
        net_mask_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "STA Gateway IP Address %s assigned \n", ip_str);
#endif

        net_mask_addr++;
        if(is_soft_ap_up == false)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
            /** Network is not up and hence mem setting the ip address to zero**/
            memset(net_mask_addr, 0, sizeof(cy_wcm_ip_address_t));
        }
        res = cy_network_get_netmask_address(nw_ap_if_ctx, &ipv4_addr);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the IP address\n");
            return CY_RSLT_WCM_IP_ADDR_ERROR;
        }
        net_mask_addr->version = CY_WCM_IP_VER_V4;
        net_mask_addr->ip.v4 = ipv4_addr.ip.v4;
#ifdef ENABLE_WCM_LOGS
        cy_nw_ntoa(&ipv4_addr, ip_str);
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Soft AP net mask Address %s assigned \n", ip_str);
#endif
    }
    return CY_RSLT_SUCCESS;
}

uint8_t cy_wcm_is_connected_to_ap(void)
{
    return (wcm_sta_link_up ? 1 : 0);
}

uint8_t cy_wcm_is_ap_up(void)
{
    return (is_soft_ap_up ? 1 : 0);
}

cy_rslt_t cy_wcm_get_associated_ap_info(cy_wcm_associated_ap_info_t *ap_info)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    wl_bss_info_t  bss_info;
    whd_security_t security;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if(ap_info == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(!wcm_sta_link_up)
    {
        return CY_RSLT_WCM_STA_NETWORK_DOWN;
    }

    if(whd_wifi_get_ap_info(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &bss_info, &security) == CY_RSLT_SUCCESS)
    {
        memcpy(ap_info->BSSID, bss_info.BSSID.octet, CY_WCM_MAC_ADDR_LEN);
        memcpy(ap_info->SSID, bss_info.SSID, bss_info.SSID_len);
        ap_info->SSID[bss_info.SSID_len] = 0;
        ap_info->security = whd_to_wcm_security(security);
        ap_info->channel = bss_info.ctl_ch;
        ap_info->signal_strength = bss_info.RSSI;
        ap_info->channel_width = channel_to_bandwidth(bss_info.chanspec);
        if(ap_info->channel_width == UNKNOWN_BAND_WIDTH)
        {
            res = CY_RSLT_WCM_CHANNEL_ERROR;
        }
    }

    return res;
}

static int ltoh16_ua(const uint8_t * bytes)
{
    const uint8_t *_bytes = (const uint8_t *)(bytes);
    return _LTOH16_UA(_bytes);

}

static int xtlv_size_for_data(int dlen, xtlv_opts_t opts, const uint8_t **data)
{
    int hsz;
    hsz = xtlv_hdr_size(opts, data);
    return ((opts & XTLV_OPTION_ALIGN32) ? WCM_ALIGN_SIZE(dlen + hsz, 4) : (dlen + hsz));
}

static int xtlv_len(const xtlv_t *elt, xtlv_opts_t opts)
{
    const uint8_t *lenp;
    int len;

    lenp = (const uint8_t *)&elt->len; /* nominal */
    if (opts & XTLV_OPTION_IDU8)
    {
        --lenp;
    }
    if (opts & XTLV_OPTION_LENU8)
    {
        len = *lenp;
    }
    else
    {
        len = ltoh16_ua(lenp);
    }
    return len;
}

static int xtlv_id(const xtlv_t *elt, xtlv_opts_t opts)
{
    int id = 0;
    if (opts & XTLV_OPTION_IDU8)
    {
        id =  *(const uint8_t *)elt;
    }
    else
    {
        id = ltoh16_ua((const uint8_t *)elt);
    }
    return id;
}

static int xtlv_hdr_size(xtlv_opts_t opts, const uint8_t **data)
{
    int len = (int)OFFSETOF(xtlv_t, data); /* nominal */
    if (opts & XTLV_OPTION_LENU8)
    {
        --len;
    }
    if (opts & XTLV_OPTION_IDU8)
    {
        --len;
    }
    return len;
}

static void xtlv_unpack_xtlv(const xtlv_t *xtlv, uint16_t *type, uint16_t *len,
    const uint8_t **data, xtlv_opts_t opts)
{
    if (type)
    {
        *type = (uint16_t)xtlv_id(xtlv, opts);
    }
    if (len)
    {
        *len = (uint16_t)xtlv_len(xtlv, opts);
    }
    if (data)
    {
        *data = (const uint8_t *)xtlv + xtlv_hdr_size(opts, data);
    }
}

static void unpack_xtlv_buf(const uint8_t *tlv_buf, uint16_t buflen,cy_wcm_wlan_statistics_t *stat)
{
    uint16_t len;
    uint16_t type;
    int size;
    const xtlv_t *ptlv;
    int sbuflen = buflen;
    const uint8_t *data;
    int hdr_size;

    hdr_size = xtlv_hdr_size(XTLV_OPTION_ALIGN32, &data);
    while (sbuflen >= hdr_size) {
        ptlv = (const xtlv_t *)tlv_buf;

        xtlv_unpack_xtlv(ptlv, &type, &len, &data, XTLV_OPTION_ALIGN32);
        size = xtlv_size_for_data(len, XTLV_OPTION_ALIGN32, &data);

        sbuflen -= size;
        if (sbuflen < 0) /* check for buffer overrun */
        {
            break;
        }
        if (type == 0x100)
        {
            if(wl_counters(data, stat) == CY_RSLT_SUCCESS)
            {
                break;
            }
        }
        tlv_buf += size;
    }
    return;
}

static cy_rslt_t wl_counters(const uint8_t *data, cy_wcm_wlan_statistics_t *stat)
{
    wl_cnt_ver_30_t *cnt = (wl_cnt_ver_30_t *)data;
    stat->rx_bytes   = cnt->rxbyte;
    stat->tx_bytes   = cnt->txbyte;
    stat->rx_packets = cnt->rxfrag;
    stat->tx_packets = cnt->txfrag;
    stat->tx_failed  = cnt->txfail;
    stat->tx_retries = cnt->txretry;
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_get_wlan_statistics(cy_wcm_interface_t interface, cy_wcm_wlan_statistics_t *stat)
{
    uint32_t data_rate;
    wl_cnt_ver_ten_t* received_counters;
    uint8_t buffer[WLC_IOCTL_MEDLEN];
    wl_cnt_info_t* wl_cnt_info ;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if(interface != CY_WCM_INTERFACE_TYPE_STA)
    {
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(stat == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    CHECK_RETURN(whd_wifi_get_iovar_buffer(whd_ifs[ CY_WCM_INTERFACE_TYPE_STA], IOVAR_STR_COUNTERS, buffer, WLC_IOCTL_MEDLEN));
    wl_cnt_info = (wl_cnt_info_t * )buffer;

    if (wl_cnt_info->version == WL_CNT_VER_30)
    {
        /* 43012 board - Process xtlv buffer data to get statistics */
        uint8_t *cntdata;
        cntdata = (uint8_t *)malloc(wl_cnt_info->datalen);

        CHK_CNTBUF_DATALEN(wl_cnt_info, WLC_IOCTL_MEDLEN);
        if (cntdata == NULL) {
            return CY_RSLT_TYPE_ERROR;
        }
        memcpy(cntdata, wl_cnt_info->data, wl_cnt_info->datalen);
        unpack_xtlv_buf(cntdata, wl_cnt_info->datalen, stat);
        free(cntdata);
    }
    else if (wl_cnt_info->version == WL_CNT_VER_10)
    {
        received_counters =(wl_cnt_ver_ten_t*)  wl_cnt_info;

        /* Copy the required statistics */
        stat->rx_bytes   = received_counters->rxbyte;
        stat->tx_bytes   = received_counters->txbyte;
        stat->rx_packets = received_counters->rxfrag;
        stat->tx_packets = received_counters->txfrag;
        stat->tx_failed  = received_counters->txfail;
        stat->tx_retries = received_counters->txretry;
    }

    /* get data rate */
    CHECK_RETURN (whd_wifi_get_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_GET_RATE, &data_rate));
    /* The data rate received is in units of 500Kbits per sec, convert to Kbits per sec*/
    stat->tx_bitrate = (data_rate * TX_BIT_RATE_CONVERTER);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_get_gateway_mac_address(cy_wcm_mac_t *mac_addr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_nw_ip_mac_t nw_mac_addr;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if(mac_addr == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(current_interface == CY_WCM_INTERFACE_TYPE_AP)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "feature not supported for AP interface \r\n");
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if(!is_sta_network_up)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Network is not up, call cy_wcm_connect_ap API to bring network up \r\n");
        return CY_RSLT_WCM_NETIF_DOES_NOT_EXIST;
    }

    memset(mac_addr , 0, sizeof(*mac_addr));
    res = cy_network_get_gateway_mac_address(nw_sta_if_ctx, &nw_mac_addr);
    if(res != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to get the Gateway MAC address\n");
        res = CY_RSLT_WCM_GATEWAY_MAC_ADDR_ERROR;
    }
    (*mac_addr)[0] = nw_mac_addr.mac[0];
    (*mac_addr)[1] = nw_mac_addr.mac[1];
    (*mac_addr)[2] = nw_mac_addr.mac[2];
    (*mac_addr)[3] = nw_mac_addr.mac[3];
    (*mac_addr)[4] = nw_mac_addr.mac[4];
    (*mac_addr)[5] = nw_mac_addr.mac[5];
    return res;
}

cy_rslt_t cy_wcm_ping(cy_wcm_interface_t interface, cy_wcm_ip_address_t* address, uint32_t timeout_ms, uint32_t* elapsed_time_ms)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface != CY_WCM_INTERFACE_TYPE_STA) && (interface != CY_WCM_INTERFACE_TYPE_AP))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "interface not supported \r\n");
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    if((address == NULL) || (elapsed_time_ms == NULL))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "BAD arguments \r\n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    if((interface == CY_WCM_INTERFACE_TYPE_STA) && (wcm_sta_link_up == false))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "STA network down \r\n");
        return CY_RSLT_WCM_STA_NETWORK_DOWN;
    }

    if((interface == CY_WCM_INTERFACE_TYPE_AP) && (is_soft_ap_up == false))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AP not up \r\n");
        return CY_RSLT_WCM_AP_NOT_UP;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    /* Call cy_network_ping() */
    if(interface == CY_WCM_INTERFACE_TYPE_AP)
    {
        res = cy_network_ping((void *)nw_ap_if_ctx, (cy_nw_ip_address_t *) address, timeout_ms, elapsed_time_ms);
    }
    else
    {
        res = cy_network_ping((void *)nw_sta_if_ctx, (cy_nw_ip_address_t *) address, timeout_ms, elapsed_time_ms);
    }

    if(cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to release the mutex \n");
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

cy_rslt_t cy_wcm_start_ap(const cy_wcm_ap_config_t *ap_config)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    whd_ssid_t ssid;
    uint8_t *key;
    uint8_t keylen;
    uint16_t chanspec = 0;
    whd_security_t security;
    cy_network_static_ip_addr_t static_ip;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((res = check_soft_ap_config(ap_config)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad soft ap config \n");
        return res;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(is_soft_ap_up)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "AP is already Up !!\n");
        res = CY_RSLT_WCM_AP_ALREADY_UP;
        goto exit;
    }

    read_ap_config(ap_config, &ssid, &key, &keylen, &security, &static_ip);
    /* Store the SoftAP security type */
    ap_security = whd_to_wcm_security(security);

    /* Construct chanspec from band and channel number */
    if(ap_config->band == CY_WCM_WIFI_BAND_5GHZ)
    {
        chanspec = WCM_WIFI_CHANSPEC_5GHZ;
    }
    else if(ap_config->band == CY_WCM_WIFI_BAND_6GHZ)
    {
        chanspec = WCM_WIFI_CHANSPEC_6GHZ;
    }
    else
    {
        /* 2.4 GHz band */
        chanspec = WCM_WIFI_CHANSPEC_2_4GHZ;
    }

    /* Add channel info */
    chanspec = ((chanspec << 8) | ap_config->channel);

    /* set up the AP info */
    res = whd_wifi_init_ap(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], &ssid, security, (const uint8_t *)key,
                           keylen, chanspec);
    if (res != CY_RSLT_SUCCESS)
    {
        if(res == WHD_UNSUPPORTED || res == WHD_WEP_NOT_ALLOWED)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Security type not supported, result = %d \n", res);
            res = CY_RSLT_WCM_SECURITY_NOT_SUPPORTED;
        }
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "ERROR initializing AP = %d \n", res);
        goto exit;
    }

    if (ap_config->ie_info)
    {
        res = whd_wifi_manage_custom_ie(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], WHD_ADD_CUSTOM_IE, (const uint8_t *)ap_config->ie_info->oui,
                                        ap_config->ie_info->subtype, (const void *)ap_config->ie_info->data, ap_config->ie_info->length, ap_config->ie_info->ie_packet_mask);
        if (res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "ERROR while adding Custom IE = %d\n", res);
            goto exit;
        }
    }

    res = whd_wifi_start_ap(whd_ifs[CY_WCM_INTERFACE_TYPE_AP]);
    if (res != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "unable to start AP \n");
        goto exit;
    }

    if((res = network_up(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], CY_NETWORK_WIFI_AP_INTERFACE, &static_ip)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to bring up the network stack\n");
        res = CY_RSLT_WCM_STA_NETWORK_DOWN;
        /* stop the AP, stopping of AP will remove custom IE */
        whd_wifi_stop_ap(whd_ifs[CY_WCM_INTERFACE_TYPE_AP]);
        goto exit;
    }

    /* Register for AP Link events*/
    if((res = whd_management_set_event_handler(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], ap_link_events, ap_link_events_handler, NULL, &ap_event_handler_index)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to set the Event handler \n");
        network_down(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], CY_NETWORK_WIFI_AP_INTERFACE);
        whd_wifi_stop_ap(whd_ifs[CY_WCM_INTERFACE_TYPE_AP]);
        goto exit;
    }
    is_soft_ap_up = true;

exit:
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to release the mutex \n");
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

cy_rslt_t cy_wcm_stop_ap(void)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(is_soft_ap_up)
    {
        network_down(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], CY_NETWORK_WIFI_AP_INTERFACE);
        /* De-register of AP link event handler */
        whd_wifi_deregister_event_handler(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], ap_event_handler_index);
        res = whd_wifi_stop_ap(whd_ifs[CY_WCM_INTERFACE_TYPE_AP]);
        if (res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WHD stop ap failed \n");
            goto exit;
        }
        is_soft_ap_up = false;
    }
exit:
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}


cy_rslt_t cy_wcm_get_associated_client_list(cy_wcm_mac_t *client_list, uint8_t num_clients)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    whd_maclist_t *mac_list_ptr;
    uint32_t      buf_length;

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. To initialize call cy_wcm_init(). \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if(client_list == NULL || num_clients == 0)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad arguments \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return CY_RSLT_WCM_WAIT_TIMEOUT;
    }

    if(current_interface == CY_WCM_INTERFACE_TYPE_STA)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "operation not allowed for STA Interface \n");
        res = CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
        goto exit;
    }

    if(!is_ap_network_up)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "AP is not Up !!\n");
        res = CY_RSLT_WCM_AP_NOT_UP;
        goto exit;
    }

    buf_length   = (sizeof(whd_mac_t) * num_clients) + sizeof(mac_list_ptr->count);
    mac_list_ptr = (whd_maclist_t*)malloc(buf_length);
    if(mac_list_ptr == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Heap space error \n");
        res = CY_RSLT_WCM_OUT_OF_MEMORY;
        goto exit;
    }
    memset(mac_list_ptr, 0, buf_length);
    mac_list_ptr->count = num_clients;

    if((res = whd_wifi_get_associated_client_list(whd_ifs[CY_WCM_INTERFACE_TYPE_AP], mac_list_ptr, buf_length)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error while getting associated client list, error = %d \n", res);
        free(mac_list_ptr);
        goto exit;
    }

    memcpy(client_list, mac_list_ptr->mac_list, (sizeof(whd_mac_t) * num_clients));
    free(mac_list_ptr);

exit:
    if(cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        res = ((res != CY_RSLT_SUCCESS) ? res : CY_RSLT_WCM_MUTEX_ERROR);
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    return res;
}

static uint16_t channel_to_bandwidth(wl_chanspec_t chanspec)
{
    uint16_t band_width;
    switch(chanspec & WL_CHANSPEC_BW_MASK)
    {
        case WL_CHANSPEC_BW_5 :
            band_width = 5;
            break;

        case WL_CHANSPEC_BW_10 :
            band_width = 10;
            break;

        case WL_CHANSPEC_BW_20 :
            band_width = 20;
            break;

        case WL_CHANSPEC_BW_40 :
            band_width = 40;
            break;

        case WL_CHANSPEC_BW_80 :
            band_width = 80;
            break;

        case WL_CHANSPEC_BW_160 :
            band_width = 160;
            break;

        case WL_CHANSPEC_BW_8080 :
            band_width = 8080;
            break;

        default :
            band_width = UNKNOWN_BAND_WIDTH;
            break;
    }
    return band_width;
 }

static cy_rslt_t check_ap_credentials(const cy_wcm_connect_params_t *connect_params)
{
    uint8_t ssid_len;
    uint8_t pwd_len;
    cy_wcm_security_t sec_type;

    if(connect_params == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    ssid_len = (uint8_t)strlen((char*)connect_params->ap_credentials.SSID);
    pwd_len  = (uint8_t)strlen((char*)connect_params->ap_credentials.password);

    if(ssid_len == 0 || ssid_len > CY_WCM_MAX_SSID_LEN )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "SSID length error\n");
        return CY_RSLT_WCM_BAD_SSID_LEN;
    }
    if(!check_wcm_security(connect_params->ap_credentials.security))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AP credentials security error\n");
        return CY_RSLT_WCM_SECURITY_NOT_SUPPORTED;
    }

    sec_type = connect_params->ap_credentials.security ;
    /* For open, owe and enterprise security auth types pwd len can be ignored */
    if(((sec_type != CY_WCM_SECURITY_OPEN) &&
#ifdef COMPONENT_WIFI6
        (sec_type != CY_WCM_SECURITY_OWE) &&
#endif
        (!check_if_ent_auth_types(sec_type))) &&
       (pwd_len < CY_WCM_MIN_PASSPHRASE_LEN || pwd_len > CY_WCM_MAX_PASSPHRASE_LEN))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AP credentials passphrase length error. Length must be between 8 and 63\n");
        return CY_RSLT_WCM_BAD_PASSPHRASE_LEN;
    }
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t convert_connect_params(const cy_wcm_connect_params_t *connect_params, whd_ssid_t *ssid, whd_mac_t *bssid,
                                          uint8_t **key, uint8_t *keylen, whd_security_t *security, cy_network_static_ip_addr_t *static_ip_addr)
{
    ssid->length = (uint8_t)strlen((char*)connect_params->ap_credentials.SSID);
    memcpy(&ssid->value, connect_params->ap_credentials.SSID, ssid->length + 1);

    *keylen = strlen((char*)connect_params->ap_credentials.password);
    *key = (uint8_t*)connect_params->ap_credentials.password;
    if(!NULL_MAC(connect_params->BSSID))
    {
        memcpy(bssid, connect_params->BSSID, CY_WCM_MAC_ADDR_LEN);
    }
    else
    {
        memset(bssid, 0, sizeof(whd_mac_t));
    }
    *security = wcm_to_whd_security(connect_params->ap_credentials.security);
    if(connect_params->static_ip_settings == NULL)
    {
        memset(static_ip_addr, 0, sizeof(cy_network_static_ip_addr_t));
    }
    else
    {
        memset(static_ip_addr, 0, sizeof(cy_network_static_ip_addr_t));
        if(connect_params->static_ip_settings->gateway.version == CY_WCM_IP_VER_V4)
        {
             static_ip_addr->gateway.ip.v4 = connect_params->static_ip_settings->gateway.ip.v4;
             static_ip_addr->addr.ip.v4    = connect_params->static_ip_settings->ip_address.ip.v4;
             static_ip_addr->netmask.ip.v4 = connect_params->static_ip_settings->netmask.ip.v4;

        }
        else
        {
            // TO DO : Copy the IPV6 addr
            return CY_RSLT_WCM_STATIC_IP_NOT_SUPPORTED;
        }
    }
    return CY_RSLT_SUCCESS;
}

static bool is_connected_to_same_ap(const cy_wcm_connect_params_t *connect_params)
{
    wl_bss_info_t  bss_info;

    if(whd_wifi_get_bss_info(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &bss_info) != CY_RSLT_SUCCESS)
    {
        return false;
    }
    if((strcmp((char*)connect_params->ap_credentials.SSID, (char*)bss_info.SSID) == 0) && wcm_sta_link_up)
    {
        return true;
    }
    return false;
}



static bool check_wcm_security(cy_wcm_security_t sec)
{
    switch (sec) {
        case CY_WCM_SECURITY_OPEN:
        case CY_WCM_SECURITY_WPA_TKIP_PSK:
        case CY_WCM_SECURITY_WPA_AES_PSK:
        case CY_WCM_SECURITY_WPA_MIXED_PSK:
        case CY_WCM_SECURITY_WPA2_AES_PSK:
        case CY_WCM_SECURITY_WPA2_AES_PSK_SHA256:
        case CY_WCM_SECURITY_WPA2_TKIP_PSK:
        case CY_WCM_SECURITY_WPA2_MIXED_PSK:
        case CY_WCM_SECURITY_WPA2_FBT_PSK:
        case CY_WCM_SECURITY_WPA3_SAE:
        case CY_WCM_SECURITY_WPA2_WPA_AES_PSK:
        case CY_WCM_SECURITY_WPA2_WPA_MIXED_PSK:
        case CY_WCM_SECURITY_WPA3_WPA2_PSK:
        case CY_WCM_SECURITY_WPA_TKIP_ENT:
        case CY_WCM_SECURITY_WPA_AES_ENT:
        case CY_WCM_SECURITY_WPA_MIXED_ENT:
        case CY_WCM_SECURITY_WPA2_TKIP_ENT:
        case CY_WCM_SECURITY_WPA2_AES_ENT:
        case CY_WCM_SECURITY_WPA2_MIXED_ENT:
        case CY_WCM_SECURITY_WPA2_FBT_ENT:
        case CY_WCM_SECURITY_IBSS_OPEN:
        case CY_WCM_SECURITY_WPS_SECURE:
#ifdef COMPONENT_WIFI6
        case CY_WCM_SECURITY_OWE:
#endif
#ifdef ENABLE_ENTPS_FEATURE
        case CY_WCM_SECURITY_WPA3_192BIT_ENT:
        case CY_WCM_SECURITY_WPA3_ENT:
        case CY_WCM_SECURITY_WPA3_ENT_AES_CCMP:
#endif
            return true;
        default:
            return false;
    }
}

void internal_scan_callback(whd_scan_result_t **result_ptr,
                             void *user_data, whd_scan_status_t status)
{
    whd_scan_result_t *whd_scan_result;
    uint32_t scan_status = status;

    /* Check if we don't have a scan result to send to the user */
    if (( result_ptr == NULL ) || ( *result_ptr == NULL ))
    {
        /* Check for scan complete */
        if (status == WHD_SCAN_COMPLETED_SUCCESSFULLY || status == WHD_SCAN_ABORTED)
        {
            /* Notify scan complete */
            if(cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_scan_completed, (void *)scan_status) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in calling the worker thread func \n");
            }
        }
        return;
    }

    whd_scan_result = (whd_scan_result_t *)malloc(sizeof(whd_scan_result_t));
    if (whd_scan_result == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Heap space error \n");
        goto exit;
    }
    memcpy(whd_scan_result, *result_ptr, sizeof(whd_scan_result_t));
    if(cy_worker_thread_enqueue(&cy_wcm_worker_thread, process_scan_data, whd_scan_result) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in calling the worker thread func \n");
        free(whd_scan_result);
        goto exit;
    }

exit:
    memset(*result_ptr, 0, sizeof(whd_scan_result_t));
    return;
}

static void notify_scan_completed(void *arg)
{
    uint32_t val = (uint32_t)arg;
    whd_scan_status_t scan_status = (whd_scan_status_t)val;
    cy_wcm_scan_result_t scan_res;
    bool invoke_application_callback = false;
    memset(&scan_res, 0, sizeof(scan_res));

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return;
    }

    scan_handler.scan_status = scan_status;
    memset(&scan_handler.scan_res, 0x0, sizeof(whd_scan_result_t));

    /* Invoke application callback only on successful scan completion. Do not invoke application callback on Abort/stop scan */
    if(scan_status == WHD_SCAN_COMPLETED_SUCCESSFULLY)
    {
        invoke_application_callback = true;
    }

    /* Scan is completed, reset the flag */
    scan_handler.is_scanning = false;

    if(mac_addr_arr != NULL)
    {
       free(mac_addr_arr);
       mac_addr_arr = NULL;
    }

    /* Notify application appropriately based on scan status */
    if((scan_handler.p_scan_calback != NULL) && (invoke_application_callback == true))
    {
       scan_handler.p_scan_calback(NULL, scan_handler.user_data, CY_WCM_SCAN_COMPLETE);
    }

    if(scan_handler.is_stop_scan_req)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "setting stop scan semaphore %s %d\r\n", __FILE__, __LINE__);
        if(cy_rtos_set_semaphore(&stop_scan_semaphore, false) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "unable to set the stop scan semaphore \n");
        }
        scan_handler.is_stop_scan_req = false;
    }

    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to release the WCM mutex \n");
        if(scan_res.ie_ptr != NULL)
        {
            free(scan_res.ie_ptr);
        }
        return;
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);
}


static void process_scan_data(void *arg)
{
    whd_scan_result_t *whd_scan_res = (whd_scan_result_t*)arg;
    cy_wcm_scan_result_t wcm_scan_res;
    whd_mac_t *mac_iter = NULL;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        free(whd_scan_res);
        return;
    }

    if (scan_handler.is_scanning == false)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "scan_handler.is_scanning is false\n");
        goto exit;
    }

    /* Check for duplicate SSID */
    for (mac_iter = mac_addr_arr; (mac_iter < mac_addr_arr + current_bssid_arr_length); ++mac_iter)
    {
        if(CMP_MAC(mac_iter->octet, whd_scan_res->BSSID.octet))
        {
            /* The scanned result is a duplicate; just return */
            goto exit;
        }
    }

    /* If scanned Wi-Fi is not a duplicate then populate the array */
    if(current_bssid_arr_length < SCAN_BSSID_ARR_LENGTH)
    {
        memcpy(&mac_iter->octet, whd_scan_res->BSSID.octet, sizeof(whd_mac_t) );
        current_bssid_arr_length++;
    }

    memset(&wcm_scan_res, 0, sizeof(wcm_scan_res));

    if(scan_handler.scan_filter.mode == CY_WCM_SCAN_FILTER_TYPE_RSSI)
    {
         int16_t requested_range = scan_handler.scan_filter.param.rssi_range;
         int16_t signal_strength = whd_scan_res->signal_strength;
         if(signal_strength <= requested_range)
         {
             goto exit;
         }
    }

    /** copy all the parameters to scan_res **/
    memcpy(wcm_scan_res.SSID, whd_scan_res->SSID.value, whd_scan_res->SSID.length);
    memcpy(wcm_scan_res.BSSID, whd_scan_res->BSSID.octet, sizeof(wcm_scan_res.BSSID));
    wcm_scan_res.security = whd_to_wcm_security(whd_scan_res->security);
    wcm_scan_res.band = whd_to_wcm_band(whd_scan_res->band);
    wcm_scan_res.bss_type = whd_to_wcm_bss_type(whd_scan_res->bss_type);
    memcpy(wcm_scan_res.ccode, whd_scan_res->ccode, sizeof(wcm_scan_res.ccode));
    wcm_scan_res.channel = whd_scan_res->channel;
    wcm_scan_res.signal_strength = whd_scan_res->signal_strength;
    wcm_scan_res.flags = whd_scan_res->flags;
    wcm_scan_res.ie_len = whd_scan_res->ie_len;
    wcm_scan_res.ie_ptr = (uint8_t*)malloc(wcm_scan_res.ie_len);
    if(wcm_scan_res.ie_ptr == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Heap space error \n");
        goto exit;
    }
    else
    {
        memcpy(wcm_scan_res.ie_ptr, whd_scan_res->ie_ptr, wcm_scan_res.ie_len);
    }

    /* Notify application appropriately based on scan status */
    if(scan_handler.p_scan_calback != NULL)
    {
        scan_handler.p_scan_calback(&wcm_scan_res, scan_handler.user_data, CY_WCM_SCAN_INCOMPLETE);
    }
    free(wcm_scan_res.ie_ptr);

exit:
    free(whd_scan_res);

    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to release the WCM mutex \n");
        return;
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);
}

static void invoke_app_callbacks(cy_wcm_event_t event_type, cy_wcm_event_data_t* arg)
{
    int i = 0;

    for ( i = 0; i < CY_WCM_MAXIMUM_CALLBACKS_COUNT; i++ )
    {
        if ( wcm_event_handler[i] != NULL )
        {
            wcm_event_handler[i](event_type, arg);
        }
    }
}

static void ap_callback_handler(void* arg)
{
    cy_wcm_event_data_t link_event_data;
    wcm_ap_link_event *event_data = (wcm_ap_link_event*)arg;

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Notify application about AP events \n");
    memset(&link_event_data, 0, sizeof(cy_wcm_event_data_t));
    memcpy(link_event_data.sta_mac, event_data->mac_addr, CY_WCM_MAC_ADDR_LEN);
    invoke_app_callbacks(event_data->event, &link_event_data);
    free(arg);
}

static void* ap_link_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header, const uint8_t *event_data, void *handler_user_data)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    UNUSED_PARAMETER(res);
    wcm_ap_link_event *ap_event_data;

    ap_event_data = (wcm_ap_link_event*)malloc(sizeof(wcm_ap_link_event));
    if(ap_event_data == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed %s %d\r\n", __FILE__, __LINE__);
        return handler_user_data;
    }

    if (event_header->event_type == WLC_E_DISASSOC_IND || event_header->event_type == WLC_E_DEAUTH_IND)
    {
#ifdef PROTO_MSGBUF
        whd_wifi_delete_peer(ifp, (uint8_t *)event_header->addr.octet);
#endif /* PROTO_MSGBUF */
        ap_event_data->event = CY_WCM_EVENT_STA_LEFT_SOFTAP;
    }
    else if (event_header->event_type == WLC_E_ASSOC_IND || event_header->event_type == WLC_E_REASSOC_IND)
    {
        /* Set ap_event_data->event only if the security type of AP is CY_WCM_SECURITY_OPEN/CY_WCM_SECURITY_WEP_PSK/CY_WCM_SECURITY_WEP_SHARED.
         * For all other security types wait for WLC_E_AUTHORIZED event to set ap_event_data->event to CY_WCM_EVENT_STA_JOINED_SOFTAP.
         */
        if(ap_security == CY_WCM_SECURITY_OPEN || ap_security == CY_WCM_SECURITY_WEP_PSK || ap_security == CY_WCM_SECURITY_WEP_SHARED)
        {
            ap_event_data->event = CY_WCM_EVENT_STA_JOINED_SOFTAP;
        }
        else
        {
            /* Ignore other security types */
            free(ap_event_data);
            return handler_user_data;
        }
    }
    else if(event_header->event_type == WLC_E_AUTHORIZED)
    {
        /* If the security type of AP is other than CY_WCM_SECURITY_OPEN, CY_WCM_SECURITY_WEP_PSK, CY_WCM_SECURITY_WEP_SHARED and
         * WLC_E_AUTHORIZED event is received, then set ap_event_data->event to CY_WCM_EVENT_STA_JOINED_SOFTAP.
         */
        if(ap_security != CY_WCM_SECURITY_OPEN && ap_security != CY_WCM_SECURITY_WEP_PSK && ap_security != CY_WCM_SECURITY_WEP_SHARED)
        {
            ap_event_data->event = CY_WCM_EVENT_STA_JOINED_SOFTAP;
        }
        else
        {
            /* Ignore other security types */
            free(ap_event_data);
            return handler_user_data;
        }
    }
    else
    {
        /* Ignore other events */
        free(ap_event_data);
        return handler_user_data;
    }

    memcpy(ap_event_data->mac_addr, event_header->addr.octet, CY_WCM_MAC_ADDR_LEN);
    if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, ap_callback_handler, (void*)ap_event_data)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to handle link up event. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
        free(ap_event_data);
    }

    return handler_user_data;
}

static void* link_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header, const uint8_t *event_data, void *handler_user_data)
{
    uint32_t connection_status;
    cy_rslt_t res = CY_RSLT_SUCCESS;
    UNUSED_PARAMETER(res);
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Link event (type, status, reason, flags) %u %u %u %u\n", (unsigned int)event_header->event_type, (unsigned int)event_header->status,
        (unsigned int)event_header->reason, (unsigned int)event_header->flags);
    switch (event_header->event_type)
    {
        case WLC_E_LINK:
            if ((event_header->flags & WLC_EVENT_MSG_LINK) != 0)
            {
                switch (sta_security_type)
                {
                    case WHD_SECURITY_OPEN:
                    case WHD_SECURITY_IBSS_OPEN:
                    case WHD_SECURITY_WPS_SECURE:
                    case WHD_SECURITY_WEP_PSK:
                    case WHD_SECURITY_WEP_SHARED:
                    {
                        /* Advertise link-up immediately as no EAPOL is required */
                        link_up();
                        break;
                    }
                    case WHD_SECURITY_WPA_TKIP_PSK:
                    case WHD_SECURITY_WPA_AES_PSK:
                    case WHD_SECURITY_WPA_MIXED_PSK:
                    case WHD_SECURITY_WPA2_AES_PSK:
                    case WHD_SECURITY_WPA2_AES_PSK_SHA256:
                    case WHD_SECURITY_WPA2_TKIP_PSK:
                    case WHD_SECURITY_WPA2_MIXED_PSK:
                    case WHD_SECURITY_WPA2_WPA_AES_PSK:
                    case WHD_SECURITY_WPA2_WPA_MIXED_PSK:
                    case WHD_SECURITY_WPA_TKIP_ENT:
                    case WHD_SECURITY_WPA_AES_ENT:
                    case WHD_SECURITY_WPA_MIXED_ENT:
                    case WHD_SECURITY_WPA2_TKIP_ENT:
                    case WHD_SECURITY_WPA2_AES_ENT:
                    case WHD_SECURITY_WPA2_MIXED_ENT:
                    case WHD_SECURITY_WPA2_FBT_PSK:
                    case WHD_SECURITY_WPA2_FBT_ENT:
                    case WHD_SECURITY_WPA3_SAE:
                    case WHD_SECURITY_WPA3_WPA2_PSK:
#ifdef COMPONENT_WIFI6
                    case WHD_SECURITY_WPA3_OWE:
#endif
#ifdef ENABLE_ENTPS_FEATURE
                    case WHD_SECURITY_WPA3_192BIT_ENT:
                    case WHD_SECURITY_WPA3_ENT_AES_CCMP:
                    case WHD_SECURITY_WPA3_ENT:
#endif
                    {
                        link_up_event_received = WHD_TRUE;
                        /* Start a timer and wait for WLC_E_PSK_SUP event */
                        cy_rtos_start_timer(&sta_handshake_timer, WCM_HANDSHAKE_TIMEOUT_MS);
                        break;
                    }
                    case WHD_SECURITY_UNKNOWN:
                    case WHD_SECURITY_FORCE_32_BIT:
                    default:
                    {
                        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad Security type\r\n");
                        break;
                    }
                }
                /* Notify application connecting */
                connection_status = CY_WCM_EVENT_CONNECTING;
                if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status)) != CY_RSLT_SUCCESS)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to send connection status. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
                }
            }
            else
            {
                /* Check if the link down event is followed by too many RSN IE Error. If yes, try join again */
                if (too_many_ie_error)
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Notify application that WCM will retry to connect to the AP!\n");
                    invoke_app_callbacks(CY_WCM_EVENT_INITIATED_RETRY, NULL);
                    /* Try to join the AP again */
                    handshake_timeout_handler(0);

                    /* Clear the error flag */
                    too_many_ie_error = WHD_FALSE;
                }
                /* Check if the beacon is lost */
                else if (event_header->reason == WLC_E_LINK_BCN_LOSS)
                {
                    link_down(event_header->reason);
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Beacon Lost notify application that WCM will retry to connect to the AP!\n");
                    invoke_app_callbacks(CY_WCM_EVENT_INITIATED_RETRY, NULL);

                    /* Try to join the AP again */
                    handshake_timeout_handler(0);
                }
                else
                {
                    /* WPA handshake is aborted because of link down. Stop handshake timer. */

                    /* Stop the handshake timer */
                    cy_rtos_stop_timer(&sta_handshake_timer);

                    /* Stop the retry timer */
                    cy_rtos_stop_timer(&sta_retry_timer);
                    retry_backoff_timeout = DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS;

                    link_down(event_header->reason);
                }
            }
            break;

        case WLC_E_DEAUTH_IND:
        case WLC_E_DISASSOC_IND:
            link_down(event_header->reason);
            break;

        case WLC_E_PSK_SUP:
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "recieved WLC_E_PSK_SUP \n");
                /* WLC_E_PSK_SUP is received. Check for status and reason. */
                if ( event_header->status == WLC_SUP_KEYED && event_header->reason == WLC_E_SUP_OTHER && link_up_event_received == WHD_TRUE )
                {
                    /* Successful WPA key exchange. Stop timer and announce link up event to application */

                    /* Stop handshake Timer */
                    cy_rtos_stop_timer(&sta_handshake_timer);

                    /* Stop retry Timer */
                    cy_rtos_stop_timer(&sta_retry_timer);

                    retry_backoff_timeout = DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS;

                    link_up();
                    link_up_event_received = WHD_FALSE;
                }
                else if ( event_header->reason == WLC_E_SUP_MSG3_TOO_MANY_IE )
                {
                    /* Wi-Fi firmware will do disassoc internally and will not retry to join the AP.
                     * Set a flag to indicate the join should be retried (from the host side).
                     */
                    too_many_ie_error = WHD_TRUE;
                }
                break;
            }
        case WLC_E_CSA_COMPLETE_IND:
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "recieved WLC_E_CSA_COMPLETE_IND \n");
                handshake_timeout_handler(0);
                break;
            }
        /* Note - These are listed to keep gcc pedantic checking happy */
        case WLC_E_NONE:
        case WLC_E_ROAM:
        case WLC_E_SET_SSID:
        case WLC_E_DEAUTH:
        case WLC_E_DISASSOC:
        case WLC_E_JOIN:
        case WLC_E_START:
        case WLC_E_AUTH:
        case WLC_E_AUTH_IND:
        case WLC_E_ASSOC:
        case WLC_E_ASSOC_IND:
        case WLC_E_REASSOC:
        case WLC_E_REASSOC_IND:
        case WLC_E_QUIET_START:
        case WLC_E_QUIET_END:
        case WLC_E_BEACON_RX:
        case WLC_E_MIC_ERROR:
        case WLC_E_NDIS_LINK:
        case WLC_E_TXFAIL:
        case WLC_E_PMKID_CACHE:
        case WLC_E_RETROGRADE_TSF:
        case WLC_E_PRUNE:
        case WLC_E_AUTOAUTH:
        case WLC_E_EAPOL_MSG:
        case WLC_E_SCAN_COMPLETE:
        case WLC_E_ADDTS_IND:
        case WLC_E_DELTS_IND:
        case WLC_E_BCNSENT_IND:
        case WLC_E_BCNRX_MSG:
        case WLC_E_BCNLOST_MSG:
        case WLC_E_ROAM_PREP:
        case WLC_E_PFN_NET_FOUND:
        case WLC_E_PFN_NET_LOST:
        case WLC_E_RESET_COMPLETE:
        case WLC_E_JOIN_START:
        case WLC_E_ROAM_START:
        case WLC_E_ASSOC_START:
        case WLC_E_IBSS_ASSOC:
        case WLC_E_RADIO:
        case WLC_E_PSM_WATCHDOG:
        case WLC_E_CCX_ASSOC_START:
        case WLC_E_CCX_ASSOC_ABORT:
        case WLC_E_PROBREQ_MSG:
        case WLC_E_SCAN_CONFIRM_IND:
        case WLC_E_COUNTRY_CODE_CHANGED:
        case WLC_E_EXCEEDED_MEDIUM_TIME:
        case WLC_E_ICV_ERROR:
        case WLC_E_UNICAST_DECODE_ERROR:
        case WLC_E_MULTICAST_DECODE_ERROR:
        case WLC_E_TRACE:
        case WLC_E_BTA_HCI_EVENT:
        case WLC_E_IF:
        case WLC_E_P2P_DISC_LISTEN_COMPLETE:
        case WLC_E_RSSI:
        case WLC_E_PFN_SCAN_COMPLETE:
        case WLC_E_EXTLOG_MSG:
        case WLC_E_ACTION_FRAME:
        case WLC_E_ACTION_FRAME_COMPLETE:
        case WLC_E_PRE_ASSOC_IND:
        case WLC_E_PRE_REASSOC_IND:
        case WLC_E_CHANNEL_ADOPTED:
        case WLC_E_AP_STARTED:
        case WLC_E_DFS_AP_STOP:
        case WLC_E_DFS_AP_RESUME:
        case WLC_E_WAI_STA_EVENT:
        case WLC_E_WAI_MSG:
        case WLC_E_ESCAN_RESULT:
        case WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE:
        case WLC_E_PROBRESP_MSG:
        case WLC_E_P2P_PROBREQ_MSG:
        case WLC_E_DCS_REQUEST:
        case WLC_E_FIFO_CREDIT_MAP:
        case WLC_E_ACTION_FRAME_RX:
        case WLC_E_WAKE_EVENT:
        case WLC_E_RM_COMPLETE:
        case WLC_E_HTSFSYNC:
        case WLC_E_OVERLAY_REQ:
        case WLC_E_EXCESS_PM_WAKE_EVENT:
        case WLC_E_PFN_SCAN_NONE:
        case WLC_E_PFN_SCAN_ALLGONE:
        case WLC_E_GTK_PLUMBED:
        case WLC_E_ASSOC_IND_NDIS:
        case WLC_E_REASSOC_IND_NDIS:
        case WLC_E_ASSOC_REQ_IE:
        case WLC_E_ASSOC_RESP_IE:
        case WLC_E_ASSOC_RECREATED:
        case WLC_E_ACTION_FRAME_RX_NDIS:
        case WLC_E_AUTH_REQ:
        case WLC_E_TDLS_PEER_EVENT:
        case WLC_E_SPEEDY_RECREATE_FAIL:
        case WLC_E_NATIVE:
        case WLC_E_PKTDELAY_IND:
        case WLC_E_AWDL_AW:
        case WLC_E_AWDL_ROLE:
        case WLC_E_AWDL_EVENT:
        case WLC_E_NIC_AF_TXS:
        case WLC_E_NAN:
        case WLC_E_BEACON_FRAME_RX:
        case WLC_E_SERVICE_FOUND:
        case WLC_E_GAS_FRAGMENT_RX:
        case WLC_E_GAS_COMPLETE:
        case WLC_E_P2PO_ADD_DEVICE:
        case WLC_E_P2PO_DEL_DEVICE:
        case WLC_E_WNM_STA_SLEEP:
        case WLC_E_TXFAIL_THRESH:
        case WLC_E_PROXD:
        case WLC_E_IBSS_COALESCE:
        case WLC_E_AWDL_RX_PRB_RESP:
        case WLC_E_AWDL_RX_ACT_FRAME:
        case WLC_E_AWDL_WOWL_NULLPKT:
        case WLC_E_AWDL_PHYCAL_STATUS:
        case WLC_E_AWDL_OOB_AF_STATUS:
        case WLC_E_AWDL_SCAN_STATUS:
        case WLC_E_AWDL_AW_START:
        case WLC_E_AWDL_AW_END:
        case WLC_E_AWDL_AW_EXT:
        case WLC_E_AWDL_PEER_CACHE_CONTROL:
        case WLC_E_CSA_START_IND:
        case WLC_E_CSA_DONE_IND:
        case WLC_E_CSA_FAILURE_IND:
        case WLC_E_CCA_CHAN_QUAL:
        case WLC_E_BSSID:
        case WLC_E_TX_STAT_ERROR:
        case WLC_E_BCMC_CREDIT_SUPPORT:
        case WLC_E_PSTA_PRIMARY_INTF_IND:
        case WLC_E_RRM:
        case WLC_E_ULP:
        case WLC_E_LAST:
        case WLC_E_BT_WIFI_HANDOVER_REQ:
        case WLC_E_PFN_BEST_BATCHING:
        case WLC_E_SPW_TXINHIBIT:
        case WLC_E_FBT_AUTH_REQ_IND:
        case WLC_E_RSSI_LQM:
        case WLC_E_PFN_GSCAN_FULL_RESULT:
        case WLC_E_PFN_SWC:
        case WLC_E_AUTHORIZED:
        case WLC_E_PROBREQ_MSG_RX:
        case WLC_E_RMC_EVENT:
        case WLC_E_DPSTA_INTF_IND:
        default:
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Received event which was not registered\n");
            break;
    }
    return handler_user_data;
}

static void link_up( void )
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    UNUSED_PARAMETER(res);
    if(!wcm_sta_link_up)
    {
        if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, sta_link_up_handler, NULL)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to handle link up event. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
            return;
        }
        wcm_sta_link_up = true;
    }
    else
    {
        /* Do not renew DHCP if the link was connected through static IP */
        if(connected_ap_details.static_ip.addr.ip.v4 != 0)
        {
            return;
        }
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Executing DHCP renewal \n");
        /* This condition will be hit when handshake fails and wcm reconnection is successful through retry timer*/
        if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, sta_link_up_renew_handler, NULL)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to handle link up event. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
            return;
        }
    }
}

static void sta_link_up_handler(void* arg)
{
    UNUSED_PARAMETER(arg);
    cy_rslt_t res = CY_RSLT_SUCCESS;

    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Link UP: Unable to acquire WCM mutex \n");
        return;
    }
    res = cy_network_ip_up(nw_sta_if_ctx);
    if(res == CY_RSLT_SUCCESS)
    {
        is_sta_network_up = true;
    }
    if(cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Link UP: Unable to Release WCM mutex \n");
    }
    if(res == CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Notify application that network is connected again!\n");
        invoke_app_callbacks(CY_WCM_EVENT_RECONNECTED, NULL);
    }
}

static void notify_connection_status(void* arg)
{
    uint32_t val = (uint32_t)arg;
    cy_wcm_event_t connect_status = (cy_wcm_event_t)val;
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Notify connection status = %d!\n", connect_status);
    invoke_app_callbacks(connect_status, NULL);
}

static void sta_link_up_renew_handler(void* arg)
{
    UNUSED_PARAMETER(arg);
    cy_network_dhcp_renew(nw_sta_if_ctx);

}

static void link_down(uint32_t reason)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    UNUSED_PARAMETER(res);
    if (wcm_sta_link_up)
    {
        if((res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, sta_link_down_handler, (void *)reason)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : ERROR : Failed to handle link down event. Err = [%lu]\r\n", __LINE__, __FUNCTION__, res);
        }
        wcm_sta_link_up = false;
    }
}

static void sta_link_down_handler(void* arg)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_wcm_event_data_t event_data;

    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Link Down: Unable to acquire WCM mutex \n");
        return;
    }
    res = cy_network_ip_down(nw_sta_if_ctx);
    if(res == CY_RSLT_SUCCESS)
    {
        is_sta_network_up = false;
    }
    if(cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Link Down: Unable to Release WCM mutex \n");
    }
    if(res == CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Notify application that network is down!\n");
        /* Store the reason for disconnection as received from WHD */
        event_data.reason = (cy_wcm_reason_code)arg;
        invoke_app_callbacks(CY_WCM_EVENT_DISCONNECTED, &event_data);
    }
}
static void hanshake_retry_timer(cy_timer_callback_arg_t arg)
{
    UNUSED_PARAMETER(arg);
#ifdef COMPONENT_CAT5
    cy_rslt_t result;
    result = cy_worker_thread_enqueue(&cy_wcm_worker_thread, handshake_error_callback, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : Failed to send async event to n/w worker thread. Err = [%lu]\r\n", __LINE__, __FUNCTION__, result);
    }
#else
    handshake_error_callback(0);
#endif
}

static void handshake_timeout_handler(cy_timer_callback_arg_t arg)
{
    cy_rslt_t result;

    UNUSED_PARAMETER( arg );

    cy_rtos_stop_timer(&sta_handshake_timer);
    cy_rtos_stop_timer(&sta_retry_timer);
    retry_backoff_timeout = DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS;

    result = cy_worker_thread_enqueue(&cy_wcm_worker_thread, handshake_error_callback, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : Failed to send async event to n/w worker thread. Err = [%lu]\r\n", __LINE__, __FUNCTION__, result);
    }
}


static void handshake_error_callback(void *arg)
{
    cy_rslt_t  res;
    uint8_t    retries;
    uint32_t   ext_sae_support = 0;
    uint32_t   connection_status;

    UNUSED_PARAMETER(arg);

    /* stop the retry timer */
    cy_rtos_stop_timer(&sta_retry_timer);

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return;
    }
    if (is_disconnect_triggered == true || is_connect_triggered == true || cy_wcm_is_connected_to_ap())
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO,
        "is_disconnect_triggered(%d),cy_wcm_is_connected_to_ap()(%d) exit handshake_error_callback\n",
        is_disconnect_triggered, cy_wcm_is_connected_to_ap());
        goto exit;
    }
    /* De-register the link event handler */
    whd_wifi_deregister_event_handler(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], sta_event_handler_index);

    /* Explicitly leave AP and then rejoin */
    whd_wifi_leave(whd_ifs[CY_WCM_INTERFACE_TYPE_STA]);
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Mutex release error \n");
        return;
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    cy_rtos_delay_milliseconds(100);

    for(retries = 0; retries < JOIN_RETRY_ATTEMPTS; retries++)
    {
        cy_rslt_t join_result;
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
        if(cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
            return;
        }

        whd_wifi_get_fwcap(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &ext_sae_support);
        if (( ext_sae_support & (1 << WHD_FWCAP_SAE_EXT)) &&
           ((connected_ap_details.security == WHD_SECURITY_WPA3_SAE) || (connected_ap_details.security == WHD_SECURITY_WPA3_WPA2_PSK)))
        {
             cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "calling wpa3_supplicant_sae_start\n");
             /* supplicant SAE Start */
             res = wpa3_supplicant_sae_start(connected_ap_details.SSID.value, connected_ap_details.SSID.length, connected_ap_details.key, connected_ap_details.keylen);
             if ( res != CY_RSLT_SUCCESS)
             {
                 res = CY_RSLT_WCM_WPA3_SUPPLICANT_ERROR;
                 goto exit;
             }
             cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wpa3_supplicant_sae_start returned res=%d\n", res);
        }

        if (is_disconnect_triggered == true || is_connect_triggered == true || cy_wcm_is_connected_to_ap())
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO,
            "is_disconnect_triggered(%d),cy_wcm_is_connected_to_ap()(%d) exit handshake_error_callback\n",
            is_disconnect_triggered, cy_wcm_is_connected_to_ap());
            goto exit;
        }

        if(!NULL_MAC(connected_ap_details.sta_mac.octet))
        {
            whd_scan_result_t ap;
            ap.security = connected_ap_details.security;
            ap.SSID.length = connected_ap_details.SSID.length;
            memcpy(ap.SSID.value, connected_ap_details.SSID.value, ap.SSID.length);
            memcpy(ap.BSSID.octet, connected_ap_details.sta_mac.octet, CY_WCM_MAC_ADDR_LEN);
            /*
             * If MAC address of a AP is know there is no need to populate channel or band
             * instead we can set the band to auto and invoke whd join
             */
            whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_AUTO);
            join_result = whd_wifi_join_specific(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &ap, connected_ap_details.key, connected_ap_details.keylen);
        }
        else
        {
            if(connected_ap_details.band == CY_WCM_WIFI_BAND_5GHZ)
            {
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_5G);
            }
            else if(connected_ap_details.band == CY_WCM_WIFI_BAND_2_4GHZ)
            {
                /*
                 * It could be possible on a dual band supported device,
                 * the current band set is 5G and the requested band from user is 2.4Ghz
                 */
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_2G);
            }
            else if(connected_ap_details.band == CY_WCM_WIFI_BAND_6GHZ)
            {
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_6G);
            }
            else
            {
                whd_wifi_set_ioctl_value(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], WLC_SET_BAND, WLC_BAND_AUTO);
            }
            /** Join to Wi-Fi AP **/
            join_result = whd_wifi_join(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &connected_ap_details.SSID, connected_ap_details.security, connected_ap_details.key, connected_ap_details.keylen);
        }

        if(join_result == CY_RSLT_SUCCESS)
        {
            link_up();
            sta_security_type = connected_ap_details.security;
            /* Register for Link events*/
            if(whd_management_set_event_handler(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], sta_link_events, link_events_handler, NULL, &sta_event_handler_index) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "sta link event registration failed \n");
            }
            /* Reset retry-backoff-timeout */
            retry_backoff_timeout = DEFAULT_RETRY_BACKOFF_TIMEOUT_IN_MS;
            goto exit;
        }
        else
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "whd_wifi_join failed : %ld \n", join_result);
            connection_status = CY_WCM_EVENT_CONNECT_FAILED;
            res = cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_connection_status, (void *)connection_status);
            if(res != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to send connection status. Err = [%lu]\r\n", res);
            }
        }

        if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Mutex release error \n");
            return;
        }
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);
    }

    /* Register retry with network worker thread */
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "L%d : %s() : Retrying to join with back-off [%ld milliseconds]\r\n", __LINE__, __FUNCTION__, retry_backoff_timeout);
    res = cy_rtos_start_timer(&sta_retry_timer, retry_backoff_timeout);
    if (res != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "L%d : %s() : ERROR : Failed in join retry with back-off. Err = [%ld]\r\n", __LINE__, __FUNCTION__, res);
    }
    /* Update backoff timeout */
    retry_backoff_timeout = (retry_backoff_timeout < MAX_RETRY_BACKOFF_TIMEOUT_IN_MS)? (uint32_t)(retry_backoff_timeout * 2) : MAX_RETRY_BACKOFF_TIMEOUT_IN_MS;
    return;

exit:
    if (cy_rtos_set_mutex(&wcm_mutex) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Mutex release error \n");
        return;
    }
    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex unlocked %s %d\r\n", __FILE__, __LINE__);
}

static void lwip_ip_change_callback(cy_network_interface_context *iface_context, void *arg)
{
    UNUSED_PARAMETER(arg);
    cy_rslt_t result;
    if(!wcm_sta_link_up)
    {
        return;
    }
    result = cy_worker_thread_enqueue(&cy_wcm_worker_thread, notify_ip_change, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "L%d : %s() : Failed to send async event to n/w worker thread. Err = [%lu]\r\n", __LINE__, __FUNCTION__, result);
    }
}
void notify_ip_change(void *arg)
{
    cy_wcm_event_data_t link_event_data;
    cy_rslt_t res = CY_RSLT_SUCCESS;
    cy_nw_ip_address_t ipv4_addr;
    cy_nw_ip_address_t ipv6_addr;
    uint32_t offld_cfg_support = 0;
    UNUSED_PARAMETER(arg);

    whd_wifi_get_fwcap(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], &offld_cfg_support);

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Notify application that ip has changed!\n");
    memset(&link_event_data, 0, sizeof(cy_wcm_event_data_t));
    res = cy_network_get_ip_address(nw_sta_if_ctx, &ipv4_addr);

    if (res == CY_RSLT_SUCCESS)
    {
        link_event_data.ip_addr.version = CY_WCM_IP_VER_V4;
        link_event_data.ip_addr.ip.v4   = ipv4_addr.ip.v4;
        invoke_app_callbacks(CY_WCM_EVENT_IP_CHANGED, &link_event_data);

	if (offld_cfg_support & (1 << WHD_FWCAP_OFFLOADS) )
	{
	   res = whd_wifi_offload_ipv4_update(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], OFFLOAD_FEATURE, ipv4_addr.ip.v4, WHD_TRUE);
	   if (res != CY_RSLT_SUCCESS )
	   {
	      cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to update ipv4 address\n");
	   }
	}
    }

    res = cy_network_get_ipv6_address(nw_sta_if_ctx, CY_NETWORK_IPV6_LINK_LOCAL, &ipv6_addr);
    if (res == CY_RSLT_SUCCESS)
    {
       if (offld_cfg_support & (1 << WHD_FWCAP_OFFLOADS) )
       {
          res = whd_wifi_offload_ipv6_update(whd_ifs[CY_WCM_INTERFACE_TYPE_STA], OFFLOAD_FEATURE, ipv6_addr.ip.v6, 0, WHD_TRUE);
          if (res != CY_RSLT_SUCCESS )
          {
	     cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to update ipv6 address\n");
          }
       }
    }
}


static bool check_if_platform_supports_band(whd_interface_t interface, cy_wcm_wifi_band_t requested_band)
{
    whd_band_list_t band_list;
    uint32_t res;

    res = whd_wifi_get_ioctl_buffer(interface, WLC_GET_BANDLIST, (uint8_t*)&band_list, sizeof(whd_band_list_t));
    if(res != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Function whd_wifi_get_ioctl_buffer failed at line %d with result %u \n ", __LINE__, res);
        return false;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "band_list.current_band = %ld, band_list.number_of_bands = %ld , requested band = %d \n ", band_list.current_band, band_list.number_of_bands, requested_band);

     /*
      * All CY chips which supports 5Ghz will also support 2.4Ghz and
      * for such platforms number of bands will be 2 or higher
      */
    if(band_list.number_of_bands == 1)
    {
        if(requested_band == CY_WCM_WIFI_BAND_2_4GHZ || requested_band == CY_WCM_WIFI_BAND_ANY)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if(band_list.number_of_bands == 2 || band_list.number_of_bands == 3)
    {
//        FW still have some issue for band_list so skip check first
//        if((requested_band == band_list.other_band[0]) || (requested_band == band_list.other_band[1]))
//        {
            return true;
//        }
    }
    else
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Unexpected value in band_list.number_of_bands !! \n ");
        return false;
    }
}

static cy_rslt_t network_up(whd_interface_t interface, cy_network_hw_interface_type_t iface_type, cy_network_static_ip_addr_t *static_ip_ptr)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;
    if(iface_type == CY_NETWORK_WIFI_STA_INTERFACE)
    {
        if(is_sta_interface_created == false)
        {
            res = cy_network_add_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0, whd_ifs[CY_WCM_INTERFACE_TYPE_STA], NULL, static_ip_ptr, &nw_sta_if_ctx);
            if (res != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to add the network interface \n");
                return res;
            }
            is_sta_interface_created = true;
        }
        if((res = cy_network_ip_up(nw_sta_if_ctx)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to bring up the network stack \n");
            if(cy_network_remove_nw_interface(nw_sta_if_ctx) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to remove the network interface \n");
            }
            is_sta_interface_created = false;
            return res;
        }
        is_sta_network_up = true;
    }
    else if(iface_type == CY_NETWORK_WIFI_AP_INTERFACE)
    {
        res = cy_network_add_nw_interface(CY_NETWORK_WIFI_AP_INTERFACE, 0, whd_ifs[CY_WCM_INTERFACE_TYPE_AP], NULL, static_ip_ptr, &nw_ap_if_ctx);
        if (res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to add the network interface \n");
            return res;
        }
        if((res = cy_network_ip_up(nw_ap_if_ctx)) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to bring up the network stack \n");
            if(cy_network_remove_nw_interface(nw_ap_if_ctx) != CY_RSLT_SUCCESS)
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to remove the network interface \n");
            }
            return res;
        }
        is_ap_network_up = true;
    }
    return res;
}

static void network_down(whd_interface_t interface, cy_network_hw_interface_type_t iface_type)
{
    if(iface_type == CY_NETWORK_WIFI_STA_INTERFACE)
    {
        cy_network_ip_down(nw_sta_if_ctx);
        is_sta_network_up = false;
        cy_network_remove_nw_interface(nw_sta_if_ctx);
        is_sta_interface_created = false;
    }
    else
    {
        cy_network_ip_down(nw_ap_if_ctx);
        cy_network_remove_nw_interface(nw_ap_if_ctx);
        is_ap_network_up = false;
    }
}

static whd_security_t wcm_to_whd_security(cy_wcm_security_t sec)
{
    switch(sec)
    {
        case CY_WCM_SECURITY_OPEN :
            return WHD_SECURITY_OPEN;

        case CY_WCM_SECURITY_WEP_PSK:
            return WHD_SECURITY_WEP_PSK;

        case CY_WCM_SECURITY_WEP_SHARED:
            return WHD_SECURITY_WEP_SHARED;

        case CY_WCM_SECURITY_WPA_TKIP_PSK:
            return WHD_SECURITY_WPA_TKIP_PSK;

        case CY_WCM_SECURITY_WPA_AES_PSK:
            return WHD_SECURITY_WPA_AES_PSK;

        case CY_WCM_SECURITY_WPA_MIXED_PSK:
            return WHD_SECURITY_WPA_MIXED_PSK;

        case CY_WCM_SECURITY_WPA2_AES_PSK:
            return WHD_SECURITY_WPA2_AES_PSK;

        case CY_WCM_SECURITY_WPA2_AES_PSK_SHA256:
            return WHD_SECURITY_WPA2_AES_PSK_SHA256;

        case CY_WCM_SECURITY_WPA2_TKIP_PSK:
            return WHD_SECURITY_WPA2_TKIP_PSK;

        case CY_WCM_SECURITY_WPA2_MIXED_PSK:
            return WHD_SECURITY_WPA2_MIXED_PSK;

        case CY_WCM_SECURITY_WPA2_FBT_PSK:
            return WHD_SECURITY_WPA2_FBT_PSK;

        case CY_WCM_SECURITY_WPA3_SAE:
            return WHD_SECURITY_WPA3_SAE;

        case CY_WCM_SECURITY_WPA2_WPA_AES_PSK:
            return WHD_SECURITY_WPA2_WPA_AES_PSK;

        case CY_WCM_SECURITY_WPA2_WPA_MIXED_PSK:
            return WHD_SECURITY_WPA2_WPA_MIXED_PSK;

        case CY_WCM_SECURITY_WPA3_WPA2_PSK:
            return WHD_SECURITY_WPA3_WPA2_PSK;

        case CY_WCM_SECURITY_WPA_TKIP_ENT:
            return WHD_SECURITY_WPA_TKIP_ENT;

        case CY_WCM_SECURITY_WPA_AES_ENT:
            return WHD_SECURITY_WPA_AES_ENT;

        case CY_WCM_SECURITY_WPA_MIXED_ENT:
            return WHD_SECURITY_WPA_MIXED_ENT;

        case CY_WCM_SECURITY_WPA2_TKIP_ENT:
            return WHD_SECURITY_WPA2_TKIP_ENT;

        case CY_WCM_SECURITY_WPA2_AES_ENT:
            return WHD_SECURITY_WPA2_AES_ENT;

#ifdef ENABLE_ENTPS_FEATURE
        case CY_WCM_SECURITY_WPA3_ENT:
            return WHD_SECURITY_WPA3_ENT;

        case CY_WCM_SECURITY_WPA3_ENT_AES_CCMP:
            return WHD_SECURITY_WPA3_ENT_AES_CCMP;

        case CY_WCM_SECURITY_WPA3_192BIT_ENT:
            return WHD_SECURITY_WPA3_192BIT_ENT;
#endif

        case CY_WCM_SECURITY_WPA2_MIXED_ENT:
            return WHD_SECURITY_WPA2_MIXED_ENT;

        case CY_WCM_SECURITY_WPA2_FBT_ENT:
            return WHD_SECURITY_WPA2_FBT_ENT;

        case CY_WCM_SECURITY_IBSS_OPEN:
            return WHD_SECURITY_IBSS_OPEN;

        case CY_WCM_SECURITY_WPS_SECURE:
            return WHD_SECURITY_WPS_SECURE;

#ifdef COMPONENT_WIFI6
        case CY_WCM_SECURITY_OWE:
            return WHD_SECURITY_WPA3_OWE;
#endif

        default:
            return WHD_SECURITY_UNKNOWN;
    }
}

cy_wcm_security_t whd_to_wcm_security(whd_security_t sec)
{
    switch(sec)
    {
        case WHD_SECURITY_OPEN:
            return CY_WCM_SECURITY_OPEN;

        case WHD_SECURITY_WEP_PSK:
            return CY_WCM_SECURITY_WEP_PSK;

        case WHD_SECURITY_WEP_SHARED:
            return CY_WCM_SECURITY_WEP_SHARED;

        case WHD_SECURITY_WPA_TKIP_PSK:
            return CY_WCM_SECURITY_WPA_TKIP_PSK;

        case WHD_SECURITY_WPA_AES_PSK:
            return CY_WCM_SECURITY_WPA_AES_PSK;

        case WHD_SECURITY_WPA_MIXED_PSK:
            return CY_WCM_SECURITY_WPA_MIXED_PSK;

        case WHD_SECURITY_WPA2_AES_PSK_SHA256:
            return CY_WCM_SECURITY_WPA2_AES_PSK_SHA256;

        case WHD_SECURITY_WPA2_AES_PSK:
            return CY_WCM_SECURITY_WPA2_AES_PSK;

        case WHD_SECURITY_WPA2_TKIP_PSK:
            return CY_WCM_SECURITY_WPA2_TKIP_PSK;

        case WHD_SECURITY_WPA2_MIXED_PSK:
            return CY_WCM_SECURITY_WPA2_MIXED_PSK;

        case WHD_SECURITY_WPA2_FBT_PSK:
            return CY_WCM_SECURITY_WPA2_FBT_PSK;

        case WHD_SECURITY_WPA3_SAE:
            return CY_WCM_SECURITY_WPA3_SAE;

        case WHD_SECURITY_WPA2_WPA_AES_PSK:
            return CY_WCM_SECURITY_WPA2_WPA_AES_PSK;

        case WHD_SECURITY_WPA2_WPA_MIXED_PSK:
            return CY_WCM_SECURITY_WPA2_WPA_MIXED_PSK;

        case WHD_SECURITY_WPA3_WPA2_PSK:
            return CY_WCM_SECURITY_WPA3_WPA2_PSK;

        case WHD_SECURITY_WPA_TKIP_ENT:
            return CY_WCM_SECURITY_WPA_TKIP_ENT;

        case WHD_SECURITY_WPA_AES_ENT:
            return CY_WCM_SECURITY_WPA_AES_ENT;

        case WHD_SECURITY_WPA_MIXED_ENT:
            return CY_WCM_SECURITY_WPA_MIXED_ENT;

        case WHD_SECURITY_WPA2_TKIP_ENT:
            return CY_WCM_SECURITY_WPA2_TKIP_ENT;

        case WHD_SECURITY_WPA2_AES_ENT:
            return CY_WCM_SECURITY_WPA2_AES_ENT;

#ifdef ENABLE_ENTPS_FEATURE
        case WHD_SECURITY_WPA3_192BIT_ENT:
            return CY_WCM_SECURITY_WPA3_192BIT_ENT;

        case WHD_SECURITY_WPA3_ENT_AES_CCMP:
            return CY_WCM_SECURITY_WPA3_ENT_AES_CCMP;

        case WHD_SECURITY_WPA3_ENT:
            return CY_WCM_SECURITY_WPA3_ENT;
#endif

        case WHD_SECURITY_WPA2_MIXED_ENT:
            return CY_WCM_SECURITY_WPA2_MIXED_ENT;

        case WHD_SECURITY_WPA2_FBT_ENT:
            return CY_WCM_SECURITY_WPA2_FBT_ENT;

        case WHD_SECURITY_IBSS_OPEN:
            return CY_WCM_SECURITY_IBSS_OPEN;

        case WHD_SECURITY_WPS_SECURE:
            return CY_WCM_SECURITY_WPS_SECURE;

#ifdef COMPONENT_WIFI6
        case WHD_SECURITY_WPA3_OWE:
            return CY_WCM_SECURITY_OWE;
#endif

        default:
            return CY_WCM_SECURITY_UNKNOWN;
    }
}

static cy_wcm_wifi_band_t whd_to_wcm_band(whd_802_11_band_t band)
{
    if(band == WHD_802_11_BAND_5GHZ)
    {
        return CY_WCM_WIFI_BAND_5GHZ;
    }
    else if(band == WHD_802_11_BAND_2_4GHZ)
    {
        return CY_WCM_WIFI_BAND_2_4GHZ;
    }
    else
    {
        return CY_WCM_WIFI_BAND_6GHZ;
    }
}

static cy_wcm_bss_type_t whd_to_wcm_bss_type(whd_bss_type_t bss_type)
{
    switch(bss_type)
    {
        case WHD_BSS_TYPE_INFRASTRUCTURE:
            return CY_WCM_BSS_TYPE_INFRASTRUCTURE;

        case WHD_BSS_TYPE_ADHOC:
            return CY_WCM_BSS_TYPE_ADHOC;

        case WHD_BSS_TYPE_ANY:
            return CY_WCM_BSS_TYPE_ANY;

        case WHD_BSS_TYPE_MESH:
            return CY_WCM_BSS_TYPE_MESH;

        default :
            return CY_WCM_BSS_TYPE_UNKNOWN;
    }
}

static void read_ap_config(const cy_wcm_ap_config_t *ap_config, whd_ssid_t *ssid,
                              uint8_t **key, uint8_t *keylen, whd_security_t *security, cy_network_static_ip_addr_t *static_ip_addr)
{
    ssid->length = (uint8_t)strlen((char*)ap_config->ap_credentials.SSID);
    memcpy(&ssid->value, ap_config->ap_credentials.SSID, ssid->length + 1);

    *keylen = strlen((char*)ap_config->ap_credentials.password);
    *key = (uint8_t*)ap_config->ap_credentials.password;
    *security = wcm_to_whd_security(ap_config->ap_credentials.security);

    memset(static_ip_addr, 0, sizeof(cy_network_static_ip_addr_t));
    if(ap_config->ip_settings.gateway.version == CY_WCM_IP_VER_V4)
    {
        static_ip_addr->gateway.ip.v4 = ap_config->ip_settings.gateway.ip.v4;
        static_ip_addr->addr.ip.v4    = ap_config->ip_settings.ip_address.ip.v4;
        static_ip_addr->netmask.ip.v4 = ap_config->ip_settings.netmask.ip.v4;
    }
}

static cy_rslt_t check_soft_ap_config(const cy_wcm_ap_config_t *ap_config_params)
{
    uint8_t ssid_len;
    uint8_t pwd_len;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    whd_interface_t prim_ifp;
    if(whd_ifs[CY_WCM_INTERFACE_TYPE_AP] == NULL)
    {
        return CY_RSLT_WCM_INTERFACE_NOT_UP;
    }
    /* In concurrent mode (AP+STA), use the primary interface(STA) to get the band-list and channel_list, as only the
     * primary interface is initialized at this point.
     * This is as per the recommendation provided by WHD.
     * Note: This does not affect the AP mode since the primary interface will be the AP interface itself.
     */
    prim_ifp = whd_get_primary_interface(whd_ifs[CY_WCM_INTERFACE_TYPE_AP]->whd_driver);

    if(ap_config_params == NULL)
    {
        return CY_RSLT_WCM_BAD_ARG;
    }

    ssid_len = (uint8_t)strlen((char*)ap_config_params->ap_credentials.SSID);
    pwd_len  = (uint8_t)strlen((char*)ap_config_params->ap_credentials.password);

    if(ap_config_params->ip_settings.ip_address.ip.v4 == 0)
    {
        return CY_RSLT_WCM_BAD_STATIC_IP;
    }

    if(ssid_len == 0 || ssid_len > CY_WCM_MAX_SSID_LEN )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "SSID length error\n");
        return CY_RSLT_WCM_BAD_SSID_LEN;
    }

    if(!check_wcm_security(ap_config_params->ap_credentials.security))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AP credentials security error\n");
        return CY_RSLT_WCM_SECURITY_NOT_SUPPORTED;
    }

#ifdef COMPONENT_WIFI6
    if(ap_config_params->ap_credentials.security == CY_WCM_SECURITY_OWE)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "OWE security not supported\n");
        return CY_RSLT_WCM_SECURITY_NOT_SUPPORTED;
    }
#endif

    if((ap_config_params->ap_credentials.security != CY_WCM_SECURITY_OPEN) &&
       (pwd_len < CY_WCM_MIN_PASSPHRASE_LEN || pwd_len > CY_WCM_MAX_PASSPHRASE_LEN))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AP credentials passphrase length error. Length must be between 8 and 63\n");
        return CY_RSLT_WCM_BAD_PASSPHRASE_LEN;
    }

    if(ap_config_params->ie_info != NULL)
    {
        if(ap_config_params->ie_info->data == NULL)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid IE \n");
            return CY_RSLT_WCM_INVALID_IE;
        }
    }

    /*
     * Validate the band and update the appropriate error code
     * channel number 1 to 11 is 2G
     * channel number 36 to 165 is 5G
     * channel number 1 to 233 is 6G
     */
    do
    {
        if(ap_config_params->band != CY_WCM_WIFI_BAND_6GHZ)
        {
            if((ap_config_params->channel > 0) && (ap_config_params->channel < 12))
            {
                /* All platforms by default supports 2.4Ghz so return success*/
                break;
            }
            else if((ap_config_params->channel > 35) && (ap_config_params->channel < 166))
            {
                if(!check_if_platform_supports_band(prim_ifp, CY_WCM_WIFI_BAND_5GHZ))
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "5GHz band not supported \n");
                    result = CY_RSLT_WCM_BAND_NOT_SUPPORTED;
                    break;
                }
            }
            else
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid channel. Channel not in expected range \n");
                result = CY_RSLT_WCM_AP_BAD_CHANNEL;
                break;
            }
        }
        else
        {
            /* For 6GHz band */
            if((ap_config_params->channel > 0) && (ap_config_params->channel < 234))
            {
                if(!check_if_platform_supports_band(prim_ifp, CY_WCM_WIFI_BAND_6GHZ))
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "6GHz band not supported \n");
                    result = CY_RSLT_WCM_BAND_NOT_SUPPORTED;
                    break;
                }
            }
        }
    }while(0);

    /*
     * Validate if the given channel is supported on the kit and update appropriate error codes.
     */
    if(result == CY_RSLT_SUCCESS)
    {
        whd_list_t *whd_channel_list;
        uint32_t channel_buf[ MAXCHANNEL+1 ];
        whd_channel_list = (whd_list_t *)(void *)channel_buf;
        uint8_t index;
        cy_rslt_t res;

        whd_channel_list->count = MAXCHANNEL;  /* Initialize to maximum channels that can be supported as defined in WHD */

        res = whd_wifi_get_channels(prim_ifp, whd_channel_list);
        if(res != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_get_channels failed \n");
            return CY_RSLT_WCM_BAD_ARG;
        }

        for(index = 0; index < whd_channel_list->count; index++)
        {
            if(ap_config_params->channel == *(whd_channel_list->element+index))
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Input channel is supported \n");
                break;
            }
        }
        if(index == whd_channel_list->count)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Channel \n");
            return CY_RSLT_WCM_AP_BAD_CHANNEL;
        }
    }

    return result;
}

static cy_rslt_t init_whd_wifi_interface(cy_wcm_interface_t iface_type)
{

    if(iface_type != CY_WCM_INTERFACE_TYPE_AP_STA)
    {
        if (cybsp_wifi_init_primary(&whd_ifs[iface_type]) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in initializing primary interface !!\n");
            return CY_RSLT_WCM_BSP_INIT_ERROR;
        }

    }
    else
    {
        /* In concurrent mode primary interface would be STA and secondary AP*/
        if (cybsp_wifi_init_primary(&whd_ifs[CY_WCM_INTERFACE_TYPE_STA]) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error in getting primary interface mac address !!\n");
            return CY_RSLT_WCM_BSP_INIT_ERROR;
        }

        /* SoftAP is the secondary interface in Concurrent mode*/
        if(cybsp_wifi_init_secondary(&whd_ifs[CY_WCM_INTERFACE_TYPE_AP], NULL) != CY_RSLT_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error Unable to bring up the secondary interface !!\n");
            return CY_RSLT_WCM_SECONDARY_INTERFACE_ERROR;
        }
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_wcm_get_whd_interface(cy_wcm_interface_t interface_type, whd_interface_t *whd_iface)
{
    if(whd_iface == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : bad arguments \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    if(!is_wcm_initalized)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized. Call cy_wcm_init() to initialize \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    if((interface_type != CY_WCM_INTERFACE_TYPE_STA) || (current_interface == CY_WCM_INTERFACE_TYPE_AP))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Supported only for STA interface !!\n");
        return CY_RSLT_WCM_INTERFACE_NOT_SUPPORTED;
    }

    *whd_iface = whd_ifs[interface_type];
    return CY_RSLT_SUCCESS;
}

static bool check_if_ent_auth_types(cy_wcm_security_t auth_type)
{
    if((auth_type == CY_WCM_SECURITY_WPA_TKIP_ENT) || (auth_type == CY_WCM_SECURITY_WPA_AES_ENT) ||
       (auth_type == CY_WCM_SECURITY_WPA_MIXED_ENT) || (auth_type == CY_WCM_SECURITY_WPA2_TKIP_ENT) ||
       (auth_type == CY_WCM_SECURITY_WPA2_AES_ENT) || (auth_type == CY_WCM_SECURITY_WPA2_MIXED_ENT) ||
       (auth_type == CY_WCM_SECURITY_WPA2_FBT_ENT)
#ifdef ENABLE_ENTPS_FEATURE
       || (auth_type == CY_WCM_SECURITY_WPA3_ENT)
       || (auth_type == CY_WCM_SECURITY_WPA3_192BIT_ENT)
       || (auth_type == CY_WCM_SECURITY_WPA3_ENT_AES_CCMP)
#endif
       )
    {
        return true;
    }

    return false;
}

cy_rslt_t cy_wcm_set_ap_ip_setting(cy_wcm_ip_setting_t *ap_ip, const char *ip_addr, const char *netmask, const char *gateway_addr, cy_wcm_ip_version_t ver)
{
    cy_rslt_t          res = CY_RSLT_SUCCESS;
    cy_nw_ip_address_t temp;

    if(ap_ip == NULL || ip_addr == NULL || netmask == NULL || gateway_addr == NULL)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Null value passed to cy_wcm_ap_ip_setting \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    if((ver != CY_WCM_IP_VER_V4) && (ver != CY_WCM_IP_VER_V6))
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Invalid version passed to cy_wcm_set_ap_ip_setting \n");
        return CY_RSLT_WCM_BAD_ARG;
    }

    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "wcm mutex locked %s %d\r\n", __FILE__, __LINE__);
    if((res = cy_rtos_get_mutex(&wcm_mutex, CY_WCM_MAX_MUTEX_WAIT_TIME_MS)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire WCM mutex \n");
        return res;
    }

    cy_nw_aton(ip_addr,&temp);
    ap_ip->ip_address.ip.v4 =  temp.ip.v4;

    cy_nw_aton(netmask,&temp);
    ap_ip->netmask.ip.v4 =  temp.ip.v4;

    cy_nw_aton(gateway_addr,&temp);
    ap_ip->gateway.ip.v4 =  temp.ip.v4;

    ap_ip->ip_address.version = ver;
    ap_ip->netmask.version = ver;
    ap_ip->gateway.version = ver;

    if ((res = cy_rtos_set_mutex(&wcm_mutex)) != CY_RSLT_SUCCESS)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to release WCM mutex \n");
    }
    return res;
}

cy_rslt_t cy_wcm_allow_low_power_mode(cy_wcm_powersave_mode_t mode)
{
    cy_rslt_t       rslt = CY_RSLT_SUCCESS;
    whd_interface_t ifp = NULL;
    uint16_t        wlan_chip_id = 0;

    if (cy_wcm_get_whd_interface(CY_WCM_INTERFACE_TYPE_STA, &ifp) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    /* get chip ID */
    wlan_chip_id = whd_chip_get_chip_id(ifp->whd_driver);

    /* Disable WLAN PM/mpc for 43907 low power issue */
    if ( (wlan_chip_id == CY_WCM_WLAN_CHIP_ID_43909) || (wlan_chip_id == CY_WCM_WLAN_CHIP_ID_43907) || (wlan_chip_id == CY_WCM_WLAN_CHIP_ID_54907) )
    {
        switch (mode)
        {
            case CY_WCM_NO_POWERSAVE_MODE:
                rslt = whd_wifi_disable_powersave(ifp);
                if (rslt == WHD_SUCCESS)
                {
                    rslt = whd_wifi_set_iovar_value(ifp, IOVAR_STR_MPC, 0);
                }
                break;
            case CY_WCM_PM1_POWERSAVE_MODE:    /*  Powersave mode on specified interface without regard for throughput reduction */
                rslt = whd_wifi_enable_powersave(ifp);
                if (rslt == WHD_SUCCESS)
                {
                    rslt = whd_wifi_set_iovar_value(ifp, IOVAR_STR_MPC, 1);
                }
                break;
            case CY_WCM_PM2_POWERSAVE_MODE:    /* Powersave mode on specified interface with High throughput */
                rslt = whd_wifi_enable_powersave_with_throughput(ifp, DEFAULT_PM2_SLEEP_RET_TIME);
                if (rslt == WHD_SUCCESS)
                {
                    rslt = whd_wifi_set_iovar_value(ifp, IOVAR_STR_MPC, 1);
                }
                break;
            default:
                rslt = CY_RSLT_WCM_BAD_ARG;
                break;
        }

        if (rslt != WHD_SUCCESS)
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to change powersave mode.\n");
        }
    }
    else
    {
        rslt = CY_RSLT_WCM_POWERSAVE_MODE_NOT_SUPPORTED;
    }
    return rslt;
}

#endif
