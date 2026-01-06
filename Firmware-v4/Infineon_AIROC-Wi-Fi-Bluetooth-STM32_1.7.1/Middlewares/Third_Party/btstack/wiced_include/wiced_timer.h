/*
 * Copyright 2019-2023, Cypress Semiconductor Corporation or
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

/** @file
 *
 *  \addtogroup timer Timer Management Services
 *  \ingroup HardwareDrivers
 *
 *  @{
 *
 * Defines the interfaces for Timer Management Services
 */

#ifndef _WICED_TIMER_H_
#define _WICED_TIMER_H_

#include "wiced_result.h"

typedef void *wiced_timer_callback_arg_t;                  /**< Timer callback argument type */
typedef wiced_timer_callback_arg_t WICED_TIMER_PARAM_TYPE; /**< To avoid recompiling older apps */
typedef wiced_timer_callback_arg_t TIMER_PARAM_TYPE;       /**< To avoid recompiling older apps */

/**
 * Function prototype for the timer call back.
 * @param[in]    cb_params      :Timer callback function
 */
typedef void (wiced_timer_callback_t)(wiced_timer_callback_arg_t cb_params);

/** Timer structure.
 *
 *  NOTE: this structure is used internally by the wiced stack. Applications MUST NOT
 *        modify any of the elements of this structure.
 *
 *        Timer control block memory MUST be peristant from when the timer is initialized,
 *        using wiced_init_timer(), till it is de-initialized, using wiced_deinit_timer().
 */
typedef struct _wiced_timer_t
{
    struct _wiced_timer_t       *p_next;   /**< internal, next pointer to \ref wiced_timer_t */
    wiced_timer_callback_t      *p_cback;  /**< internal, the callback function */
    wiced_timer_callback_arg_t  cb_arg;    /**< internal, the callback argument */
    uint32_t                    pi;        /**< internal, timer flags */
    uint64_t                    tt;        /**< internal, timer timeout */
} wiced_timer_t;

/**
 * Defines the wiced timer types. These timers are system tick driven and a
 * systick is 1 millisecond.So the minimum timer resolution supported is
 * 1 millisecond
 */
typedef enum
{
    WICED_MILLI_SECONDS_TIMER,
    WICED_SECONDS_TIMER,
    WICED_MILLI_SECONDS_PERIODIC_TIMER,
    WICED_SECONDS_PERIODIC_TIMER,
} wiced_timer_type_e;

#ifdef __cplusplus
extern "C"
{
#endif

/**  Initializes the timer
 *
 *@param[in]    p_timer         :Pointer to the timer structure
 *@param[in]    p_timer_cb      :Timer callback function to be invoked on timer expiry
 *@param[in]    cb_arg          :Parameter to be passed to the timer callback function which
 *                                              gets invoked on timer expiry,if any
 *@param[in]    timer_type      :Shows if the timer is milliseconds or seconds, and if periodic or not
 *
 * @return   wiced_result_t
 */
wiced_result_t wiced_init_timer (wiced_timer_t* p_timer, wiced_timer_callback_t *p_timer_cb,
                                 wiced_timer_callback_arg_t cb_arg, wiced_timer_type_e timer_type);

/** Starts a timer
 * @note: Starting an already running timer restarts the timer timeout
 *
 * @param[in]    p_timer                :Pointer to the timer structure
 * @param[in]    timeout_ms             :timeout in milliseconds
 *
 * @return       wiced_result_t
 */
wiced_result_t wiced_start_timer (wiced_timer_t* p_timer,uint32_t timeout_ms);

/** Stops a timer
 *
 * @param[in]    p_timer      :Pointer to the timer structure
 *
 * @return       wiced_result_t
 */
wiced_result_t wiced_stop_timer (wiced_timer_t *p_timer);

/**  Checks if a timer is in use
*
*@param[in]    p_timer                  :Pointer to the timer structure
*
* @return   TRUE if the timer is in use and FALSE if the timer is not in use
*/
wiced_bool_t wiced_is_timer_in_use (wiced_timer_t *p_timer);

/** Deinitialize a timer instance and stops the timer if it is running
 *
 * @param[in]    p_timer                :Pointer to the timer
 *
 * @return       wiced_result_t
 */
wiced_result_t wiced_deinit_timer (wiced_timer_t* p_timer);

/** @} */
#ifdef __cplusplus
}
#endif

#endif // _WICED_TIMER_H_
