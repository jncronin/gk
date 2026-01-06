/***********************************************************************************************//**
 * \file stm32_cyhal_sdio_ex.h
 *
 * \brief
 * Extensions to the SDIO HAL needed for the port.
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

#if defined(HAL_SD_MODULE_ENABLED)

#if defined(__cplusplus)
extern "C" {
#endif

/***************************************************************************************************
 *      Device-specific macros
 **************************************************************************************************/

#if defined (TARGET_STM32H7xx)
/* RCC clock for SDMMC */
  #define STM32_RCC_PERIPHCLK_SDMMC RCC_PERIPHCLK_SDMMC
#elif defined (TARGET_STM32L5xx)
/* RCC clock for SDMMC */
  #define STM32_RCC_PERIPHCLK_SDMMC RCC_PERIPHCLK_SDMMC1
#elif defined (TARGET_STM32U5xx)
/* RCC clock for SDMMC */
  #define STM32_RCC_PERIPHCLK_SDMMC RCC_PERIPHCLK_SDMMC
#elif defined (TARGET_STM32H5xx)
/* RCC clock for SDMMC */
  #define STM32_RCC_PERIPHCLK_SDMMC RCC_PERIPHCLK_SDMMC1
#endif // if defined (TARGET_STM32H7xx)


/***************************************************************************************************
 *      Private types
 **************************************************************************************************/

typedef struct
{
    uint8_t  write_data;            /* 0 - 7 */
    uint32_t _stuff2          : 1;  /* 8     */
    uint32_t register_address : 17; /* 9-25  */
    uint32_t _stuff           : 1;  /* 26    */
    uint32_t raw_flag         : 1;  /* 27    */
    uint32_t function_number  : 3;  /* 28-30 */
    uint32_t rw_flag          : 1;  /* 31    */
} sdio_cmd52_argument_t;

typedef struct
{
    uint32_t count            : 9;  /* 0-8   */
    uint32_t register_address : 17; /* 9-25  */
    uint32_t op_code          : 1;  /* 26    */
    uint32_t block_mode       : 1;  /* 27    */
    uint32_t function_number  : 3;  /* 28-30 */
    uint32_t rw_flag          : 1;  /* 31    */
} sdio_cmd53_argument_t;

typedef union
{
    uint32_t              value;
    sdio_cmd52_argument_t cmd52;
    sdio_cmd53_argument_t cmd53;
} sdio_cmd_argument_t;


/***************************************************************************************************
 *       Functions declarations
 **************************************************************************************************/

/** Initialize the SDIO hardware.
 *
 *  @param hsd     The handle to the SDIO hardware block.
 */
uint32_t stm32_cypal_sdio_hw_init(SD_HandleTypeDef* hsd);

/* This function handles SDIO interrupt request */
void stm32_cyhal_sdio_irq_handler(void);


#if defined(__cplusplus)
}
#endif

#endif /* defined(HAL_SD_MODULE_ENABLED) */
