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
* @file cy_wps.h
* @brief Cypress WPS file for WPS initialization and handshake
*/
#pragma once

#include "cy_wps_structures.h"
#include "cy_chip_constants.h"
#include "whd_types.h"
#include "cyabs_rtos_impl.h"
#include "whd_wlioctl.h"
#include "whd_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/
#define DSSS_PARAMETER_SET_LENGTH (1)

#define HT_CAPABILITIES_IE_LENGTH (26)
#define HT_OPERATION_IE_LENGTH    (22)

#define RTOS_HIGHEST_PRIORITY            (CY_RTOS_PRIORITY_MAX)
#define RTOS_DEFAULT_THREAD_PRIORITY     (RTOS_HIGHEST_PRIORITY - 4)
#define RTOS_HIGHER_PRIORTIY_THAN(x)     (x < RTOS_HIGHEST_PRIORITY ? x+1 : RTOS_HIGHEST_PRIORITY)
#define RTOS_LOWER_PRIORTIY_THAN(x)      (x > RTOS_LOWEST_PRIORITY ? x-1 : RTOS_LOWEST_PRIORITY)



/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

extern cy_rslt_t cy_wps_init( cy_wps_agent_t* workspace, const cy_wps_device_detail_t* details, cy_wps_agent_type_t type, whd_interface_t interface );
extern cy_rslt_t cy_wps_get_result( cy_wps_agent_t* workspace );
extern cy_rslt_t cy_wps_deinit( cy_wps_agent_t* workspace );
extern cy_rslt_t cy_wps_start( cy_wps_agent_t* workspace, cy_wps_mode_t mode, const char* password, cy_wps_credential_t* credentials, uint16_t* credential_length );
extern cy_rslt_t cy_p2p_wps_start( cy_wps_agent_t* workspace );
extern cy_rslt_t cy_wps_restart( cy_wps_agent_t* workspace );
extern cy_rslt_t cy_wps_reset_registrar( cy_wps_agent_t* workspace, whd_mac_t* mac );
extern cy_rslt_t cy_wps_wait_till_complete( cy_wps_agent_t* workspace );
extern cy_rslt_t cy_wps_abort( cy_wps_agent_t* workspace );
extern cy_rslt_t cy_wps_management_set_event_handler( cy_wps_agent_t* workspace, bool enable );
extern cy_rslt_t cy_wps_set_directed_wps_target( cy_wps_agent_t* workspace, cy_wps_ap_t* ap, uint32_t maximum_join_attempts );


int              cy_wps_get_stored_credential_count( cy_wps_agent_t* workspace );
extern void      cy_wps_thread_main( cy_thread_arg_t arg );
extern cy_rslt_t cy_wps_internal_init( cy_wps_agent_t* workspace, uint32_t interface, cy_wps_mode_t mode, const char* password, cy_wps_credential_t* credentials, uint16_t* credential_length );

#ifdef __cplusplus
} /*extern "C" */
#endif
