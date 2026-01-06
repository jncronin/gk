/***************************************************************************************************
 * \file stm32_cyhal_syspm.c
 *
 * \brief
 * Implementation for syspm abstraction
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2021 Cypress Semiconductor Corporation
 * This software, including source code, documentation and related materials
 * ("Software"), is owned by Cypress Semiconductor Corporation or one of its
 * subsidiaries ("Cypress") and is protected by and subject to worldwide patent
 * protection (United States and foreign), United States copyright laws and
 * international treaty provisions. Therefore, you may use this Software only
 * as provided in the license agreement accompanying the software package from
 * which you obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source
 * code solely for use in connection with Cypress's integrated circuit products.
 * Any reproduction, modification, translation, compilation, or representation
 * of this Software except as specified above is prohibited without the express
 * written permission of Cypress.
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
 * including Cypress's product in a High Risk Product, the manufacturer of such
 * system or application assumes all risk of such use and in doing so agrees to
 * indemnify Cypress against all liability.
 **************************************************************************************************/

#include "cyhal_system.h"
#include "cyhal_syspm.h"
#include "stm32_cyhal_syspm_ex.h"
#include "stm32_cyhal_common.h"


/***************************************************************************************************
 *      Private macros
 **************************************************************************************************/

#define USHRT_MAX                 (50U)

static uint16_t deep_sleep_lock = 0;


/***************************************************************************************************
 * stm32_cypal_syspm_deepsleep_allowed
 **************************************************************************************************/
bool stm32_cypal_syspm_deepsleep_allowed(void)
{
    return ((deep_sleep_lock == 0) ? true : false);
}


/***************************************************************************************************
 * cyhal_syspm_lock_deepsleep
 **************************************************************************************************/
void cyhal_syspm_lock_deepsleep(void)
{
    assert_param(deep_sleep_lock != USHRT_MAX);
    uint32_t intr_status = cyhal_system_critical_section_enter();
    if (deep_sleep_lock < USHRT_MAX)
    {
        deep_sleep_lock++;
    }
    cyhal_system_critical_section_exit(intr_status);
}


/***************************************************************************************************
 * cyhal_syspm_unlock_deepsleep
 **************************************************************************************************/
void cyhal_syspm_unlock_deepsleep(void)
{
    assert_param(deep_sleep_lock != 0U);
    uint32_t intr_status = cyhal_system_critical_section_enter();
    if (deep_sleep_lock > 0U)
    {
        deep_sleep_lock--;
    }
    cyhal_system_critical_section_exit(intr_status);
}
