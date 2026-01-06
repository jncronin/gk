/*
 * Copyright 2021-2023, Cypress Semiconductor Corporation or
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

#include "wiced_bt_types.h"
#include "wiced_result.h"
#include "hcidefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * Verify if the given bd_addr matches with the local bd_addr
 *
 * @param[in]      bd_addr    :  remote bd address
 *
 * @return         TRUE if bd_addr is same as local_bd_addr,FALSE otherwise
 *
 */
wiced_bool_t wiced_bt_dev_bdaddr_is_local(wiced_bt_device_address_t bd_addr);

/**
*
* Compare two BD address
*
* @param[in]      bd_addr_1    :  bd address
* @param[in]      bd_addr_2    :  bd address to be compared with bd_addr_1
*
* Returns         TRUE if both bd_addr are same,
*                 FALSE if different
*
*/
wiced_bool_t wiced_bt_dev_bdaddr_is_same(wiced_bt_device_address_t bd_addr_1, wiced_bt_device_address_t bd_addr_2);

/**
* Is controller address resolution enabled
*
*
* Returns         TRUE if enabled
*
*/
wiced_bool_t wiced_bt_dev_is_address_resolution_enabled(void);

/**
* Is device privacy supported
*
*
* Returns         TRUE if supported
*
*/
wiced_bool_t wiced_bt_dev_is_privacy_supported(void);

/**
*  This function turns OFF/ON SMP over BR/EDR (i.e. link keys crosspairing SC BR/EDR->SC LE) for the remote device.
*  If mode is set to TRUE then the crosspairing will not happen.
*
* @param[in]  mode :  Set to TRUE to disable support for smp on br.
*
* Returns void
*
*/
void wiced_bt_dev_set_no_smp_on_br(wiced_bool_t mode);
