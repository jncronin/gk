/***********************************************************************************************//**
 * \file stm32_cyhal_gpio.h
 *
 * \brief
 * Provides a high level interface for interacting with the GPIO on Cypress devices.
 * This interface abstracts out the chip specific details. If any chip specific
 * functionality is necessary, or performance is critical the low level functions
 * can be used directly.
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

#pragma once

#include "cyhal_hw_types.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/***************************************************************************************************
 *       Functions declarations
 **************************************************************************************************/

/** GPIO events irq handler. This function calls the callback function associated
 * with the specified GPIO. This function must be called from the EXTI line
 * detection callbacks function (HAL_GPIO_EXTI_Callback) on the application side.
 *
 * @param[in] pin  The GPIO pin associated with the callback
 *
 */
void stm32_cyhal_gpio_irq_handler(uint32_t gpio);


/***************************************************************************************************
 *      Private functions
 **************************************************************************************************/

GPIO_TypeDef* _stm32_cyhal_gpio_get_port(cyhal_gpio_t pin);
void _stm32_cyhal_gpio_enable_clock(const GPIO_TypeDef* gpio_port);


/***************************************************************************************************
 *      Private macros
 **************************************************************************************************/

/** Macro that, given a gpio, will extract the pin number */
#define CYHAL_GET_PIN(pin)      ((uint16_t)(((uint32_t)pin) & GPIO_PIN_MASK))

/** Macro that, given a gpio, will extract the port number */
#define CYHAL_GET_PORT(pin)     (_stm32_cyhal_gpio_get_port(pin))


#ifdef __cplusplus
}
#endif /* __cplusplus */
