/***************************************************************************************************
 * File Name: wifi_bt_if.h
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

#include "stm32_cyhal_sdio_ex.h"
#include "stm32_cyhal_spi_ex.h"
#include "cyhal_hw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif


/***************************************************************************************************
 *             Function declarations
 **************************************************************************************************/

/** Initialize the Wi-Fi SPI interface
 *
 *  @param hspi               The handle for the SPI hardware to use with the WiFi interface.
 *
 *  @param spi_cs             The GPIO to use for the SPI chip select pin.
 *                            If this is set to NC, a chip select is not used with the SPI.
 *
 *  @param wifi_reset         The GPIO to use for the WiFi reset pin.
 *
 *  @param wifi_host_wake     The GPIO to use for the WiFi host wake pin.
 *
 */
#if defined(HAL_SPI_MODULE_ENABLED)
void stm32_cypal_wifi_spi_init(SPI_HandleTypeDef* hspi, cyhal_gpio_t spi_cs,
                               cyhal_gpio_t wifi_reset, cyhal_gpio_t wifi_host_wake);
#endif /* defined(HAL_SPI_MODULE_ENABLED) */


/** Initialize the Wi-Fi SDIO interface
 *
 *  @param hsdio  The handle for the SDIO hardware to use with the WiFi interface.
 *
 *  @return The status of the init request
 *
 */
#if defined(HAL_SD_MODULE_ENABLED)
cy_rslt_t stm32_cypal_wifi_sdio_init(SD_HandleTypeDef* hsdio);
#endif /* defined(HAL_SD_MODULE_ENABLED) */


/** Initialize the BT interface
 *
 *  @param huart               The handle for the UART hardware to use with the BT HCI interface.
 *
 *  @param lptimer             The handle for the LP timer hardware to use with the BT HCI
 *                             interface.
 *  @return The status of the init request
 *
 * Note:
 *  The macros CYBSP_BT_UART_TX, CYBSP_BT_UART_RX in cybsp.h associate UART pin (RX or TX).
 *  This pin is used to match cyhal_uart object with UART handle during cyhal_uart_init
 *  function.
 *  If CYHAL_UART_MAX_INSTANCES = 1 (default), defining CYBSP_BT_UART_TX/RX is optional.
 */
#if defined(HAL_UART_MODULE_ENABLED) && defined(HAL_LPTIM_MODULE_ENABLED)
cy_rslt_t stm32_cypal_bt_init(UART_HandleTypeDef* huart, LPTIM_HandleTypeDef* lptimer);
#endif /* defined(HAL_UART_MODULE_ENABLED) && defined(HAL_LPTIM_MODULE_ENABLED) */


#ifdef __cplusplus
} /* extern "C" */


#endif
