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
* @file cy_wcm_wps.c
* @brief Wi-Fi connection manager(WCM) WPS provides set of APIs that are useful to connect to
* AP using either PIN or PBC mode
*/

#include "cy_wcm.h"
#include "cy_wcm_log.h"
#include "cy_wcm_error.h"
#include "cybsp_wifi.h"
#include "cyabs_rtos.h"
#include "whd.h"
#include "whd_wifi_api.h"
#include "whd_network_types.h"
#include "whd_buffer_api.h"

#include "cy_wps_memory.h"
#include "cy_wcm.h"
#include "whd_types.h"
#include "cyabs_rtos_impl.h"
#include "whd_int.h"
#include "cy_wcm_error.h"
#include "cy_wps.h"
#include "cy_wps_common.h"
#include "cy_wps_structures.h"

#define MAX_INTERFACE            (2)
/******************************************************
 *             Structures
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/
/* The primary Wi-Fi driver  */
extern whd_interface_t whd_ifs[MAX_INTERFACE];
extern bool is_wcm_initalized;
/******************************************************
 *               Static Function Declarations
 ******************************************************/
static cy_rslt_t convert_result_type( cy_rslt_t wps_result );
static int       cy_wps_compute_pin_checksum      (unsigned long int PIN);
static cy_wps_mode_t wcm_wps_to_wps( cy_wcm_wps_mode_t mode );
/******************************************************
 *               Extern Functions
 ******************************************************/
extern cy_wcm_security_t whd_to_wcm_security(whd_security_t sec);
/******************************************************
 *               Function Definitions
 ******************************************************/
/* Convert internal WPS error to WCM errors */
static cy_rslt_t convert_result_type( cy_rslt_t wps_result )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    switch(wps_result)
    {
        case CY_RSLT_WPS_OUT_OF_HEAP_SPACE:
        case CY_RSLT_WPS_ERROR_WPS_STACK_MALLOC_FAIL:
        case CY_RSLT_WPS_ERROR_OUT_OF_MEMORY:
             result = CY_RSLT_WCM_OUT_OF_MEMORY;
             break;
        case CY_RSLT_WPS_PBC_OVERLAP:
             result = CY_RSLT_WCM_WPS_PBC_OVERLAP;
             break;
        case CY_RSLT_WPS_BADARG:
             result = CY_RSLT_WCM_BAD_ARG;
             break;
        case CY_RSLT_WPS_ERROR:
             result = CY_RSLT_WCM_WPS_FAILED;
             break;
        case CY_RSLT_WPS_ERROR_RECEIVED_WEP_CREDENTIALS:
             result = CY_RSLT_WCM_WPS_ERROR_RECEIVED_WEP_CREDENTIALS;
             break;
        default:
             result = wps_result;
             break;
    }

    return result;
}

cy_rslt_t cy_wcm_wps_generate_pin( char wps_pin_string[CY_WCM_WPS_PIN_LENGTH] )
{
    uint16_t  r[2];
    uint32_t  random = 0;
    int i,    checksum;
    size_t    output_length = 0;
    char      temp_string[CY_WCM_WPS_PIN_LENGTH] = "00000000";
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if( !is_wcm_initalized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized, to initialize call cy_wcm_init() \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    memcpy( wps_pin_string, temp_string, CY_WCM_WPS_PIN_LENGTH );

    /* Generate a random number between 1 and 9999999 */
    while ( random == 0 )
    {
        result = cy_host_random_bytes(r,  4, &output_length);
        if( result != CY_RSLT_SUCCESS )
        {
            return result;
        }
        random = (uint32_t)(r[0] * r[1]) % 9999999;
    }

    checksum = cy_wps_compute_pin_checksum( random ); /* Compute checksum which will become the eighth digit */

    i = 8;
    wps_pin_string[i] = '\0';
    i--;
    wps_pin_string[i] = checksum + '0';
    i--;

    do {       /* generate digits */
        wps_pin_string[i] = random % 10 + '0';   /* get next digit */
        i--;
    } while ((random /= 10) > 0);     /* delete it */

    return CY_RSLT_SUCCESS;
}

static int cy_wps_compute_pin_checksum(unsigned long int PIN)
{
    unsigned long int accum = 0;

    PIN *= 10;
    accum += 3 * ((PIN / 10000000) % 10);
    accum += 1 * ((PIN / 1000000) % 10);
    accum += 3 * ((PIN / 100000) % 10);
    accum += 1 * ((PIN / 10000) % 10);
    accum += 3 * ((PIN / 1000) % 10);
    accum += 1 * ((PIN / 100) % 10);
    accum += 3 * ((PIN / 10) % 10);

    int digit = (accum % 10);

    return (10 - digit) % 10;
}
static cy_wps_mode_t wcm_wps_to_wps(cy_wcm_wps_mode_t mode)
{
    return ((mode == CY_WCM_WPS_PBC_MODE) ? CY_WPS_PBC_MODE : CY_WPS_PIN_MODE);
}

cy_rslt_t cy_wcm_wps_enrollee(cy_wcm_wps_config_t* wps_config, const cy_wcm_wps_device_detail_t *details, cy_wcm_wps_credential_t *credentials, uint16_t *credential_count)
{
    cy_rslt_t result;
    cy_wps_agent_t *workspace;
    cy_wps_credential_t* wps_credentials;

    if( !is_wcm_initalized )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "WCM is not initialized, to initialize call cy_wcm_init() \n");
        return CY_RSLT_WCM_NOT_INITIALIZED;
    }

    workspace = (cy_wps_agent_t*) cy_wps_calloc("wps", 1, sizeof(cy_wps_agent_t));
    if ( workspace == NULL )
    {
        return CY_RSLT_WCM_OUT_OF_MEMORY;
    }

    wps_credentials = (cy_wps_credential_t*)cy_wps_calloc("wps_creds", (*credential_count), sizeof(cy_wps_credential_t));
    if(wps_credentials == NULL)
    {
    	free(workspace);
    	return CY_RSLT_WCM_OUT_OF_MEMORY;
    }

    if( cy_wcm_is_connected_to_ap() )
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Currently connected to an AP, disconnect from the current AP before trying to connect using WPS \n");
        free(wps_credentials);
        free(workspace);
        return CY_RSLT_WCM_WPS_FAILED;
    }
    
    result = cy_wps_init ( workspace, (cy_wps_device_detail_t*) details, CY_WPS_ENROLLEE_AGENT, whd_ifs[CY_WCM_INTERFACE_TYPE_STA] );
    if( result != CY_RSLT_SUCCESS )
    {
        goto convert_result_type;
    }

    result = cy_wps_start( workspace, wcm_wps_to_wps(wps_config->mode), wps_config->password, wps_credentials, credential_count );
    if( result != CY_RSLT_SUCCESS )
    {
        goto convert_result_type;
    }

    if( cy_wps_wait_till_complete( workspace ) != CY_RSLT_SUCCESS )
    {
        goto convert_result_type;
    }

    memset(credentials->ssid, 0, sizeof(credentials->ssid));
    memset(credentials->passphrase, 0, sizeof(credentials->passphrase));
    memcpy(credentials->ssid, wps_credentials->ssid, sizeof(wps_credentials->ssid));
    memcpy(credentials->passphrase, wps_credentials->passphrase, sizeof(wps_credentials->passphrase));
    credentials->security = whd_to_wcm_security(wps_credentials->security);
    result = cy_wps_get_result( workspace );

convert_result_type:
    if( result == CY_RSLT_SUCCESS )
    {
        *credential_count = cy_wps_get_stored_credential_count(workspace);
    }
    else
    {
        *credential_count = 0;
        if( whd_wifi_stop_scan(whd_ifs[CY_WCM_INTERFACE_TYPE_STA]) != WHD_SUCCESS )
        {
            cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR,"Failed to stop scan \r\n");
            /* Fall through to cleanup remaining resource */
        }
    }
    cy_wps_deinit( workspace );
    free(wps_credentials);
    free( workspace );
    wps_credentials = NULL;
    workspace = NULL;

    return convert_result_type( result );
}
