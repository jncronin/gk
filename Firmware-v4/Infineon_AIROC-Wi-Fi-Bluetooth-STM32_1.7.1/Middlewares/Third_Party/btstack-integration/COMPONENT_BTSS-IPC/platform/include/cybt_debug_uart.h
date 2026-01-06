/*******************************************************************************
* \file cybt_debug_uart.h

*
* \brief
* Provides API to access Debug transport.
*
********************************************************************************
* \copyright
* Copyright 2018-2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#include "cybt_result.h"
#include "wiced_bt_dev.h"
/**
 *  @addtogroup    debug_uart_cfg   Debug UART Configuration
 *
 * The BLESS UART-specific configurations, including hardware pin assignment.
 * This debug UART is used for communication between the PSOC and host connected via USB cable.
 *  @{
 */


/**
 * Received data handler callback type
 *
 * @param[in] p_data   : received data pointer
 * @param[in] data_len : data length
 *
 */
typedef uint32_t (*cybt_debug_uart_data_handler_t)( uint8_t* p_data, uint32_t data_len );

/** Debug Uart Configuration */
typedef struct
{
    cyhal_gpio_t         uart_tx_pin;  /**< Uart TXD pin */
    cyhal_gpio_t         uart_rx_pin;  /**< Uart RXD pin */
    cyhal_gpio_t         uart_rts_pin;  /**< Uart RTS pin */
    cyhal_gpio_t         uart_cts_pin;  /**< Uart CTS pin */

    uint32_t             baud_rate;     /**< Uart baud rate */

    bool                 flow_control;  /**< flow control status */
} cybt_debug_uart_config_t;
/**
 * Initialize Debug UART. 
 * This debug UART is used for communication between the PSOC and host connected via USB cable.
 *
 * @param[in] config       : uart configuration
 * @param[in] p_data_handler  : received data handler callback pointer
 *
 * @returns  CYBT_SUCCESS if success else error reason.
 *
 * @note : Debug UART Must be initialized to send traces over Debug UART.
 */
cybt_result_t cybt_debug_uart_init(cybt_debug_uart_config_t* config, cybt_debug_uart_data_handler_t p_data_handler);

/**
 * Sends traces over Debug UART
 *
 * @param[in] length: Length of the data
 * @param[in] p_data: data pointer
 *
 * @returns  CYBT_SUCCESS if success else error reason.
 *
 */
cybt_result_t cybt_debug_uart_send_trace (uint16_t length, uint8_t* p_data);

/**
 * Sends HCI traces over Debug UART
 *
 * @param[in] type  : Trace data type (refer wiced_bt_hci_trace_type_t in wiced_bt_dev.h)
 * @param[in] length: Length of the data
 * @param[in] p_data: data pointer
 *
 * @returns  CYBT_SUCCESS if success else error reason.
 *
 */
cybt_result_t cybt_debug_uart_send_hci_trace (uint8_t type, uint16_t length, uint8_t* p_data);

/**
 * Sends data over Debug UART
 *
 * @param[in] opcode  : Opcode
 * @param[in] length: Length of the data
 * @param[in] p_data: data pointer
 *
 * @returns  CYBT_SUCCESS if success else error reason.
 *
 * @note This can be used from register callback of wiced_bt_dev_register_hci_trace function.
 */
cybt_result_t cybt_debug_uart_send_data (uint16_t opcode, uint16_t data_size, uint8_t *p_data);

/**
 * Sends coredump HCI traces over Debug UART
 *
 * @param[in] length: Length of the data
 * @param[in] p_data: data pointer
 *
 * @returns  CYBT_SUCCESS if success else error reason.
 */
cybt_result_t cybt_send_coredump_hci_trace (uint16_t data_size, uint8_t *p_data);

/**@} */

