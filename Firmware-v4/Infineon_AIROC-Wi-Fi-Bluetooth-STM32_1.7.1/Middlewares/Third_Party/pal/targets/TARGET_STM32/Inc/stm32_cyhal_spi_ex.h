/***********************************************************************************************//**
 * \file stm32_cyhal_spi_ex.h
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
#include "stm32_cyhal_common.h"

#if defined(HAL_SPI_MODULE_ENABLED)

#if defined(__cplusplus)
extern "C" {
#endif


/***************************************************************************************************
 *       Functions declarations
 **************************************************************************************************/

/** Initialize the SPI hardware.
 *
 *  @param hspi     The handle to the SPI to use with the WiFi interface.
 *
 *  @param spi_cs   Information for the GPIO associated with the SPI chip select pin.
 *                  If this is set to NC, a chip select is not used with the SPI.
 *
 *  @return The status of the init request
 *
 */
cy_rslt_t stm32_cypal_spi_hw_init(SPI_HandleTypeDef* hspi, cyhal_gpio_t spi_cs);

#if defined(__cplusplus)
}
#endif

#endif /* defined(HAL_SPI_MODULE_ENABLED) */
