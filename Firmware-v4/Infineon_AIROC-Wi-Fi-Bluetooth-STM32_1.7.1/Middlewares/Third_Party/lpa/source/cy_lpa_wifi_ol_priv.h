/***************************************************************************//**
* \file cy_lpa_wifi_ol_priv.h
* \version 1.0
*
*
* Offload API called by Offload Manager.
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#ifndef OL_PRIV_H__
#define OL_PRIV_H__  (1)

#ifdef __cplusplus
extern "C" {
#endif


#include "cy_lpa_wifi_ol_common.h"  /* for ol_info_t */
#include "cy_lpa_wifi_olm.h"        /* for olm_t */


/******************************************************************************/
/** \addtogroup group_lpa_internal *//** \{ */
/******************************************************************************/

/**
 * Offload host power-mode status.
 */
typedef enum ol_pm_st
{
    OL_PM_ST_GOING_TO_SLEEP,    /**< Host is going to sleep. */
    OL_PM_ST_AWAKE,            /**< Host is awake. */
    OL_PM_ST_MAX,            /**< Maximum power-mode status value; invalid power-mode status. */
} ol_pm_st_t;

/**< Offload initialization function
 *
 * Offload will cache or interpret configuration parameter and
 * register for callbacks.  IOVARs may be called through the
 * whd pointer.
 *
 * Returns RESULT_UNSUPPORTED if WiFi device firmware does not
 * support the offload.
 */
typedef int (ol_init_t)(void *ol, ol_info_t *ol_info, const void *cfg);

/**< Offload de-initialization function
 *
 * Offload will free resource and unregister callbacks.
 */
typedef void (ol_deinit_t)(void *ol);

/**< Offload host power-mode changed handler function
 *
 * Offload is notified that the host power-mode is changing
 * and has an opportunity to change its internal state.
 */
typedef void (ol_pm_t)(void *ol, ol_pm_st_t pm_state);

/**< Offload function pointer table
 *
 * Offload must define an implementation for each table entry.
 */
struct ol_fns
{
    ol_init_t *init;        /**< Offload initialization function. */
    ol_deinit_t *deinit;    /**< Offload deinitialization function. */
    ol_pm_t *pm;            /**< Offload host power-mode changed handler function. */
};

/**< Offload power management notification */
void cylpa_olm_dispatch_pm_notification(olm_t *olm, ol_pm_st_t st);

/** \} */

#ifdef __cplusplus
}
#endif
#endif /* !OL_PRIV_H__ */

