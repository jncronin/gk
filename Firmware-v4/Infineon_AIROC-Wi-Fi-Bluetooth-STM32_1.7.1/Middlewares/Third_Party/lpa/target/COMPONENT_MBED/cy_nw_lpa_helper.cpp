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

/** @file cy_nw_lpa_helper.cpp
 *
 * Network IP change utility for MBEDOS
 */
#include <stddef.h>
#include "cy_nw_lpa_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

static cy_nw_ip_status_change_callback_t *cy_ip_status_change_callback = NULL;

void cylpa_nw_ip_initialize_status_change_callback(cylpa_nw_ip_status_change_callback_t *cb, cylpa_nw_ip_status_change_callback_func_t *cb_func, void *arg)
{
    cy_ip_status_change_callback = (cy_nw_ip_status_change_callback_t *)cb;
    cy_ip_status_change_callback->arg = arg;
    cy_ip_status_change_callback->cb_func = ( cy_nw_ip_status_change_callback_func_t *)cb_func;
    cy_ip_status_change_callback->priv = NULL;
    cy_nw_ip_initialize_status_change_callback(cy_ip_status_change_callback, cb_func, arg);
}

void cylpa_nw_ip_register_status_change_callback(cy_nw_ip_interface_t nw_interface, cylpa_nw_ip_status_change_callback_t *cb)
{
    cy_nw_ip_register_status_change_callback(nw_interface, (cy_nw_ip_status_change_callback_t *)cb);
}

void cylpa_nw_ip_unregister_status_change_callback(cy_nw_ip_interface_t nw_interface, cylpa_nw_ip_status_change_callback_t *cb)
{
    cy_nw_ip_unregister_status_change_callback(nw_interface, (cy_nw_ip_status_change_callback_t *)cb);
}

#ifdef __cplusplus
}
#endif
