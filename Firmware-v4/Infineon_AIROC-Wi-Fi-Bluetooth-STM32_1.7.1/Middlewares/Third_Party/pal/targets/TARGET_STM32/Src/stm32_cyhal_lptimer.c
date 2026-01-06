/***************************************************************************************************
 * \file stm32_cyhal_lptimer.c
 *
 * \brief
 * Provides a high level interface for interacting with the STM32 Low-Power Timer.
 * This interface abstracts out the chip specific details. If any chip specific
 * functionality is necessary, or performance is critical the low-level functions
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
 * not authorize its products for us in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer of such
 * system or application assumes all risk of such use and in doing so agrees to
 * indemnify Cypress against all liability.
 **************************************************************************************************/

#include "cmsis_compiler.h"
#include "cyhal_system.h"
#include "cyhal_lptimer.h"
#include "stm32_cyhal_lptimer_ex.h"

#if defined(HAL_LPTIM_MODULE_ENABLED)

#if defined(__cplusplus)
extern "C" {
#endif


/***************************************************************************************************
 *      Private macros
 **************************************************************************************************/

#define _CYHAL_LPTIMER_MAX_INSTANCES            (2U)
#define _CYHAL_LPTIMER_MIN_DELAY_TICKS          (5U)

#define _CYHAL_LPTIMER_MAX_DELAY_TICKS          (0xFFFFFFFEUL)

/* consider LPTIMER being driven by 32768Hz and 32kHz */
#define _CYHAL_LPTIMER_CLK_FREQ_HZ              (32768U)

/* Adjust for period to be exactly 2 sec */
#if (_CYHAL_LPTIMER_CLK_FREQ_HZ == 32768U)
    #define _CYHAL_LPTIMER_MAX_PERIOD           (65535U)
#elif (_CYHAL_LPTIMER_CLK_FREQ_HZ == 32000U)
    #define _CYHAL_LPTIMER_MAX_PERIOD           (63999U)
#endif


#if defined (TARGET_STM32U5xx) || defined (TARGET_STM32H5xx)
    #define _CYHAL_LPTIM_FLAG_DIEROK    LPTIM_FLAG_DIEROK
    #define _CYHAL_LPTIM_FLAG_CMPOK     LPTIM_FLAG_CMP1OK
    #define _CYHAL_LPTIM_IER_ARRMIE     LPTIM_DIER_ARRMIE
#else
    #define _CYHAL_LPTIM_FLAG_CMPOK     LPTIM_FLAG_CMPOK
    #define _CYHAL_LPTIM_IER_ARRMIE     LPTIM_IER_ARRMIE
#endif /* defined (TARGET_STM32U5xx) */

/* The datasheet states: ARR is the autoreload value for the LPTIM. This value
 * must be strictly greater than the CMP[15:0] value.  Since ARR is being set
 * to _CYHAL_LPTIMER_MAX_PERIOD in the init set max compare to 1 less. */
#define _CYHAL_LPTIMER_MAX_COMPARE              (_CYHAL_LPTIMER_MAX_PERIOD - 1)

#define _CYHAL_LPTIMER_COMPARE_IS_SET           (_CYHAL_LPTIMER_MAX_DELAY_TICKS + 1)


/***************************************************************************************************
 *      Private types
 **************************************************************************************************/

typedef struct
{
    LPTIM_HandleTypeDef* hlptim;
    cyhal_lptimer_t*     obj;
    uint32_t             occupied;
} stm32_cyhal_lptimer_structs_t;


/***************************************************************************************************
 *      Private globals
 **************************************************************************************************/

static stm32_cyhal_lptimer_structs_t _cyhal_lptimer_structs[_CYHAL_LPTIMER_MAX_INSTANCES] =
    { 0 }; /* init with zeros */


/***************************************************************************************************
 * stm32_cypal_lptimer_hw_init
 **************************************************************************************************/
uint32_t stm32_cypal_lptimer_hw_init(LPTIM_HandleTypeDef* hlptim)
{
    stm32_cyhal_lptimer_structs_t* struct_ptr;
    for (uint32_t i = 0; i < _CYHAL_LPTIMER_MAX_INSTANCES; i++)
    {
        /* create a pool of available handles */
        struct_ptr = &_cyhal_lptimer_structs[i];
        if (NULL == struct_ptr->hlptim)
        {
            /* store a handle */
            struct_ptr->hlptim = hlptim;
            return 0; /* success */
        }
    }
    return 1; /* all structs are occupied already */
}


/***************************************************************************************************
 * _stm32_cypal_lptimer_get_irqn
 **************************************************************************************************/
static cy_rslt_t _stm32_cypal_lptimer_get_irqn(LPTIM_HandleTypeDef* hlptim, IRQn_Type* irqn)
{
    cy_rslt_t ret = CY_RSLT_SUCCESS;

    if (irqn == NULL)
    {
        ret = (uint32_t)CY_RSLT_TYPE_ERROR;
    }
    else

    #if defined (LPTIM1)
    if (hlptim->Instance == LPTIM1)
    {
        *irqn = LPTIM1_IRQn;
    }
    else
    #endif

    #if defined (LPTIM2)
    if (hlptim->Instance == LPTIM2)
    {
        *irqn = LPTIM2_IRQn;
    }
    else
    #endif

    #if defined (LPTIM3)
    if (hlptim->Instance == LPTIM3)
    {
        *irqn = LPTIM3_IRQn;
    }
    else
    #endif

    #if defined (LPTIM4)
    if (hlptim->Instance == LPTIM4)
    {
        *irqn = LPTIM4_IRQn;
    }
    else
    #endif

    #if defined (LPTIM5)
    if (hlptim->Instance == LPTIM5)
    {
        *irqn = LPTIM5_IRQn;
    }
    else
    #endif
    {
        ret = (uint32_t)CY_RSLT_TYPE_ERROR;
        assert_param(false); /* wrong instance */
    }

    return ret;
}


/***************************************************************************************************
 * _cyhal_lptimer_compare_match_callback
 **************************************************************************************************/
static void _cyhal_lptimer_compare_match_callback(LPTIM_HandleTypeDef* hlptim)
{
    stm32_cyhal_lptimer_structs_t* struct_ptr;

    for (uint32_t i=0; i < _CYHAL_LPTIMER_MAX_INSTANCES; i++)
    {
        struct_ptr = &_cyhal_lptimer_structs[i];

        if ((hlptim == struct_ptr->hlptim) && (struct_ptr->obj->event_enable))
        {
            cyhal_lptimer_t* obj = struct_ptr->obj;
            /* Since CMP is always enabled this gets called even without the
             * user setting a match. Therefore only call the user callback when
             * they have set a match. */
            if (obj->match == _CYHAL_LPTIMER_COMPARE_IS_SET)
            {
                obj->match = 0;
                ((cyhal_lptimer_event_callback_t)obj->callback)(obj->callback_arg,
                                                                CYHAL_LPTIMER_COMPARE_MATCH);
            }

            /* early exit as we already found target handler */
            break;
        }
    }
}


/* Check if match will occur within the next lptimer period and, if so, set
 * compare value. */
static void _cyhal_lptimer_handle_match(cyhal_lptimer_t* obj)
{
    uint32_t ticks_left;

    if (obj->match == _CYHAL_LPTIMER_COMPARE_IS_SET)
    {
        return;
    }

    uint32_t current_cnt = HAL_LPTIM_ReadCounter(obj->hlptimer);
    /* current_ticks is an extraneous variable used only in calculating
     * ticks_left. It is here to avoid an IAR compiler warning regarding the
     * order of volatile accesses (Pa082) that occurs when
     * obj->cur_time_in_lpticks and obj->match (both are volatile) are used in
     * the same calculation. */
    uint32_t current_ticks = obj->cur_time_in_lpticks + current_cnt;

    if (obj->match > current_ticks)
    {
        ticks_left = obj->match - current_ticks;
    }
    else
    {
        /* match is in past, lets schedule CMP on next timer event */
        ticks_left = 0;
    }

    if (ticks_left < _CYHAL_LPTIMER_MAX_PERIOD)
    {
        uint32_t cmp_val = (ticks_left + current_cnt) % _CYHAL_LPTIMER_MAX_PERIOD;

        /* Skip setting CMP, if we can not fit in current period,
         * _cyhal_lptimer_autoreload_match_callback will set CMP when new period starts */
        if (cmp_val < current_cnt)
        {
            return;
        }

        /* Clear flag */
        __HAL_LPTIM_CLEAR_FLAG(obj->hlptimer, _CYHAL_LPTIM_FLAG_CMPOK);

        /* Load the Timeout value in the compare register */
        #if defined (TARGET_STM32U5xx) || defined (TARGET_STM32H5xx)
        __HAL_LPTIM_COMPARE_SET(obj->hlptimer, LPTIM_CHANNEL_1, cmp_val);
        #else
        __HAL_LPTIM_COMPARE_SET(obj->hlptimer, cmp_val);
        #endif /* defined (TARGET_STM32U5xx) */

        /* Wait for the completion of the write operation to the LPTIM_CMP register */
        while (!__HAL_LPTIM_GET_FLAG(obj->hlptimer, _CYHAL_LPTIM_FLAG_CMPOK))
        {
            /* Wait for _CYHAL_LPTIM_FLAG_CMPOK */
        }

        /* match is working double-duty here: At first it is used to record
         * which tick to delay until. Once the CMP reg is set, though, the
         * match field is not needed and can be used as a flag to the compare
         * callback to indicate that both match has been set (by the user) and
         * CMP reg has been set. This is a workaround for the fact that the CMP
         * register cannot be greater than ARR (so a CMP interrupt is always
         * fired every period whether the user has set a delay or not) and to
         * flag to the compare callback that the next match is the user match.
         * */
        obj->match = _CYHAL_LPTIMER_COMPARE_IS_SET;
    }
}


/***************************************************************************************************
 * _cyhal_lptimer_autoreload_match_callback
 **************************************************************************************************/
static void _cyhal_lptimer_autoreload_match_callback(LPTIM_HandleTypeDef* hlptim)
{
    for (uint32_t i=0; i < _CYHAL_LPTIMER_MAX_INSTANCES; i++)
    {
        if (hlptim == _cyhal_lptimer_structs[i].hlptim)
        {
            cyhal_lptimer_t* obj = _cyhal_lptimer_structs[i].obj;
            obj->cur_time_in_lpticks += _CYHAL_LPTIMER_MAX_PERIOD;

            if ((obj->match != 0) && (obj->match != _CYHAL_LPTIMER_COMPARE_IS_SET))
            {
                _cyhal_lptimer_handle_match(obj);
            }

            /* early exit as we already found target handler */
            break;
        }
    }
}


/***************************************************************************************************
 * _cyhal_lptimer_set_match_common
 **************************************************************************************************/
static uint32_t _cyhal_lptimer_set_match_common(cyhal_lptimer_t* obj, uint32_t ticks)
{
    uint32_t critical_section = cyhal_system_critical_section_enter();
    obj->match = ticks;
    _cyhal_lptimer_handle_match(obj);
    cyhal_system_critical_section_exit(critical_section);

    return CY_RSLT_SUCCESS;
}


/***************************************************************************************************
 * cyhal_lptimer_set_match
 **************************************************************************************************/
cy_rslt_t cyhal_lptimer_set_match(cyhal_lptimer_t* obj, uint32_t ticks)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    /* Error out if trying to set match less than current ticks (i.e in the
     * past) */
    if (ticks <= cyhal_lptimer_read(obj))
    {
        return CYHAL_LPTIMER_RSLT_ERR_BAD_ARGUMENT;
    }

    return _cyhal_lptimer_set_match_common(obj, ticks);
}


/***************************************************************************************************
 * cyhal_lptimer_set_delay
 **************************************************************************************************/
cy_rslt_t cyhal_lptimer_set_delay(cyhal_lptimer_t* obj, uint32_t delay)
{
    uint32_t delay_tmp = delay;

    /* Check the parameters */
    assert_param(NULL != obj);

    if (delay < _CYHAL_LPTIMER_MIN_DELAY_TICKS)
    {
        delay_tmp = _CYHAL_LPTIMER_MIN_DELAY_TICKS;
    }
    if (delay > _CYHAL_LPTIMER_MAX_DELAY_TICKS)
    {
        delay_tmp = _CYHAL_LPTIMER_MAX_DELAY_TICKS;
    }

    #ifdef CYID_BSP_2741
    obj->event_enable = true;
    #endif /* CYID_BSP_2741 */

    return _cyhal_lptimer_set_match_common(obj, cyhal_lptimer_read(obj) + delay_tmp);
}


/***************************************************************************************************
 * cyhal_lptimer_init
 **************************************************************************************************/
cy_rslt_t cyhal_lptimer_init(cyhal_lptimer_t* obj)
{
    cy_rslt_t                      rslt = CY_RSLT_SUCCESS;
    stm32_cyhal_lptimer_structs_t* struct_ptr;
    IRQn_Type                      irqn;

    /* Check the parameters */
    assert_param(NULL != obj);

    obj->hlptimer            = NULL;
    obj->cur_time_in_lpticks = 0u;
    obj->match               = 0u;

    for (uint32_t i = 0; i < _CYHAL_LPTIMER_MAX_INSTANCES; i++)
    {
        struct_ptr = &_cyhal_lptimer_structs[i];
        if (struct_ptr->occupied == 0)
        {   /* make obj refer an added handle  */
            obj->hlptimer        = struct_ptr->hlptim;
            struct_ptr->occupied = 1;
            struct_ptr->obj      = obj;
            break;
        }
    }

    /* INFO: cyhal_lptimer_init() suggests IRQ initialization */
    if ((NULL != obj->hlptimer) &&
        (HAL_LPTIM_STATE_RESET != obj->hlptimer->State))
    {   /*
         * LPTIMER is initialized
         * LPTIMER is enabled
         *
         * It also contains a call to HAL_LPTIM_MspInit where:
         * Clock is enabled
         *
         * LPTIMER IRQs are NOT enabled here
         */
        #if defined (TARGET_STM32U5xx) || defined (TARGET_STM32H5xx)
        /* For STM32U5 a period of Autoreload register (ARR) configure in
         * HAL_LPTIM_Init by using hlptimer->Init.Period value */
        obj->hlptimer->Init.Period = _CYHAL_LPTIMER_MAX_PERIOD;
        #endif /* defined (TARGET_STM32U5xx) */

        if (HAL_LPTIM_Init(obj->hlptimer) != HAL_OK)
        {
            cyhal_lptimer_free(obj);
            rslt = (uint32_t)CY_RSLT_TYPE_ERROR;
        }

        /* Enable interrupt */
        if ((rslt == CY_RSLT_SUCCESS) &&
            (_stm32_cypal_lptimer_get_irqn(obj->hlptimer, &irqn) == CY_RSLT_SUCCESS))
        {
            HAL_NVIC_SetPriority(irqn, 0, 0);
            HAL_NVIC_EnableIRQ(irqn);
        }

        /* Start LPtimer */
        if (rslt == CY_RSLT_SUCCESS)
        {
            HAL_StatusTypeDef hal_ret;
            /* Set ARR to MAX(16bit) */
            /* Set CMP to MAX(16bit) */
            #if defined (TARGET_STM32U5xx) || defined (TARGET_STM32H5xx)
            hal_ret = HAL_LPTIM_TimeOut_Start_IT(obj->hlptimer,
                                                 _CYHAL_LPTIMER_MAX_COMPARE);
            #else
            hal_ret = HAL_LPTIM_TimeOut_Start_IT(obj->hlptimer,
                                                 _CYHAL_LPTIMER_MAX_PERIOD,
                                                 _CYHAL_LPTIMER_MAX_COMPARE);
            #endif /* defined (TARGET_STM32U5xx) */


            /* Register Autoreload match callback */
            (void)HAL_LPTIM_RegisterCallback(obj->hlptimer, HAL_LPTIM_AUTORELOAD_MATCH_CB_ID,
                                             &_cyhal_lptimer_autoreload_match_callback);

            /* Enable Autoreload match interrupt enable */
            #if defined (TARGET_STM32U5xx) || defined (TARGET_STM32H5xx)
            __HAL_LPTIM_CLEAR_FLAG(obj->hlptimer, _CYHAL_LPTIM_FLAG_DIEROK);
            __HAL_LPTIM_ENABLE_IT(obj->hlptimer, _CYHAL_LPTIM_IER_ARRMIE);
            while (!__HAL_LPTIM_GET_FLAG(obj->hlptimer, _CYHAL_LPTIM_FLAG_DIEROK))
            {
                /* Wait for _CYHAL_LPTIM_FLAG_DIEROK */
            }
            #else
            __HAL_LPTIM_ENABLE_IT(obj->hlptimer, _CYHAL_LPTIM_IER_ARRMIE);
            #endif /* defined (TARGET_STM32U5xx) */


            rslt = (hal_ret == HAL_OK) ? CY_RSLT_SUCCESS : CY_RSLT_TYPE_ERROR;
        }
    }
    else
    {
        rslt = (uint32_t)CY_RSLT_TYPE_ERROR;
    }
    return rslt;
}


/***************************************************************************************************
 * cyhal_lptimer_free
 **************************************************************************************************/
void cyhal_lptimer_free(cyhal_lptimer_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    (void)HAL_LPTIM_TimeOut_Stop_IT(obj->hlptimer);
    /*
     * LPTIMER is de-initialized
     * LPTIMER is disabled
     *
     * It also contains a call to HAL_LPTIM_MspDeInit where:
     * LPTIMER IRQs are disabled
     * Clock is disabled
     */
    (void)HAL_LPTIM_DeInit(obj->hlptimer);

    stm32_cyhal_lptimer_structs_t* struct_ptr;
    for (uint32_t i=0; i < _CYHAL_LPTIMER_MAX_INSTANCES; i++)
    {
        struct_ptr = &_cyhal_lptimer_structs[i];
        if (struct_ptr->hlptim == obj->hlptimer)
        {
            struct_ptr->occupied = 0;
            break;
        }
    }
}


/***************************************************************************************************
 * cyhal_lptimer_read
 **************************************************************************************************/
uint32_t cyhal_lptimer_read(const cyhal_lptimer_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    /* Read current tick */
    uint32_t critical_section = cyhal_system_critical_section_enter();
    uint32_t ticks            = HAL_LPTIM_ReadCounter(obj->hlptimer) + obj->cur_time_in_lpticks;
    cyhal_system_critical_section_exit(critical_section);
    return ticks;
}


/***************************************************************************************************
 * cyhal_lptimer_register_callback
 **************************************************************************************************/
void cyhal_lptimer_register_callback(cyhal_lptimer_t* obj, cyhal_lptimer_event_callback_t callback,
                                     void* callback_arg)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    /* store of callback pointer */
    obj->callback = (void*)callback;

    /* store of callback arg */
    obj->callback_arg = callback_arg;
}


/***************************************************************************************************
 * cyhal_lptimer_enable_event
 **************************************************************************************************/
void cyhal_lptimer_enable_event(cyhal_lptimer_t* obj, cyhal_lptimer_event_t event,
                                uint8_t intr_priority, bool enable)
{
    (void)event;

    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(event == CYHAL_LPTIMER_COMPARE_MATCH);

    obj->event_enable = enable;

    #ifdef CYID_BSP_2741
    IRQn_Type irqn;

    /* Update priority */
    if (_stm32_cypal_lptimer_get_irqn(obj->hlptimer, &irqn) == CY_RSLT_SUCCESS)
    {
        HAL_NVIC_SetPriority(irqn, intr_priority, 0);
    }

    /* Register  Compare match callback */
    (void)HAL_LPTIM_RegisterCallback(obj->hlptimer, HAL_LPTIM_COMPARE_MATCH_CB_ID,
                                     &_cyhal_lptimer_compare_match_callback);
    #else /* ifdef CYID_BSP_2741 */
    IRQn_Type irqn;

    if (enable)
    {
        /* Update priority */
        if (_stm32_cypal_lptimer_get_irqn(obj->hlptimer, &irqn) == CY_RSLT_SUCCESS)
        {
            HAL_NVIC_SetPriority(irqn, intr_priority, 0);
        }

        /* Register  Compare match callback */
        (void)HAL_LPTIM_RegisterCallback(obj->hlptimer, HAL_LPTIM_COMPARE_MATCH_CB_ID,
                                         &_cyhal_lptimer_compare_match_callback);
    }
    else
    {
        /* Un-register Compare match callback */
        (void)HAL_LPTIM_UnRegisterCallback(obj->hlptimer, HAL_LPTIM_COMPARE_MATCH_CB_ID);
    }
    #endif /* ifdef CYID_BSP_2741 */
}


#if defined(__cplusplus)
}
#endif

#endif /* (HAL_LPTIM_MODULE_ENABLED) */
