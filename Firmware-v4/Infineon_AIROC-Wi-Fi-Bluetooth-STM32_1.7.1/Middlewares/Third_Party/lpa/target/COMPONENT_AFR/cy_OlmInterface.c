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

/**
* @file cy_OlmInterface.c
* @brief Offload Manager Interface
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cy_OlmInterface.h"
#include "cy_lpa_compat.h"
#include "cy_lpa_wifi_ol.h"
#include "cy_lpa_wifi_ol_common.h"
#include "cy_lpa_wifi_pf_ol.h"
#include "cy_lpa_wifi_olm.h"
#include "cy_lpa_wifi_arp_ol.h"
#include "cy_result_mw.h"
#include "network_activity_handler.h"

olm_t cy_olm;
whd_interface_t cylpa_iface = NULL;

CYPRESS_WEAK const struct ol_desc *cycfg_get_default_ol_list()
{
    return NULL;
}

CYPRESS_WEAK const struct ol_desc *get_default_ol_list()
{
    return cycfg_get_default_ol_list();
}


void *cy_get_olm_instance()
{
    return &cy_olm;
}

cy_rslt_t cy_olm_create(void *ifp, ol_desc_t *oflds_list)
{
    ol_desc_t *olm_desc;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    // Get Offload configuration from device configurator
    olm_desc = (ol_desc_t *)get_default_ol_list();
    if (olm_desc == NULL)
    {
        printf("Offloads not configured \n");
		olm_desc = oflds_list;
    }

    /* Offload Manager init */
    cy_olm.ol_info.whd = ifp;
    cylpa_iface = ifp;
    cylpa_olm_init(&cy_olm, olm_desc);

    return result;
}

cy_rslt_t cy_olm_init_ols(olm_t *olm, void *whd, void *ip)
{
    return cylpa_olm_init_ols(olm, whd, ip);
}

void cy_olm_deinit_ols(olm_t *olm)
{
    cylpa_olm_deinit_ols(olm);
}

void cy_olm_pm_notification_callback(olm_t *olm, ol_pm_st_t st)
{
    cylpa_olm_dispatch_pm_notification(olm, st);
}

whd_interface_t cy_olm_get_whd_interface ( void )
{
	return cylpa_iface;
}
