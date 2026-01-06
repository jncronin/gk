/***************************************************************************************************
 * File Name: wifi_bt_if.c
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

/** @file
 *  Prototypes of functions for initializing the Wi-Fi/BT interface
 *
 *  This file provides prototypes for end-user functions to configure
 *  parts of the Wi-Fi/BT interface such as the SPI chip select.
 *
 */

#include "wifi_bt_if.h"
#include "cybsp.h"
#include "cyhal_sdio.h"
#include "stm32_cyhal_sdio_ex.h"
#include "stm32_cyhal_spi_ex.h"
#include "stm32_cyhal_uart_ex.h"
#include "stm32_cyhal_lptimer_ex.h"



#ifdef __cplusplus
extern "C"
{
#endif

static cyhal_sdio_t sdio_obj;


/***************************************************************************************************
 * cybsp_get_wifi_sdio_obj
 **************************************************************************************************/
cyhal_sdio_t* cybsp_get_wifi_sdio_obj(void)
{
    return &sdio_obj;
}


/***************************************************************************************************
 * stm32_cypal_wifi_spi_init
 **************************************************************************************************/
#if defined(HAL_SPI_MODULE_ENABLED)
void stm32_cypal_wifi_spi_init(SPI_HandleTypeDef* hspi, cyhal_gpio_t spi_cs,
                               cyhal_gpio_t wifi_reset, cyhal_gpio_t wifi_host_wake)
{
    (void)wifi_reset;
    (void)wifi_host_wake;
    (void)stm32_cypal_spi_hw_init(hspi, spi_cs);
}


#endif /* defined(HAL_SPI_MODULE_ENABLED) */


/***************************************************************************************************
 * stm32_cypal_wifi_sdio_init
 **************************************************************************************************/
#if defined(HAL_SD_MODULE_ENABLED)
cy_rslt_t stm32_cypal_wifi_sdio_init(SD_HandleTypeDef* hsdio)
{
    cy_rslt_t rslt = (uint32_t)CY_RSLT_TYPE_ERROR;

    /* Set SDIO handle in to hal. */
    if (stm32_cypal_sdio_hw_init(hsdio) == 0u)
    {
        sdio_obj.hsd = hsdio;

        /* Init SDIO */
        rslt = cyhal_sdio_init(&sdio_obj, NC, NC, NC, NC, NC, NC);
    }
    return rslt;
}


#endif /* defined(HAL_SD_MODULE_ENABLED) */


/***************************************************************************************************
 * stm32_cypal_bt_init
 **************************************************************************************************/
#if defined(HAL_UART_MODULE_ENABLED) && defined(HAL_LPTIM_MODULE_ENABLED)
cy_rslt_t stm32_cypal_bt_init(UART_HandleTypeDef* huart, LPTIM_HandleTypeDef* lptimer)
{
    cyhal_gpio_t uart_associate_pin = NC;
    uint32_t     status;

    /* Get associate pin */
    #if defined(CYBSP_BT_UART_TX)
    uart_associate_pin = CYBSP_BT_UART_TX;

    #elif defined(CYBSP_BT_UART_RX)
    uart_associate_pin = CYBSP_BT_UART_RX;

    #elif (CYHAL_UART_MAX_INSTANCES > 1)
    /* Return error if associate pin is wrong  */
    {
        /* If CYHAL_UART_MAX_INSTANCES > 1, one of the macros - CYBSP_BT_UART_TX or
         * CYBSP_BT_UART_RX - must be defined in cybsp.h to have the correct
         * associate pin information for the UART instance used by BT HCI interface */
        assert_param(false);
        return CY_RSLT_TYPE_ERROR;
    }
    #endif /* defined(CYBSP_BT_UART_TX) */

    /* Set UART handle in to hal.*/
    status = stm32_cypal_uart_hw_init(huart, uart_associate_pin);

    /* Set LP timer handle in to hal.*/
    if (status == 0)
    {
        status = stm32_cypal_lptimer_hw_init(lptimer);
    }

    return ((status == 0u) ? CY_RSLT_SUCCESS : CY_RSLT_TYPE_ERROR);
}


#endif /* defined(HAL_UART_MODULE_ENABLED) && defined(HAL_LPTIM_MODULE_ENABLED) */


#ifdef __cplusplus
} /* extern "C" */
#endif
