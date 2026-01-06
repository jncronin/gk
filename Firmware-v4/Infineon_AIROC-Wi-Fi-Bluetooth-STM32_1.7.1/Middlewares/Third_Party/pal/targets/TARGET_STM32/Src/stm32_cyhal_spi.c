/***************************************************************************************************
 * \file stm32_cyhal_spi.c
 *
 * \brief
 * Implementation for SPI abstraction
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
#include "cyhal_general_types.h"
#include "cyhal_gpio.h"
#include "cyhal_spi.h"
#include "stm32_cyhal_common.h"
#include "stm32_cyhal_gpio_ex.h"
#include "stm32_cyhal_spi_ex.h"


#if defined(HAL_SPI_MODULE_ENABLED)

/***************************************************************************************************
 *      Private globals
 **************************************************************************************************/

static SPI_HandleTypeDef* wifi_spi;
static GPIO_TypeDef*      spi_cs_gpio_port = NULL;
static uint16_t           spi_cs_gpio_pin;


/***************************************************************************************************
 * spi_transfer_complete
 **************************************************************************************************/
static void spi_transfer_complete(SPI_HandleTypeDef* hspi)
{
    (void)hspi;

    if (spi_cs_gpio_port != NULL)
    {
        HAL_GPIO_WritePin(spi_cs_gpio_port, spi_cs_gpio_pin, GPIO_PIN_SET);
    }
}


/***************************************************************************************************
 * _stm32_cyhal_spi_cs_init
 **************************************************************************************************/
cy_rslt_t stm32_cypal_spi_hw_init(SPI_HandleTypeDef* hspi, cyhal_gpio_t spi_cs)
{
    cy_rslt_t         rslt = CY_RSLT_SUCCESS;
    HAL_StatusTypeDef status;

    assert_param(hspi != NULL);
    #if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
    assert_param(spi_cs != NC);
    #endif

    wifi_spi = hspi;

    if (spi_cs != NC)
    {
        spi_cs_gpio_port = CYHAL_GET_PORT(spi_cs);
        spi_cs_gpio_pin  = CYHAL_GET_PIN(spi_cs);

        rslt = cyhal_gpio_init(spi_cs, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_OPENDRAINDRIVESHIGH,
                               true);
    }

    if (rslt != CY_RSLT_SUCCESS)
    {
        rslt = CYHAL_SPI_RSLT_BAD_ARGUMENT;
    }
    #if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
    else
    {
        status = HAL_SPI_RegisterCallback(hspi, HAL_SPI_TX_COMPLETE_CB_ID, spi_transfer_complete);

        if (status == HAL_OK)
        {
            status = HAL_SPI_RegisterCallback(hspi, HAL_SPI_RX_COMPLETE_CB_ID,
                                              spi_transfer_complete);
        }

        if (status == HAL_OK)
        {
            status = HAL_SPI_RegisterCallback(hspi, HAL_SPI_TX_RX_COMPLETE_CB_ID,
                                              spi_transfer_complete);
        }

        if (status != HAL_OK)
        {
            rslt = CYHAL_SPI_RSLT_BAD_ARGUMENT;
        }
    }
    #endif /* USE_HAL_SPI_REGISTER_CALLBACKS */

    return rslt;
}


/***************************************************************************************************
 * cyhal_spi_transfer
 **************************************************************************************************/
cy_rslt_t cyhal_spi_transfer(cyhal_spi_t* obj, const uint8_t* tx, size_t tx_length, uint8_t* rx,
                             size_t rx_length, uint8_t write_fill)
{
    HAL_StatusTypeDef status;

    (void)obj;
    (void)write_fill;

    if ((tx == NULL) && (rx == NULL))
    {
        return CY_RSLT_SUCCESS;
    }

    if (spi_cs_gpio_port != NULL)
    {
        HAL_GPIO_WritePin(spi_cs_gpio_port, spi_cs_gpio_pin, GPIO_PIN_RESET);
    }

    if ((tx != NULL) && (rx != NULL))
    {
        if (tx_length != rx_length)
        {
            /* NOTE:  WPRINT_WHD_INFO(("write_fill not currently supported\n")); */
            return CYHAL_SPI_RSLT_TRANSFER_ERROR;
        }
        status = HAL_SPI_TransmitReceive_DMA(wifi_spi, (uint8_t*)tx, (uint8_t*)rx,
                                             (uint16_t)tx_length);
    }
    else if (tx != NULL)
    {
        status = HAL_SPI_Transmit_DMA(wifi_spi, (uint8_t*)tx, (uint16_t)tx_length);
    }
    else /* if (rx != NULL) */
    {
        status = HAL_SPI_Receive_DMA(wifi_spi, (uint8_t*)rx, (uint16_t)rx_length);
    }

    if (status == HAL_OK)
    {
        return CY_RSLT_SUCCESS;
    }
    else
    {
        return CYHAL_SPI_RSLT_TRANSFER_ERROR;
    }
}


/* TODO: rework callback usage.
 * Need to use HAL_SPI_RegisterCallback, instead of Legacy weak
 * SPI callback functions. */
/***************************************************************************************************
 * HAL_SPI_ErrorCallback
 **************************************************************************************************/
/*
   void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi)
   {
   }
 */


#endif /* defined(HAL_SPI_MODULE_ENABLED) */
