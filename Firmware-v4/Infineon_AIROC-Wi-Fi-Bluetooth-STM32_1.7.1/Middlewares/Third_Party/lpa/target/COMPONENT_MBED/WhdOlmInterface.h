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
* @file WhdOlmInterface.h
* @brief Offload Manager Interface
*/
#ifndef WHD_OLM_INTERFACE_H
#define WHD_OLM_INTERFACE_H

#include "mbed.h"
#include "WhdSTAInterface.h"
#include "cy_lpa_wifi_olm.h"
#include "cy_lpa_wifi_ol_priv.h"

extern "C" const struct ol_desc *get_default_ol_list();

class WhdOlmInterface : public WhdSTAInterface::OlmInterface
{
public:
WhdOlmInterface(const struct ol_desc *list = ::get_default_ol_list() ) : _ols_inited(false) {
    memset(&_olm, 0, sizeof(_olm));
    cylpa_olm_init(&_olm, list);
}

virtual ~WhdOlmInterface() {
    if (_ols_inited)
    {
        cylpa_olm_deinit_ols(&_olm);
        _ols_inited = false;
    }
}

int init_ols(void *whd, void *ip) {
    int result = false;
    if (!_ols_inited)
    {
        result = cylpa_olm_init_ols(&_olm, whd, ip);
        _ols_inited = result == 0 ? true : false;
    }
    return result;
}

void deinit_ols(void) {
    if (_ols_inited)
    {
        cylpa_olm_deinit_ols(&_olm);
        _ols_inited = false;
    }
}

int sleep() {
    if (!_ols_inited)
    {
        return -1;
    }
    cylpa_olm_dispatch_pm_notification(&_olm, OL_PM_ST_GOING_TO_SLEEP);
    return 0;
}

int wake() {
    if (!_ols_inited)
    {
        return -1;
    }
    cylpa_olm_dispatch_pm_notification(&_olm, OL_PM_ST_AWAKE);
    return 0;
}

private:
bool _ols_inited;
olm_t _olm;
};

#endif /* WHD_OLM_INTERFACE_H */

