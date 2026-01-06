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
 * 
 */

/** @file cy_nw_lpa_helper.h
 * 
 * This is a collection of LPA network IP change functions used by LPA Middle-ware libraries.
 * 
 */
#pragma once
#include "cy_nw_helper.h"

#if defined(__cplusplus)
extern "C" {
#endif

/******************************************************************************/
/** \addtogroup group_nw_lpahelper_structures *//** \{ */
/******************************************************************************/
/** Network IP status change callback function
 *
 * @param[in] iface : Pointer to the network interface for which the callback is invoked.
 * @param[in] arg   : User data object provided during the status change callback registration.

 * @return none
 */
typedef void (cylpa_nw_ip_status_change_callback_func_t)(cy_nw_ip_interface_t iface, void *arg);

/** Network IP status change callback info */
typedef struct cylpa_nw_ip_status_change_callback
{
    cylpa_nw_ip_status_change_callback_func_t *cb_func; /**< IP address status change callback function */
    void *arg;                                    /**< User data */
    void *priv;                                   /**< NW interface */
} cylpa_nw_ip_status_change_callback_t;

/** \} */

/*****************************************************************************/
/**
 *
 *  @addtogroup group_nw_lpahelper_func
 *
 * This is a collection of network helper functions which would be used by various Cypress Middleware libraries.
 *
 *  @{
 */
/*****************************************************************************/

/** Initialize status change callback
 *
 * Initialize @ref cylpa_nw_ip_status_change_callback_t instead of
 * directly manipulating the callback struct.
 *
 * @param[in, out] info : Pointer to network IP status change callback information structure which would be filled upon return
 * @param[in] cbf       : Pointer to callback function to be invoked during network status change
 * @param[in] arg       : User data object to be sent sent in the callback function
 *
 * @return none
 */
void cylpa_nw_ip_initialize_status_change_callback(cylpa_nw_ip_status_change_callback_t *info, cylpa_nw_ip_status_change_callback_func_t *cbf, void *arg);

/** Registers IP status change callback
 *
 * @param[in] iface : Pointer to network interface object
 * @param[in] info  : Pointer to the status change information structure
 *
 * @return none
 */
void cylpa_nw_ip_register_status_change_callback(cy_nw_ip_interface_t iface, cylpa_nw_ip_status_change_callback_t *info);

/** Un-registers IP status change callback
 *
 * @param[in] iface : Pointer to network interface object
 * @param[in] info  : Pointer to the status change information structure
 *
 * @return none
 */
void cylpa_nw_ip_unregister_status_change_callback(cy_nw_ip_interface_t iface, cylpa_nw_ip_status_change_callback_t *info);

/** @} */

#if defined(__cplusplus)
}
#endif
