/*
 * Copyright 2016-2023, Cypress Semiconductor Corporation or
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
 * Bluetooth Management (BTM) Application Programming Interface
 *
 * The BTM consists of several management entities:
 *      1. Device Control - controls the local device
 *      2. Device Discovery - manages inquiries, discover database
 *      3. ACL Channels - manages ACL connections (BR/EDR and LE)
 *      4. SCO Channels - manages SCO connections
 *      5. Security - manages all security functionality
 *      6. Power Management - manages park, sniff, hold, etc.
 *
 * WICED Bluetooth Framework Functions
 */

#pragma once
#include "wiced_bt_cfg.h"
#include "wiced_bt_dev.h"

/******************************************************
 *               Function Declarations
 ******************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/**
 *
 * @ingroup     wiced_bt_cfg
 *
 * @{
 */
/****************************************************************************/

/**
 *
 * Initialize the Bluetooth controller and stack; register
 * callback for Bluetooth event notification.
 *
 * @param[in] p_bt_management_cback     : Callback for receiving Bluetooth management events
 * @param[in] p_bt_cfg_settings         : Bluetooth stack configuration #wiced_bt_cfg_settings_t
 *
 * @return   <b> WICED_BT_SUCCESS </b> : on success; \n
 *           <b> WICED_BT_FAILED  </b> : if an error occurred
 * @note This API must be called before using any BT functionality. \n
 * If p_bt_cfg_settings is null, stack uses default parameters defined in wiced_bt_cfg.h \n
 *     However, it is strongly recommended that applications define the configuration to appropriate values based on the application use case.
 */
wiced_result_t wiced_bt_stack_init(wiced_bt_management_cback_t *p_bt_management_cback,
                                    const wiced_bt_cfg_settings_t *p_bt_cfg_settings);

/**
 * This is a blocking call (returns after all de-initialisation procedures are complete)
 * It is recommended that the application disconnect any outstanding connections prior to invoking this function.
 *
 * @return  <b>  WICED_BT_SUCCESS </b> : on success; \n
 *          <b>  WICED_BT_ERROR   </b> : if an error occurred
 */
wiced_result_t wiced_bt_stack_deinit( void );

/**
* Initialize l2cap
*
* @return wiced_result_t
*/
wiced_result_t wiced_bt_l2c_module_init(void);

 /**
 * Initialize SMP.
 * Called by application to accept incoming pairing requests
 *
 * @result wiced_result_t
 */
wiced_result_t wiced_bt_smp_module_init(void);

/**
* Enable host based resolution
* This should be invoked in the BTM_ENABLED_EVT event
*/
wiced_result_t wiced_bt_init_resolution(void);


/**@} wicedbt_Framework */


#ifdef __cplusplus
}
#endif
