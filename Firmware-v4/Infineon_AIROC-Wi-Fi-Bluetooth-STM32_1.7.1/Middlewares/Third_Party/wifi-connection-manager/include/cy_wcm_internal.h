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
* @file cy_wcm_internal.h
* @brief This file contains structures and defines needed for encapsulating the parameters of WCM APIs.
*        These structures can be used to encapsulate and send API parameters over multiple interfaces.
*        For example, Virtual Connectivity Manager uses these structures to communicate the API parameters over IPC.
*/

#ifndef CY_WCM_INTERNAL_H
#define CY_WCM_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cy_wcm.h"
#ifdef ENABLE_MULTICORE_CONN_MW
#include "cy_vcm_internal.h"
#endif

/**
 * \addtogroup group_wcm_structures
 * \{
 */
/******************************************************
 *             Structures
 ******************************************************/
/**
 * Structure which encapsulates the parameters of cy_wcm_register_event_callback API.
 */
typedef struct
{
#ifdef ENABLE_MULTICORE_CONN_MW
    cy_vcm_internal_callback_t event_callback;      /**< Callback function registered by the virtual WCM API with VCM to receive WCM events. This function internally calls the application callback. */
#else
    cy_wcm_event_callback_t    event_callback;      /**< WCM event callback function pointer type; if registered, callback is invoked when WHD posts events to WCM */
#endif
} cy_wcm_register_event_callback_params_t;

/**
 * Structure which encapsulates the parameters of cy_wcm_deregister_event_callback API.
 */
typedef struct
{
#ifdef ENABLE_MULTICORE_CONN_MW
    cy_vcm_internal_callback_t event_callback;      /**< VCM event callback function pointer to be de-registered with VCM by the virtual WCM API */
#else
    cy_wcm_event_callback_t    event_callback;      /**< WCM event callback function pointer to be de-registered */
#endif
} cy_wcm_deregister_event_callback_params_t;

/**
 * Structure which encapsulates the parameters of cy_wcm_event_callback_t function.
 */
typedef struct
{
    cy_wcm_event_t             event;               /**< WCM event type. */
    cy_wcm_event_data_t        *event_data;         /**< A pointer to the event data. The event data will be freed once the callback returns from the application */
} cy_wcm_event_callback_params_t;

/** \} group_wcm_structures */

#ifdef __cplusplus
} /* extern C */
#endif

#endif  /* CY_WCM_INTERNAL_H */
