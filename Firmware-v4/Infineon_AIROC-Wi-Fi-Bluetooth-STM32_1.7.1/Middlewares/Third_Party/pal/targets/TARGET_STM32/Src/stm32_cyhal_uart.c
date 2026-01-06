/***************************************************************************************************
 * \file cyhal_uart.c
 *
 * \brief
 * Provides a high level interface for interacting with the STM32 UART. This is
 * a wrapper around the lower level PDL API.
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

#include <stdlib.h>
#include <string.h>
#include "cy_utils.h"
#include "cyhal_hw_types.h"
#include "cyhal_uart.h"
#include "stm32_cyhal_uart_ex.h"
#include "cyhal_syspm.h"

#include "stm32_cyhal_common.h"

#if (CYHAL_UART_USE_RTOS_MUTEX == 1)
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */


#if defined(HAL_UART_MODULE_ENABLED)

#if defined(__cplusplus)
extern "C"
{
#endif

/***************************************************************************************************
 *      Private macros
 **************************************************************************************************/

#define CYHAL_UART_OVERSAMPLE                 (12UL)
#define CYHAL_UART_OVERSAMPLE_MIN             (8UL)
#define CYHAL_UART_OVERSAMPLE_MAX             (16UL)

#define CYHAL_REPEAT_OPERATION                (10UL)


/***************************************************************************************************
 *      Private types
 **************************************************************************************************/

typedef struct
{
    UART_HandleTypeDef* huart;
    cyhal_gpio_t        associate_pin;
    cyhal_uart_t*       occupied_obj;
    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    cy_mutex_t          lock;
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */
} stm32_cyhal_uart_structs_t;


/***************************************************************************************************
 *      Private variables
 **************************************************************************************************/

static stm32_cyhal_uart_structs_t _cyhal_uart_structs[CYHAL_UART_MAX_INSTANCES] =
    { 0 }; /* init with zeros */


/***************************************************************************************************
 *      Private functions
 **************************************************************************************************/

static uint32_t _stm32_cyhal_uart_convert_parity(cyhal_uart_parity_t parity);
static uint32_t _stm32_cyhal_uart_convert_stopbits(uint32_t stopbits);
static uint32_t _stm32_cyhal_uart_convert_wordlength(uint32_t wordlength);
static UART_HandleTypeDef* _stm32_cyhal_uart_alloc_hw(cyhal_uart_t* obj, cyhal_gpio_t tx,
                                                      cyhal_gpio_t rx);
static void _stm32_cyhal_uart_free_hw(cyhal_uart_t* obj);
static void cyhal_uart_prepare_dma_buffer(cyhal_uart_t* obj, void* buffer, size_t length,
                                          bool is_tx);
static void _stm32_cyhal_uart_enable_irq(USART_TypeDef* instance, uint32_t priority, bool enable);
static void _stm32_cyhal_uart_critical_section_enter(cyhal_uart_t* obj);
static void _stm32_cyhal_uart_critical_section_exit(cyhal_uart_t* obj);
static IRQn_Type _stm32_cyhal_uart_get_irqn(USART_TypeDef* instance);
static IRQn_Type _stm32_cyhal_uart_get_dma_irqn(DMA_HandleTypeDef* hdma);
#if (USE_HAL_UART_REGISTER_CALLBACKS == 1) || (USE_HAL_USART_REGISTER_CALLBACKS == 1)
static cyhal_uart_t* _stm32_cyhal_uart_get_obj(UART_HandleTypeDef* huart);
static void _stm32_cyhal_uart_tx_complete_callback(UART_HandleTypeDef* huart);
static void _stm32_cyhal_uart_rx_complete_callback(UART_HandleTypeDef* huart);
static void _stm32_cyhal_uart_error_callback(UART_HandleTypeDef* huart);
static void _stm32_cyhal_uart_rx_fifo_full_callback(UART_HandleTypeDef* huart);
static void _stm32_cyhal_uart_tx_fifo_empty_callback(UART_HandleTypeDef* huart);
static void _stm32_cyhal_uart_rx_not_empty_irq_handler(UART_HandleTypeDef* huart);
#endif /* #if (USE_HAL_UART_REGISTER_CALLBACKS == 1) */
#if (CYHAL_UART_USE_RTOS_MUTEX == 1)
static stm32_cyhal_uart_structs_t* _stm32_cyhal_uart_get_struct(cyhal_uart_t* obj);
#endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */


/***************************************************************************************************
 * cyhal_uart_init
 **************************************************************************************************/
cy_rslt_t cyhal_uart_init(cyhal_uart_t* obj, cyhal_gpio_t tx, cyhal_gpio_t rx,
                          cyhal_gpio_t cts, cyhal_gpio_t rts, const cyhal_clock_t* clk,
                          const cyhal_uart_cfg_t* cfg)
{
    (void)clk;
    (void)cts;
    (void)rts;

    HAL_StatusTypeDef status = HAL_OK;

    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != cfg);
    assert_param((NC != tx) && (NC != rx));

    /* Clean uart object */
    (void)memset(obj, 0, sizeof(cyhal_uart_t));

    /* Try to allocate uart instance from _cyhal_uart_structs table */
    obj->huart = _stm32_cyhal_uart_alloc_hw(obj, tx, rx);
    if (obj->huart != NULL)
    {
        uint32_t temp;
        bool     new_config = false;

        /* Disable UART interrupts enabled by Stm32CubeMx
         * the cyhal_uart_enable_event function enables these interrupts
         * when needed */
        _stm32_cyhal_uart_enable_irq(obj->huart->Instance, 0u, false);

        /* Set common parameters from cyhal_uart_cfg_t */
        temp = _stm32_cyhal_uart_convert_wordlength(cfg->data_bits);
        if (obj->huart->Init.WordLength != temp)
        {
            obj->huart->Init.WordLength = temp;
            new_config                  = true;
        }

        temp = _stm32_cyhal_uart_convert_stopbits(cfg->stop_bits);
        if (obj->huart->Init.StopBits != temp)
        {
            obj->huart->Init.StopBits =temp;
            new_config                = true;
        }

        temp = _stm32_cyhal_uart_convert_parity(cfg->parity);
        if (obj->huart->Init.Parity != temp)
        {
            obj->huart->Init.Parity = temp;
            new_config              = true;
        }

        /* NOTE: skip uart initialize in following cases
         *   -- uart is initialized (state != HAL_UART_STATE_RESET) and
         *   -- init parameters is the same as in input configuration (cfg)
         */
        if ((obj->huart->gState == HAL_UART_STATE_RESET) || new_config)
        {
            /* Initialize the UART mode according to the specified parameters */
            status = HAL_UART_Init(obj->huart);

            if (status == HAL_OK)
            {
                status = HAL_UARTEx_SetTxFifoThreshold(obj->huart, UART_TXFIFO_THRESHOLD_1_8);
            }

            if (status == HAL_OK)
            {
                status = HAL_UARTEx_SetRxFifoThreshold(obj->huart, UART_RXFIFO_THRESHOLD_1_8);
            }

            if (status == HAL_OK)
            {
                status = HAL_UARTEx_DisableFifoMode(obj->huart);
            }
        }

        if (__HAL_UART_GET_IT(obj->huart, UART_IT_RXNE))
        {
            /* Send Receive Data flush Request to discard the received data */
            __HAL_UART_SEND_REQ(obj->huart, UART_RXDATA_FLUSH_REQUEST);
        }

        if (status != HAL_OK)
        {
            cyhal_uart_free(obj);
        }
    }
    else
    {
        /* Making out an error code 15, which is far enough from already implemented */
        return (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CYHAL_RSLT_MODULE_UART, 15));
    }

    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    if (status == HAL_OK)
    {
        /* Initialize UART resource mutex.  */
        stm32_cyhal_uart_structs_t* uart_struct = _stm32_cyhal_uart_get_struct(obj);
        status = (cy_rtos_init_mutex(&uart_struct->lock) == CY_RSLT_SUCCESS) ?
                 HAL_OK : HAL_ERROR;
    }
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */

    return ((status != HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
}


/***************************************************************************************************
 * cyhal_uart_free
 **************************************************************************************************/
void cyhal_uart_free(cyhal_uart_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    /* De-initialize the UART mutex */
    stm32_cyhal_uart_structs_t* uart_struct = _stm32_cyhal_uart_get_struct(obj);
    (void)cy_rtos_deinit_mutex(&uart_struct->lock);
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */

    /* De-initialize the UART peripheral */
    (void)HAL_UART_DeInit(obj->huart);

    /* Free from uart resource table */
    _stm32_cyhal_uart_free_hw(obj);
}


/***************************************************************************************************
 * cyhal_uart_set_baud
 **************************************************************************************************/
cy_rslt_t cyhal_uart_set_baud(cyhal_uart_t* obj, uint32_t baudrate, uint32_t* actualbaud)
{
    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != actualbaud);

    if (actualbaud != NULL)
    {
        *actualbaud = baudrate;
    }

    if (baudrate == obj->huart->Init.BaudRate)
    {
        return CY_RSLT_SUCCESS;
    }

    /* Set new baud rate value */
    obj->huart->Init.BaudRate = baudrate;

    return ((UART_SetConfig(obj->huart) !=
             HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
}


/***************************************************************************************************
 * cyhal_uart_configure
 **************************************************************************************************/
cy_rslt_t cyhal_uart_configure(cyhal_uart_t* obj, const cyhal_uart_cfg_t* cfg)
{
    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != cfg);

    obj->huart->Init.Parity     = _stm32_cyhal_uart_convert_parity(cfg->parity);
    obj->huart->Init.StopBits   = _stm32_cyhal_uart_convert_stopbits(cfg->stop_bits);
    obj->huart->Init.WordLength = _stm32_cyhal_uart_convert_wordlength(cfg->data_bits);

    return ((UART_SetConfig(obj->huart) !=
             HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
}


/***************************************************************************************************
 * cyhal_uart_set_flow_control
 **************************************************************************************************/
cy_rslt_t cyhal_uart_enable_flow_control(cyhal_uart_t* obj, bool enable_cts, bool enable_rts)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    if (!enable_cts && enable_rts)
    {
        obj->huart->Init.HwFlowCtl = UART_HWCONTROL_RTS;
    }
    else if (enable_cts && !enable_rts)
    {
        obj->huart->Init.HwFlowCtl = UART_HWCONTROL_CTS;
    }
    else if (enable_rts)
    {
        obj->huart->Init.HwFlowCtl = UART_HWCONTROL_RTS_CTS;
    }
    else
    {
        obj->huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    }

    return ((UART_SetConfig(obj->huart) !=
             HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
}


/***************************************************************************************************
 * cyhal_uart_write
 **************************************************************************************************/
cy_rslt_t cyhal_uart_write(cyhal_uart_t* obj, void* tx, size_t* tx_length)
{
    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != tx);
    assert_param(NULL != tx_length);

    HAL_StatusTypeDef status;

    /* Enter UART critical section (disable UART interrupt if it is enabled) */
    _stm32_cyhal_uart_critical_section_enter(obj);

    /* Send an amount of data in blocking mode */
    status = HAL_UART_Transmit(obj->huart, tx, (uint16_t)*tx_length, 0xFFFF);

    /* Exit UART critical section */
    _stm32_cyhal_uart_critical_section_exit(obj);

    if (status == HAL_BUSY)
    {
        return CYHAL_SYSPM_RSLT_ERR_PM_PENDING;
    }
    else
    {
        return ((status != HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
    }
}


/***************************************************************************************************
 * cyhal_uart_read
 **************************************************************************************************/
cy_rslt_t cyhal_uart_read(cyhal_uart_t* obj, void* rx, size_t* rx_length)
{
    HAL_StatusTypeDef status = HAL_TIMEOUT;

    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != rx);
    assert_param(NULL != rx_length);

    /* Check Receive data register not empty flag */
    if (__HAL_UART_GET_FLAG(obj->huart, UART_FLAG_RXNE) != 0u)
    {
        /* Enter UART critical section (disable UART interrupt if it is enabled) */
        _stm32_cyhal_uart_critical_section_enter(obj);

        /* Receive an amount of data in blocking mode */
        status = HAL_UART_Receive(obj->huart, rx, (uint16_t)*rx_length, 0U);

        /* Exit UART critical section */
        _stm32_cyhal_uart_critical_section_exit(obj);

        if (status == HAL_TIMEOUT)
        {
            *rx_length = *rx_length - obj->huart->RxXferCount;
        }
    }
    else
    {
        *rx_length = 0;
    }

    if (status == HAL_BUSY)
    {
        return CYHAL_SYSPM_RSLT_ERR_PM_PENDING;
    }
    else
    {
        return (((status == HAL_TIMEOUT) || (status == HAL_OK)) ?
                CY_RSLT_SUCCESS : CYHAL_UART_RSLT_ERR_HAL_ERROR);
    }
}


/***************************************************************************************************
 * cyhal_uart_write_abort
 **************************************************************************************************/
cy_rslt_t cyhal_uart_write_abort(cyhal_uart_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    return ((HAL_UART_AbortTransmit(obj->huart) !=
             HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
}


/***************************************************************************************************
 * cyhal_uart_read_abort
 **************************************************************************************************/
cy_rslt_t cyhal_uart_read_abort(cyhal_uart_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    return ((HAL_UART_AbortReceive(obj->huart) !=
             HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
}


/***************************************************************************************************
 * cyhal_uart_prepare_dma_tx_buf
 **************************************************************************************************/
static void cyhal_uart_prepare_dma_buffer(cyhal_uart_t* obj, void* buffer, size_t length,
                                          bool is_tx)
{
    if (is_tx)
    {
        /* TX */
        /* Copy data to DMA buffer */
        (void)memcpy((void*)obj->dma_buff.tx, buffer, length);

        /* D-cache maintenance
         *
         * NOTE: Cache clean/invalidation functions require to have the
         * address 32-byte aligned, UART driver allocates DMA UART TX buffers
         * aligned to a 32-byte boundary.
         */
        #if defined(_CYHAL_DCACHE_MAINTENANCE)
        if (SCB->CCR & SCB_CCR_DC_Msk)
        {
            SCB_CleanDCache_by_Addr((uint32_t*)obj->dma_buff.tx, CYHAL_UART_TX_DMA_BUFFER_SIZE);
        }
        #endif
    }
    else
    {
        /* RX */
        /* Store buffer address */
        obj->rx_async_buff     = (void*)buffer;
        obj->rx_async_buff_len = length;

        /* D-cache maintenance
         *
         * NOTE: Cache clean/invalidation functions require to have the
         * address 32-byte aligned, UART driver allocates DMA UART RX buffer
         * aligned to a 32-byte boundary.
         */
        #if defined(_CYHAL_DCACHE_MAINTENANCE)
        /* Cache-Invalidate the output from DMA */
        if (SCB->CCR & SCB_CCR_DC_Msk)
        {
            SCB_InvalidateDCache_by_Addr((uint32_t*)obj->dma_buff.rx,
                                         CYHAL_UART_RX_DMA_BUFFER_SIZE);
        }
        #endif
    }
}


/***************************************************************************************************
 * cyhal_uart_write_async
 **************************************************************************************************/
cy_rslt_t cyhal_uart_write_async(cyhal_uart_t* obj, void* tx, size_t length)
{
    HAL_StatusTypeDef status;
    uint32_t          repeat_operation = CYHAL_REPEAT_OPERATION;

    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != tx);
    assert_param((0u != length) && (length < CYHAL_UART_TX_DMA_BUFFER_SIZE));

    /* Lock UART resource  */
    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    stm32_cyhal_uart_structs_t* uart_struct = _stm32_cyhal_uart_get_struct(obj);
    if (cy_rtos_get_mutex(&uart_struct->lock, CY_RTOS_NEVER_TIMEOUT) != CY_RSLT_SUCCESS)
    {
        return CYHAL_SYSPM_RSLT_ERR_PM_PENDING;
    }
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */

    /* Prepare DMA buffer */
    if (obj->huart->hdmarx != NULL)
    {
        cyhal_uart_prepare_dma_buffer(obj, tx, length, true);
    }

    /* Enter UART critical section (disable UART interrupt if it is enabled) */
    _stm32_cyhal_uart_critical_section_enter(obj);

    /* Transit operation */
    do
    {
        if (obj->huart->hdmatx == NULL)
        {
            status = HAL_UART_Transmit_IT(obj->huart, tx, (uint16_t)length);
        }
        else
        {
            status = HAL_UART_Transmit_DMA(obj->huart, (uint8_t*)obj->dma_buff.tx,
                                           (uint16_t)length);
        }

        if (status != HAL_OK)
        {
            HAL_Delay(1);
        }

        repeat_operation--;
    } while ((status != HAL_OK) && (repeat_operation != 0u));

    /* Exit UART critical section */
    _stm32_cyhal_uart_critical_section_exit(obj);

    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    /* Un-lock UART resource */
    (void)cy_rtos_set_mutex(&uart_struct->lock);
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */

    if (status == HAL_BUSY)
    {
        return CYHAL_SYSPM_RSLT_ERR_PM_PENDING;
    }
    else
    {
        return ((status != HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
    }
}


/***************************************************************************************************
 * cyhal_uart_read_async
 **************************************************************************************************/
cy_rslt_t cyhal_uart_read_async(cyhal_uart_t* obj, void* rx, size_t length)
{
    HAL_StatusTypeDef status;
    uint32_t          repeat_operation = CYHAL_REPEAT_OPERATION;

    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != rx);
    assert_param((0u != length) && (length < CYHAL_UART_RX_DMA_BUFFER_SIZE));

    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    /* Lock UART resource  */
    stm32_cyhal_uart_structs_t* uart_struct = _stm32_cyhal_uart_get_struct(obj);
    if (cy_rtos_get_mutex(&uart_struct->lock, CY_RTOS_NEVER_TIMEOUT) != CY_RSLT_SUCCESS)
    {
        return CYHAL_SYSPM_RSLT_ERR_PM_PENDING;
    }
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */

    /* Prepare DMA buffer */
    if (obj->huart->hdmarx != NULL)
    {
        cyhal_uart_prepare_dma_buffer(obj, rx, length, false);
    }

    /* Enter UART critical section (disable UART interrupt if it is enabled) */
    _stm32_cyhal_uart_critical_section_enter(obj);

    /* Receive Operation */
    do
    {
        if (obj->huart->hdmarx == NULL)
        {
            status = HAL_UART_Receive_IT(obj->huart, rx, (uint16_t)length);
        }
        else
        {
            status = HAL_UART_Receive_DMA(obj->huart, (uint8_t*)obj->dma_buff.rx, (uint16_t)length);
        }

        if (status != HAL_OK)
        {
            HAL_Delay(1);
        }
        repeat_operation--;
    } while ((status != HAL_OK) && (repeat_operation != 0u));

    /* Exit UART critical section */
    _stm32_cyhal_uart_critical_section_exit(obj);

    /* NOTE: keep context.rxBufIdx to have BWC with bluetooth-freertos (PSoC implementation) */
    obj->context.rxBufIdx = obj->huart->NbRxDataToProcess;

    #if (CYHAL_UART_USE_RTOS_MUTEX == 1)
    /* Un-lock UART resource */
    (void)cy_rtos_set_mutex(&uart_struct->lock);
    #endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */

    if (status == HAL_BUSY)
    {
        return CYHAL_SYSPM_RSLT_ERR_PM_PENDING;
    }
    else
    {
        return ((status != HAL_OK) ? CYHAL_UART_RSLT_ERR_HAL_ERROR : CY_RSLT_SUCCESS);
    }
}


/***************************************************************************************************
 * cyhal_uart_is_tx_active
 **************************************************************************************************/
bool cyhal_uart_is_tx_active(cyhal_uart_t* obj)
{
    HAL_UART_StateTypeDef state;

    state = HAL_UART_GetState(obj->huart);

    /* NOTE: it will also return TRUE in case of both - TX AND RX are busy
     *       simultaneously */
    return ((state & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX);
}


/***************************************************************************************************
 * cyhal_uart_is_rx_active
 **************************************************************************************************/
bool cyhal_uart_is_rx_active(cyhal_uart_t* obj)
{
    HAL_UART_StateTypeDef state;

    state = HAL_UART_GetState(obj->huart);

    /* NOTE: it will also return TRUE in case of both - TX AND RX are busy
     *       simultaneously */
    return ((state & HAL_UART_STATE_BUSY_RX) == HAL_UART_STATE_BUSY_RX);
}


/***************************************************************************************************
 * Callback functions
 * NOTE: For UART/USART callbacks, USE_HAL_UART/USART_REGISTER_CALLBACKS must be enabled
 * in STM32CubeMx
 **************************************************************************************************/

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1) || (USE_HAL_USART_REGISTER_CALLBACKS == 1)

/***************************************************************************************************
 * _stm32_cyhal_uart_tx_complete_callback
 **************************************************************************************************/
static void _stm32_cyhal_uart_tx_complete_callback(UART_HandleTypeDef* huart)
{
    cyhal_uart_t* obj = _stm32_cyhal_uart_get_obj(huart);
    if (obj != NULL)
    {
        ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg, CYHAL_UART_IRQ_TX_DONE);
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_rx_complete_callback
 **************************************************************************************************/
static void _stm32_cyhal_uart_rx_complete_callback(UART_HandleTypeDef* huart)
{
    cyhal_uart_t* obj = _stm32_cyhal_uart_get_obj(huart);

    if (obj != NULL)
    {
        if (obj->huart->hdmarx != NULL)
        {
            (void)memcpy(obj->rx_async_buff, (void*)obj->dma_buff.rx, obj->rx_async_buff_len);
        }

        /* Call registered callback */
        if (obj->callback != NULL)
        {
            ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg, CYHAL_UART_IRQ_RX_DONE);
        }

        if ((obj->irq & (uint32_t)CYHAL_UART_IRQ_RX_NOT_EMPTY) != 0U)
        {
            /* Restore the RxISR function pointer. RxISR is clean in default RX interrupt
             * handler, when Rx complete Set own RX handler to catch IRQ_RX_NOT_EMPTY
             * interrupt */
            obj->huart->RxISR = _stm32_cyhal_uart_rx_not_empty_irq_handler;

            /* Enable the UART Parity Error interrupt and Data Register Not Empty interrupt */
            SET_BIT(obj->huart->Instance->CR1, USART_CR1_RXNEIE_RXFNEIE);
        }
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_error_callback
 **************************************************************************************************/
static void _stm32_cyhal_uart_error_callback(UART_HandleTypeDef* huart)
{
    cyhal_uart_t* obj = _stm32_cyhal_uart_get_obj(huart);
    if (obj != NULL)
    {
        if (obj->huart->RxState == HAL_UART_STATE_BUSY_RX)
        {
            ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg,
                                                         CYHAL_UART_IRQ_RX_ERROR);
        }
        else
        {
            ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg,
                                                         CYHAL_UART_IRQ_TX_ERROR);
        }
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_rx_fifo_full_callback
 **************************************************************************************************/
static void _stm32_cyhal_uart_rx_fifo_full_callback(UART_HandleTypeDef* huart)
{
    cyhal_uart_t* obj = _stm32_cyhal_uart_get_obj(huart);
    if (obj != NULL)
    {
        ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg,
                                                     CYHAL_UART_IRQ_RX_FULL);
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_tx_fifo_empty_callback
 **************************************************************************************************/
static void _stm32_cyhal_uart_tx_fifo_empty_callback(UART_HandleTypeDef* huart)
{
    cyhal_uart_t* obj = _stm32_cyhal_uart_get_obj(huart);
    if (obj != NULL)
    {
        ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg,
                                                     CYHAL_UART_IRQ_TX_EMPTY);
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_rx_not_empty_irq_handler
 **************************************************************************************************/
static void _stm32_cyhal_uart_rx_not_empty_irq_handler(UART_HandleTypeDef* huart)
{
    cyhal_uart_t* obj = _stm32_cyhal_uart_get_obj(huart);
    if (obj != NULL)
    {
        ((cyhal_uart_event_callback_t)obj->callback)(obj->callback_arg,
                                                     CYHAL_UART_IRQ_RX_NOT_EMPTY);
    }
}


#endif /* (USE_HAL_UART_REGISTER_CALLBACKS == 1) */


/***************************************************************************************************
 * cyhal_uart_register_callback
 **************************************************************************************************/
void cyhal_uart_register_callback(cyhal_uart_t* obj, cyhal_uart_event_callback_t callback,
                                  void* callback_arg)
{
    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != callback);

    /* Store callback info */
    obj->callback     = (void*)callback;
    obj->callback_arg = callback_arg;
}


/***************************************************************************************************
 * cyhal_uart_enable_event
 *
 * NOTE: By default, the input parameter 'intr_priority' is skipped in
 * cyhal_uart_enable_event function. In STM32, all UARTS events that came from one
 * UARTx IRQ and the priority for this IRQ are set in STM32CubeMX Environment.
 *
 * For a user case that requires overwriting the UART IRQ priority
 * from cyhal_uart_enable_event, define the CYHAL_UART_USE_HAL_IRQ_PRIOPITY
 * macro in cybsp.h.
 *
 **************************************************************************************************/
void cyhal_uart_enable_event(cyhal_uart_t* obj, cyhal_uart_event_t event,
                             uint8_t intr_priority, bool enable)
{
    #if (USE_HAL_UART_REGISTER_CALLBACKS == 1) || (USE_HAL_USART_REGISTER_CALLBACKS == 1)
    pUART_CallbackTypeDef p_callback;

    /* Register CYHAL_UART_IRQ_TX_DONE callback */
    if ((event & CYHAL_UART_IRQ_TX_DONE) != (uint32_t)CYHAL_UART_IRQ_NONE)
    {
        p_callback = &_stm32_cyhal_uart_tx_complete_callback;
        if (enable)
        {
            obj->irq |= (uint32_t)CYHAL_UART_IRQ_TX_DONE;
            (void)HAL_UART_RegisterCallback(obj->huart, HAL_UART_TX_COMPLETE_CB_ID, p_callback);
        }
        else
        {
            obj->irq &= (uint32_t) ~CYHAL_UART_IRQ_TX_DONE;
            (void)HAL_UART_UnRegisterCallback(obj->huart, HAL_UART_TX_COMPLETE_CB_ID);
        }
    }

    /* Register CYHAL_UART_IRQ_RX_DONE callback */
    if ((event & CYHAL_UART_IRQ_RX_DONE) != (uint32_t)CYHAL_UART_IRQ_NONE)
    {
        p_callback = &_stm32_cyhal_uart_rx_complete_callback;
        if (enable)
        {
            obj->irq |= (uint32_t)CYHAL_UART_IRQ_RX_DONE;
            (void)HAL_UART_RegisterCallback(obj->huart, HAL_UART_RX_COMPLETE_CB_ID, p_callback);
        }
        else
        {
            obj->irq &= (uint32_t) ~CYHAL_UART_IRQ_RX_DONE;
            (void)HAL_UART_UnRegisterCallback(obj->huart, HAL_UART_RX_COMPLETE_CB_ID);
        }
    }

    /* Register CYHAL_UART_IRQ_TX_ERROR  callback */
    if (((event & CYHAL_UART_IRQ_TX_ERROR) != (uint32_t)CYHAL_UART_IRQ_NONE) ||
        ((event & CYHAL_UART_IRQ_RX_ERROR) != (uint32_t)CYHAL_UART_IRQ_NONE))
    {
        p_callback = &_stm32_cyhal_uart_error_callback;
        if (enable)
        {
            obj->irq |= (uint32_t)event;
            (void)HAL_UART_RegisterCallback(obj->huart, HAL_UART_ERROR_CB_ID, p_callback);
        }
        else
        {
            obj->irq &= (uint32_t) ~event;
            (void)HAL_UART_UnRegisterCallback(obj->huart, HAL_UART_ERROR_CB_ID);
        }
    }

    /* Register CYHAL_UART_IRQ_RX_FULL callback */
    if ((event & CYHAL_UART_IRQ_RX_FULL) != (uint32_t)CYHAL_UART_IRQ_NONE)
    {
        p_callback = &_stm32_cyhal_uart_rx_fifo_full_callback;
        if (enable)
        {
            obj->irq |= (uint32_t)CYHAL_UART_IRQ_RX_FULL;
            (void)HAL_UART_RegisterCallback(obj->huart, HAL_UART_RX_FIFO_FULL_CB_ID, p_callback);
        }
        else
        {
            obj->irq &= (uint32_t) ~CYHAL_UART_IRQ_RX_FULL;
            (void)HAL_UART_UnRegisterCallback(obj->huart, HAL_UART_RX_FIFO_FULL_CB_ID);
        }
    }

    /* Register / un-register CYHAL_UART_IRQ_TX_EMPTY callback */
    if ((event & CYHAL_UART_IRQ_TX_EMPTY) != (uint32_t)CYHAL_UART_IRQ_NONE)
    {
        p_callback = &_stm32_cyhal_uart_tx_fifo_empty_callback;

        if (enable)
        {
            obj->irq |= (uint32_t)CYHAL_UART_IRQ_TX_EMPTY;
            (void)HAL_UART_RegisterCallback(obj->huart, HAL_UART_TX_FIFO_EMPTY_CB_ID, p_callback);
        }
        else
        {
            obj->irq &= (uint32_t) ~CYHAL_UART_IRQ_TX_EMPTY;
            (void)HAL_UART_UnRegisterCallback(obj->huart, HAL_UART_TX_FIFO_EMPTY_CB_ID);
        }
    }

    /* Register / un-register CYHAL_UART_IRQ_RX_NOT_EMPTY callback */
    if ((event & CYHAL_UART_IRQ_RX_NOT_EMPTY) != (uint32_t)CYHAL_UART_IRQ_NONE)
    {
        /* Enter UART critical section (disable UART interrupt if it is enabled) */
        _stm32_cyhal_uart_critical_section_enter(obj);

        if (enable)
        {
            obj->irq |= (uint32_t)CYHAL_UART_IRQ_RX_NOT_EMPTY;
            /* Update the RxISR function pointer. Set own RX handler to catch
             * IRQ_RX_NOT_EMPTY interrupt */
            obj->huart->RxISR = _stm32_cyhal_uart_rx_not_empty_irq_handler;

            /* Enable the Data Register Not Empty interrupt */
            SET_BIT(obj->huart->Instance->CR1, USART_CR1_RXNEIE_RXFNEIE);
        }
        else
        {
            obj->irq &= (uint32_t) ~CYHAL_UART_IRQ_RX_NOT_EMPTY;
            CLEAR_BIT(obj->huart->Instance->CR1, USART_CR1_RXNEIE_RXFNEIE);
        }

        /* Exit UART critical section */
        _stm32_cyhal_uart_critical_section_exit(obj);
    }

    /* Configure uart interrupt. Do not disable interrupt if we have have some
     * enabled  obj->irq */
    if ((enable) || (obj->irq == 0U))
    {
        _stm32_cyhal_uart_enable_irq(obj->huart->Instance, intr_priority, enable);
    }

    #else /* if (USE_HAL_UART_REGISTER_CALLBACKS == 1) */
    /* NOTE: USE_HAL_UART_REGISTER_CALLBACKS must be enabled */
    assert_param(false);
    #endif /*  (USE_HAL_UART_REGISTER_CALLBACKS == 1) */
}


/***************************************************************************************************
 * Private functions
 **************************************************************************************************/

/***************************************************************************************************
 * _stm32_cyhal_uart_convert_parity
 **************************************************************************************************/
static uint32_t _stm32_cyhal_uart_convert_parity(cyhal_uart_parity_t parity)
{
    uint32_t ret = 0xFFFFu;

    switch (parity)
    {
        case CYHAL_UART_PARITY_NONE:
            ret = UART_PARITY_NONE;
            break;

        case CYHAL_UART_PARITY_EVEN:
            ret = UART_PARITY_EVEN;
            break;

        case CYHAL_UART_PARITY_ODD:
            ret = UART_PARITY_ODD;
            break;

        default:
            assert_param(false); /* wrong value */
            break;
    }

    return (ret);
}


/***************************************************************************************************
 * _stm32_cyhal_uart_convert_stopbits
 **************************************************************************************************/
static uint32_t _stm32_cyhal_uart_convert_stopbits(uint32_t stopbits)
{
    uint32_t ret = 0xFFFFu;

    switch (stopbits)
    {
        case 1U:
            ret = UART_STOPBITS_1;
            break;

        case 2U:
            ret = UART_STOPBITS_2;
            break;

        default:
            assert_param(false); /* wrong value */
            break;
    }

    return (ret);
}


/***************************************************************************************************
 * _stm32_cyhal_uart_convert_wordlength
 **************************************************************************************************/
static uint32_t _stm32_cyhal_uart_convert_wordlength(uint32_t wordlength)
{
    uint32_t ret = 0xFFFFu;

    switch (wordlength)
    {
        case 7U:
            ret = UART_WORDLENGTH_7B;
            break;

        case 8U:
            ret = UART_WORDLENGTH_8B;
            break;

        case 9U:
            ret = UART_WORDLENGTH_9B;
            break;

        default:
            assert_param(false); /* wrong value */
            break;
    }

    return (ret);
}


/***************************************************************************************************
 * _stm32_cyhal_uart_get_irqn
 **************************************************************************************************/
static IRQn_Type _stm32_cyhal_uart_get_irqn(USART_TypeDef* instance)
{
    IRQn_Type IRQn = (IRQn_Type)0;

    #if defined (LPUART1)
    if (instance == LPUART1)
    {
        IRQn = LPUART1_IRQn;
    }
    else
    #endif

    #if defined (USART1)
    if (instance == USART1)
    {
        IRQn = USART1_IRQn;
    }
    else
    #endif

    #if defined (USART2)
    if (instance == USART2)
    {
        IRQn = USART2_IRQn;
    }
    else
    #endif

    #if defined (USART3)
    if (instance == USART3)
    {
        IRQn = USART3_IRQn;
    }
    else
    #endif

    #if defined (UART4)
    if (instance == UART4)
    {
        IRQn = UART4_IRQn;
    }
    else
    #endif

    #if defined (UART5)
    if (instance == UART5)
    {
        IRQn = UART5_IRQn;
    }
    else
    #endif

    #if defined (USART6)
    if (instance == USART6)
    {
        IRQn = USART6_IRQn;
    }
    else
    #endif

    #if defined (UART7)
    if (instance == UART7)
    {
        IRQn = UART7_IRQn;
    }
    else
    #endif

    #if defined (UART8)
    if (instance == UART8)
    {
        IRQn = UART8_IRQn;
    }
    else
    #endif
    {
        assert_param(false); /* wrong instance */
    }

    return (IRQn);
}


/***************************************************************************************************
 * _stm32_cyhal_uart_get_dma_irqn
 **************************************************************************************************/
static IRQn_Type _stm32_cyhal_uart_get_dma_irqn(DMA_HandleTypeDef* hdma)
{
    IRQn_Type IRQn = (IRQn_Type)0;

    #if defined (DMA1_Stream0)
    if (hdma->Instance == DMA1_Stream0)
    {
        IRQn = DMA1_Stream0_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream1)
    if (hdma->Instance == DMA1_Stream1)
    {
        IRQn = DMA1_Stream1_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream2)
    if (hdma->Instance == DMA1_Stream2)
    {
        IRQn = DMA1_Stream2_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream3)
    if (hdma->Instance == DMA1_Stream3)
    {
        IRQn = DMA1_Stream3_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream4)
    if (hdma->Instance == DMA1_Stream4)
    {
        IRQn = DMA1_Stream4_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream5)
    if (hdma->Instance == DMA1_Stream5)
    {
        IRQn = DMA1_Stream5_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream6)
    if (hdma->Instance == DMA1_Stream6)
    {
        IRQn = DMA1_Stream6_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Stream7)
    if (hdma->Instance == DMA1_Stream7)
    {
        IRQn = DMA1_Stream7_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream0)
    if (hdma->Instance == DMA2_Stream0)
    {
        IRQn = DMA2_Stream0_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream1)
    if (hdma->Instance == DMA2_Stream1)
    {
        IRQn = DMA2_Stream1_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream2)
    if (hdma->Instance == DMA2_Stream2)
    {
        IRQn = DMA2_Stream2_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream3)
    if (hdma->Instance == DMA2_Stream3)
    {
        IRQn = DMA2_Stream3_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream4)
    if (hdma->Instance == DMA2_Stream4)
    {
        IRQn = DMA2_Stream4_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream5)
    if (hdma->Instance == DMA2_Stream5)
    {
        IRQn = DMA2_Stream5_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream6)
    if (hdma->Instance == DMA2_Stream6)
    {
        IRQn = DMA2_Stream6_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Stream7)
    if (hdma->Instance == DMA2_Stream7)
    {
        IRQn = DMA2_Stream7_IRQn;
    }
    else
    #endif

    /* STM32L5 uses Channel1..8 instead Stream0..7 */
    #if defined (DMA1_Channel1)
    if (hdma->Instance == DMA1_Channel1)
    {
        IRQn = DMA1_Channel1_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel2)
    if (hdma->Instance == DMA1_Channel2)
    {
        IRQn = DMA1_Channel2_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel3)
    if (hdma->Instance == DMA1_Channel3)
    {
        IRQn = DMA1_Channel3_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel4)
    if (hdma->Instance == DMA1_Channel4)
    {
        IRQn = DMA1_Channel4_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel5)
    if (hdma->Instance == DMA1_Channel5)
    {
        IRQn = DMA1_Channel5_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel6)
    if (hdma->Instance == DMA1_Channel6)
    {
        IRQn = DMA1_Channel6_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel7)
    if (hdma->Instance == DMA1_Channel7)
    {
        IRQn = DMA1_Channel7_IRQn;
    }
    else
    #endif

    #if defined (DMA1_Channel8)
    if (hdma->Instance == DMA1_Channel8)
    {
        IRQn = DMA1_Channel8_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel1)
    if (hdma->Instance == DMA2_Channel1)
    {
        IRQn = DMA2_Channel1_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel2)
    if (hdma->Instance == DMA2_Channel2)
    {
        IRQn = DMA2_Channel2_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel3)
    if (hdma->Instance == DMA2_Channel3)
    {
        IRQn = DMA2_Channel3_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel4)
    if (hdma->Instance == DMA2_Channel4)
    {
        IRQn = DMA2_Channel4_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel5)
    if (hdma->Instance == DMA2_Channel5)
    {
        IRQn = DMA2_Channel5_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel6)
    if (hdma->Instance == DMA2_Channel6)
    {
        IRQn = DMA2_Channel6_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel7)
    if (hdma->Instance == DMA2_Channel7)
    {
        IRQn = DMA2_Channel7_IRQn;
    }
    else
    #endif

    #if defined (DMA2_Channel8)
    if (hdma->Instance == DMA2_Channel8)
    {
        IRQn = DMA2_Channel8_IRQn;
    }
    else
    #endif

    /* STM32U5 uses GPDMA Channels */
    #if defined (GPDMA1_Channel0)
    if (hdma->Instance == GPDMA1_Channel0)
    {
        IRQn = GPDMA1_Channel0_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel1)
    if (hdma->Instance == GPDMA1_Channel1)
    {
        IRQn = GPDMA1_Channel1_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel2)
    if (hdma->Instance == GPDMA1_Channel2)
    {
        IRQn = GPDMA1_Channel2_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel3)
    if (hdma->Instance == GPDMA1_Channel3)
    {
        IRQn = GPDMA1_Channel3_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel4)
    if (hdma->Instance == GPDMA1_Channel4)
    {
        IRQn = GPDMA1_Channel4_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel5)
    if (hdma->Instance == GPDMA1_Channel5)
    {
        IRQn = GPDMA1_Channel5_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel6)
    if (hdma->Instance == GPDMA1_Channel6)
    {
        IRQn = GPDMA1_Channel6_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel7)
    if (hdma->Instance == GPDMA1_Channel7)
    {
        IRQn = GPDMA1_Channel7_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel8)
    if (hdma->Instance == GPDMA1_Channel8)
    {
        IRQn = GPDMA1_Channel8_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel9)
    if (hdma->Instance == GPDMA1_Channel9)
    {
        IRQn = GPDMA1_Channel9_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel10)
    if (hdma->Instance == GPDMA1_Channel10)
    {
        IRQn = GPDMA1_Channel10_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel11)
    if (hdma->Instance == GPDMA1_Channel11)
    {
        IRQn = GPDMA1_Channel11_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel12)
    if (hdma->Instance == GPDMA1_Channel12)
    {
        IRQn = GPDMA1_Channel12_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel13)
    if (hdma->Instance == GPDMA1_Channel13)
    {
        IRQn = GPDMA1_Channel13_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel14)
    if (hdma->Instance == GPDMA1_Channel14)
    {
        IRQn = GPDMA1_Channel14_IRQn;
    }
    else
    #endif

    #if defined (GPDMA1_Channel15)
    if (hdma->Instance == GPDMA1_Channel15)
    {
        IRQn = GPDMA1_Channel15_IRQn;
    }
    else
    #endif

    #if defined (LPDMA1_Channel0)
    if (hdma->Instance == LPDMA1_Channel0)
    {
        IRQn = LPDMA1_Channel0_IRQn;
    }
    else
    #endif

    #if defined (LPDMA1_Channel1)
    if (hdma->Instance == LPDMA1_Channel1)
    {
        IRQn = LPDMA1_Channel1_IRQn;
    }
    else
    #endif

    #if defined (LPDMA1_Channel2)
    if (hdma->Instance == LPDMA1_Channel1)
    {
        IRQn = LPDMA1_Channel2_IRQn;
    }
    else
    #endif

    #if defined (LPDMA1_Channel3)
    if (hdma->Instance == LPDMA1_Channel3)
    {
        IRQn = LPDMA1_Channel3_IRQn;
    }
    else
    #endif

    {
        assert_param(false); /* wrong instance */
    }

    return (IRQn);
}


/***************************************************************************************************
 * _stm32_cyhal_uart_enable_irq
 **************************************************************************************************/
static void _stm32_cyhal_uart_enable_irq(USART_TypeDef* instance, uint32_t priority, bool enable)
{
    /* Use the priority, configured by STM32CubeMx if CYHAL_UART_USE_HAL_IRQ_PRIOPITY
     * has not been defined */
    #if !defined(CYHAL_UART_USE_HAL_IRQ_PRIOPITY)
    (void)priority;
    #endif /* !defined(CYHAL_UART_USE_HAL_IRQ_PRIOPITY) */

    IRQn_Type IRQn = _stm32_cyhal_uart_get_irqn(instance);

    if (enable)
    {
        /* Use the priority, configured by STM32CubeMx if CYHAL_UART_USE_HAL_IRQ_PRIOPITY
         * has not been defined */
        #if defined(CYHAL_UART_USE_HAL_IRQ_PRIOPITY)
        HAL_NVIC_SetPriority(IRQn, priority, 0);
        #endif /* defined(CYHAL_UART_USE_HAL_IRQ_PRIOPITY) */
        HAL_NVIC_EnableIRQ(IRQn);
    }
    else
    {
        HAL_NVIC_DisableIRQ(IRQn);
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_critical_section_enter
 **************************************************************************************************/
static void _stm32_cyhal_uart_critical_section_enter(cyhal_uart_t* obj)
{
    /* Force disable UART IRQ */
    HAL_NVIC_DisableIRQ(_stm32_cyhal_uart_get_irqn(obj->huart->Instance));

    /* Disable DMA Stream/Channel IRQ (TX) */
    if (obj->huart->hdmatx != NULL)
    {
        HAL_NVIC_DisableIRQ(_stm32_cyhal_uart_get_dma_irqn(obj->huart->hdmatx));
    }

    /* Disable DMA Stream/Channel IRQ (RX) */
    if (obj->huart->hdmarx != NULL)
    {
        HAL_NVIC_DisableIRQ(_stm32_cyhal_uart_get_dma_irqn(obj->huart->hdmarx));
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_critical_section_exit
 **************************************************************************************************/
static void _stm32_cyhal_uart_critical_section_exit(cyhal_uart_t* obj)
{
    IRQn_Type IRQn = _stm32_cyhal_uart_get_irqn(obj->huart->Instance);

    /* Enable UART IRQ if enabled any events  */
    if (obj->irq != 0u)
    {
        HAL_NVIC_EnableIRQ(IRQn);
    }

    /* Disable DMA Stream/Channel IRQ (TX) */
    if (obj->huart->hdmatx != NULL)
    {
        HAL_NVIC_EnableIRQ(_stm32_cyhal_uart_get_dma_irqn(obj->huart->hdmatx));
    }

    /* Disable DMA Stream/Channel IRQ (RX) */
    if (obj->huart->hdmarx != NULL)
    {
        HAL_NVIC_EnableIRQ(_stm32_cyhal_uart_get_dma_irqn(obj->huart->hdmarx));
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_alloc_hw
 **************************************************************************************************/
static UART_HandleTypeDef* _stm32_cyhal_uart_alloc_hw(cyhal_uart_t* obj, cyhal_gpio_t tx,
                                                      cyhal_gpio_t rx)
{
    UART_HandleTypeDef* handle = NULL;

    /* Go through _cyhal_uart_structs and find uart handle by TX/RX pins */
    for (uint32_t i = 0u; i < CYHAL_UART_MAX_INSTANCES; i++)
    {
        #if (CYHAL_UART_MAX_INSTANCES > 1)
        if (((tx != NC) && (tx == _cyhal_uart_structs[i].associate_pin)) ||
            ((rx != NC) && (rx == _cyhal_uart_structs[i].associate_pin)))
        #else
        (void)tx;
        (void)rx;
        #endif /* (CYHAL_UART_MAX_INSTANCES > 1) */
        {
            if (_cyhal_uart_structs[i].occupied_obj == NULL)
            {
                handle                              =  _cyhal_uart_structs[i].huart;
                _cyhal_uart_structs[i].occupied_obj = obj;
                break;
            }
        }
    }
    return (handle);
}


/***************************************************************************************************
 * _stm32_cyhal_uart_free_hw
 **************************************************************************************************/
static void _stm32_cyhal_uart_free_hw(cyhal_uart_t* obj)
{
    /* Go through _cyhal_uart_structs and find uart handle by TX/RX pins */
    for (uint32_t i = 0u; i < CYHAL_UART_MAX_INSTANCES; i++)
    {
        if (obj->huart == _cyhal_uart_structs[i].huart)
        {
            _cyhal_uart_structs[i].occupied_obj = NULL;
            break;
        }
    }
}


/***************************************************************************************************
 * _stm32_cyhal_uart_get_obj
 **************************************************************************************************/
#if (USE_HAL_UART_REGISTER_CALLBACKS == 1) || (USE_HAL_USART_REGISTER_CALLBACKS == 1)
static cyhal_uart_t* _stm32_cyhal_uart_get_obj(UART_HandleTypeDef* huart)
{
    for (uint32_t i = 0u; i < CYHAL_UART_MAX_INSTANCES; i++)
    {
        if (_cyhal_uart_structs[i].occupied_obj->huart == huart)
        {
            return (_cyhal_uart_structs[i].occupied_obj);
        }
    }
    return NULL;
}


#endif /* (USE_HAL_UART_REGISTER_CALLBACKS == 1) */


/***************************************************************************************************
 * _stm32_cyhal_uart_get_struct
 **************************************************************************************************/
#if (CYHAL_UART_USE_RTOS_MUTEX == 1)
static stm32_cyhal_uart_structs_t* _stm32_cyhal_uart_get_struct(cyhal_uart_t* obj)
{
    for (uint32_t i = 0u; i < CYHAL_UART_MAX_INSTANCES; i++)
    {
        if (_cyhal_uart_structs[i].occupied_obj == obj)
        {
            return (&_cyhal_uart_structs[i]);
        }
    }
    return NULL;
}


#endif /* (CYHAL_UART_USE_RTOS_MUTEX == 1) */


/***************************************************************************************************
 * stm32_cypal_uart_hw_init
 **************************************************************************************************/
uint32_t stm32_cypal_uart_hw_init(UART_HandleTypeDef* huart, cyhal_gpio_t associate_pin)
{
    for (uint32_t i = 0u; i < CYHAL_UART_MAX_INSTANCES; i++)
    {
        if (_cyhal_uart_structs[i].huart == NULL)
        {
            _cyhal_uart_structs[i].huart         = huart;
            _cyhal_uart_structs[i].associate_pin = associate_pin;
            return 0u;
        }
    }

    /* Failed to allocate memory for the UART instance. CYHAL_UART_MAX_INSTANCES
     * defines the max number of UART instances (default = 1).
     * Overwrite CYHAL_UART_MAX_INSTANCES in cybsp.h to increase the number of
     * supported UART instances */
    assert_param(false);
    return 1u;
}


/***************************************************************************************************
 * cyhal_uart_readable
 *
 * NOTE: This only returns 0 or 1, which is all that is needed at this time.
 *       But this function is supposed to return the total number of characters available.
 *       This implimentation provides the functionality needed by the command-console,
 *       which is the only library that uses this function.
 **************************************************************************************************/
uint32_t cyhal_uart_readable(cyhal_uart_t* obj)
{
    /* Check the parameters */
    assert_param(NULL != obj);

    return (uint32_t)__HAL_UART_GET_FLAG(obj->huart, UART_FLAG_RXNE);
}


/***************************************************************************************************
 * cyhal_uart_getc
 **************************************************************************************************/
cy_rslt_t cyhal_uart_getc(cyhal_uart_t* obj, uint8_t* value, uint32_t timeout)
{
    uint32_t uart_rx_timeout = timeout;
    HAL_StatusTypeDef status;

    /* Check the parameters */
    assert_param(NULL != obj);
    assert_param(NULL != value);

    /* The "wait forever" case is different for the STM32 function */
    if (uart_rx_timeout == 0)
    {
        uart_rx_timeout = HAL_MAX_DELAY;
    }

    /* Enter UART critical section (disable UART interrupt if it is enabled) */
    _stm32_cyhal_uart_critical_section_enter(obj);

    /* Receive an amount of data in blocking mode */
    status = HAL_UART_Receive(obj->huart, value, 1, uart_rx_timeout);

    /* Exit UART critical section */
    _stm32_cyhal_uart_critical_section_exit(obj);

    return (status == HAL_OK) ? CY_RSLT_SUCCESS : CYHAL_UART_RSLT_ERR_HAL_ERROR;
}


#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* defined(HAL_UART_MODULE_ENABLED) */
