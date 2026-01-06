/***********************************************************************************************//**
 * \file stm32_cyhal_uart_ex.h
 *
 * \brief
 * Extensions to the SPI HAL needed for the port.
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
#include "cyhal_general_types.h"

#include "stm32_cyhal_common.h"


#if defined(__cplusplus)
extern "C" {
#endif

/***************************************************************************************************
 *      Private macros
 **************************************************************************************************/

/* Numbers of supported UART instances */
#if !defined(CYHAL_UART_MAX_INSTANCES)
    #define CYHAL_UART_MAX_INSTANCES          (1u)
#endif /* !defined(CYHAL_UART_MAX_INSTANCES) */

#if !defined(CYHAL_UART_USE_RTOS_MUTEX)
    #define CYHAL_UART_USE_RTOS_MUTEX         (1)
#endif /* !defined(CYHAL_UART_USE_RTOS_MUTEX) */

/* UART specific return codes */
#define CYHAL_UART_RSLT_ERR_HAL_ERROR                 \
    (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CYHAL_RSLT_MODULE_UART, 4))

/***************************************************************************************************
 *       Functions declarations
 **************************************************************************************************/

/** Initialize the UART hardware.
 *
 *  @param huart     The handle to the UART hardware.
 *
 *  @param associate_pin   Information for the GPIO (RX or TX) associated with
 *                         the UATR block.
 */
uint32_t stm32_cypal_uart_hw_init(UART_HandleTypeDef* huart, cyhal_gpio_t associate_pin);

#if defined(__cplusplus)
}
#endif
