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

/**
* @file cy_mqtt_api_internal.h
* @brief This file contains structures and defines needed for encapsulating the parameters of MQTT APIs.
*        These structures can be used to encapsulate and send API parameters over multiple interfaces. 
*        For example, virtual connectivity manager uses these structures to communicate the API parameters over IPC.
*/

#ifndef CY_MQTT_API_INTERNAL_H_
#define CY_MQTT_API_INTERNAL_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "cy_mqtt_api.h"
#ifdef ENABLE_MULTICORE_CONN_MW
#include "cy_vcm_internal.h"
#endif
/******************************************************
 *             Structures
 ******************************************************/
 
/**
 * Structure which encapsulates the parameters of cy_mqtt_publish API. 
 */
typedef struct
{
    cy_mqtt_t                  mqtt_handle;     /**< Handle to MQTT instance */
    cy_mqtt_publish_info_t     *pub_msg;        /**< MQTT publish message information */
} cy_mqtt_publish_params_t;

/**
 * Structure which encapsulates the parameters of cy_mqtt_subscribe API. 
 */
typedef struct
{
    cy_mqtt_t                  mqtt_handle;     /**< Handle to MQTT instance */
    cy_mqtt_subscribe_info_t   *sub_info;       /**< MQTT unsubscribe information structure */
    uint8_t                    sub_count;       /**< Number of subscription topics in the subscription array */
} cy_mqtt_subscribe_params_t;

/**
 * Structure which encapsulates the parameters of cy_mqtt_unsubscribe API. 
 */
typedef struct
{
    cy_mqtt_t                  mqtt_handle;     /**< Handle to MQTT instance */
    cy_mqtt_unsubscribe_info_t *unsub_info;     /**< Pointer to array of MQTT unsubscription information structure */
    uint8_t                    unsub_count;     /**< Number of unsubscription topics in the unsubscription array */
} cy_mqtt_unsubscribe_params_t;

/**
 * Structure which encapsulates the parameters of cy_mqtt_get_handle API. 
 */
typedef struct
{
    cy_mqtt_t                  *mqtt_handle;    /**< Handle to MQTT instance */
    char                       descriptor[ CY_MQTT_DESCP_MAX_LEN + 1 ]; /**< MQTT descriptor used to identify the mqtt object uniquely */
} cy_mqtt_get_handle_params_t;

/**
 * Structure which encapsulates the parameters of cy_mqtt_register_event_callback API. 
 */
typedef struct
{
    cy_mqtt_t                  mqtt_handle;     /**< Handle to MQTT instance */
#ifdef ENABLE_MULTICORE_CONN_MW
    cy_vcm_internal_callback_t event_callback;  /**< Callback function registered by the virtual API with VCM to receive MQTT events. This function internally calls the application callback. */
#else
    cy_mqtt_callback_t         event_callback;  /**< Application callback function which needs to be called on arrival of MQTT incoming publish packets and network disconnection notification from network layer */
#endif
    void                       *user_data;      /**< Pointer to user data to be passed in the event callback */
} cy_mqtt_register_event_callback_params_t;

/**
 * Structure which encapsulates the parameters of cy_mqtt_deregister_event_callback API. 
 */
typedef struct
{
    cy_mqtt_t                  mqtt_handle;     /**< Handle to MQTT instance */
#ifdef ENABLE_MULTICORE_CONN_MW
    cy_vcm_internal_callback_t event_callback;  /**< Callback function registered by the virtual API with VCM which needs to be deregistered. */
#else
    cy_mqtt_callback_t         event_callback;  /**< Application callback function which needs to be deregistered. */
#endif
} cy_mqtt_deregister_event_callback_params_t;

/**
 * Structure which encapsulates the parameters of cy_mqtt_callback_t function.
 */
typedef struct
{
    cy_mqtt_t                  mqtt_handle;     /**< Handle to MQTT instance */
    cy_mqtt_event_t            event;           /**< MQTT event information structure */
    void                       *user_data;      /**< Pointer to user data provided during \ref cy_mqtt_create */
} cy_mqtt_callback_params_t;

#if defined(__cplusplus)
}
#endif

#endif  /* CY_MQTT_API_INTERNAL_H_ */
