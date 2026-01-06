/***************************************************************************************************
 * \file cyhal_system.c
 *
 * \brief
 * Provides a high level interface for interacting with the Cypress power
 * management and system clock configuration. This interface abstracts out the
 * chip specific details. If any chip specific functionality is necessary, or
 * performance is critical the low level functions can be used directly.
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
#include "cyabs_rtos.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/***************************************************************************************************
 * cyhal_system_delay_ms
 **************************************************************************************************/
cy_rslt_t cyhal_system_delay_ms(uint32_t milliseconds)
{
    return cy_rtos_delay_milliseconds(milliseconds);
}


/***************************************************************************************************
 * cyhal_system_critical_section_enter
 **************************************************************************************************/
uint32_t cyhal_system_critical_section_enter(void)
{
    uint32_t result = __get_PRIMASK();  /**< backup PRIMASK bit */
    __disable_irq();                    /**< Disable all interrupts by setting PRIMASK bit on
                                           Cortex */
    return result;
}


/***************************************************************************************************
 * cyhal_system_critical_section_exit
 **************************************************************************************************/
void cyhal_system_critical_section_exit(uint32_t old_state)
{
    __set_PRIMASK(old_state);  /**< Restore PRIMASK bit*/
}


#if defined(__cplusplus)
}
#endif
