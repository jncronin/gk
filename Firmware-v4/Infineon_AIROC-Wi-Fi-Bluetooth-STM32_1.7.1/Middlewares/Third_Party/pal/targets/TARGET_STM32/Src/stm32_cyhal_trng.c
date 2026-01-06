/***************************************************************************************************
 * \file cyhal_trng.c
 *
 * \brief
 * Provides a high level interface for interacting with the Cypress True Random
 * Number Generator. This is a wrapper around the lower level PDL API.
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
#include "cyhal_hw_types.h"
#include "cy_result.h"
#include "cyhal_trng.h"
#include "stm32_cyhal_common.h"

#if defined(HAL_RNG_MODULE_ENABLED)


#if defined(__cplusplus)
extern "C"
{
#endif

/** The requested resource type is invalid */
#define CYHAL_HWMGR_RSLT_ERR_INVALID                    \
    CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CYHAL_RSLT_MODULE_HWMGR, 0)
/** The requested resource is already in use */
#define CYHAL_HWMGR_RSLT_ERR_INUSE                      \
    CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CYHAL_RSLT_MODULE_HWMGR, 1)

static RNG_HandleTypeDef* _cyhal_trng_hndl = NULL;

/***************************************************************************************************
 * cyhal_trng_init
 **************************************************************************************************/
cy_rslt_t cyhal_trng_init(cyhal_trng_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    if (_cyhal_trng_hndl != NULL)
    {   /* Associate HAL handle with an object */
        obj->hrng = _cyhal_trng_hndl;
        if (HAL_RNG_Init(obj->hrng) == HAL_OK)
        {
            return CY_RSLT_SUCCESS;
        }
        else
        {
            return CYHAL_HWMGR_RSLT_ERR_INUSE;
        }
    }
    else
    {
        return CYHAL_HWMGR_RSLT_ERR_INVALID;
    }
}


/***************************************************************************************************
 * cyhal_trng_free
 **************************************************************************************************/
void cyhal_trng_free(cyhal_trng_t* obj)
{
    HAL_RNG_DeInit(obj->hrng);
    obj->hrng = NULL;
}


/***************************************************************************************************
 * cyhal_trng_generate
 **************************************************************************************************/
uint32_t cyhal_trng_generate(const cyhal_trng_t* obj)
{
    (void)(obj);
    uint32_t random32bit = 0;

    if (HAL_RNG_GenerateRandomNumber(obj->hrng, &random32bit) != HAL_OK)
    {
        assert_param(false);
    }

    return random32bit;
}


/***************************************************************************************************
 * stm32_cypal_uart_hw_init
 **************************************************************************************************/
uint32_t stm32_cypal_trng_hw_init(RNG_HandleTypeDef* trng_handle)
{
    /* Check the parameters */
    assert_param(NULL != trng_handle);

    if (_cyhal_trng_hndl == NULL)
    {
        /* Prestore handle for given HW resource */
        _cyhal_trng_hndl = trng_handle;
        return 0;
    }
    /* Failed to store a RNG handle - already allocated.*/
    assert_param(false);
    return 1;
}


#if defined(__cplusplus)
}
#endif
#endif /* defined(HAL_RNG_MODULE_ENABLED) */
