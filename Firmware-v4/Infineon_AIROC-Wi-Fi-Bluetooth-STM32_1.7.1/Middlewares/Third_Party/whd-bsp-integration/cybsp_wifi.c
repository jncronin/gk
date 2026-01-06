/***********************************************************************************************//**
 * \file cybsp_wifi.c
 *
 * \brief
 * Provides utility functions that are used by board support packages.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2018-2022 Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation
 *
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
 **************************************************************************************************/

#include "cybsp.h"
#include "cybsp_wifi.h"
#include "cy_network_buffer.h"
#include "cyabs_rtos.h"
#include "whd_types.h"
#include "cyhal.h"

#ifdef COMPONENT_CAT5
#include "whd_int.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * \addtogroup group_bsp_wifi
 * \{
 * Macros to abstract out whether the LEDs & Buttons are wired high or active low.
 */

/** Defines the amount of stack memory available for the wifi thread. */
#if !defined(CY_WIFI_THREAD_STACK_SIZE)
#define CY_WIFI_THREAD_STACK_SIZE           (5120)
#endif

/** Defines the priority of the thread that services wifi packets. Legal values are defined by the
 *  RTOS being used.
 */
#if !defined(CY_WIFI_THREAD_PRIORITY)
#ifndef COMPONENT_CAT5
#define CY_WIFI_THREAD_PRIORITY             (CY_RTOS_PRIORITY_HIGH)
#else
#define CY_WIFI_THREAD_PRIORITY             (CY_RTOS_PRIORITY_MAX)
#endif /* WHD_TO_TEST */
#endif

/** Defines the country this will operate in for wifi initialization parameters. See the
 *  wifi-host-driver's whd_country_code_t for legal options.
 */
#if !defined(CY_WIFI_COUNTRY)
#define CY_WIFI_COUNTRY                     (WHD_COUNTRY_AUSTRALIA)
#endif

/** Defines the priority of the interrupt that handles out-of-band notifications from the wifi
 *  chip. Legal values are defined by the MCU running this code.
 */
#if !defined(CY_WIFI_OOB_INTR_PRIORITY)
    #define CY_WIFI_OOB_INTR_PRIORITY       (2)
#endif

/** Defines whether to use the out-of-band pin to allow the WIFI chip to wake up the MCU. */
#if defined(CY_WIFI_HOST_WAKE_SW_FORCE)
    #define CY_USE_OOB_INTR                 (CY_WIFI_HOST_WAKE_SW_FORCE)
#else
    #define CY_USE_OOB_INTR                 (1u)
#endif // defined(CY_WIFI_HOST_WAKE_SW_FORCE)

/** Default GPIO drive mode for CYBSP_WIFI_WL_REG_ON. Override with Makefile define  */
#ifndef CYBSP_WIFI_WL_REG_ON_GPIO_DRIVE_MODE
#define CYBSP_WIFI_WL_REG_ON_GPIO_DRIVE_MODE    (CYHAL_GPIO_DRIVE_PULLUP)
#endif

/** Default GPIO drive mode for CYBSP_WIFI_WL_HOSTWAKE. Override with Makefile define  */
#ifndef CYBSP_WIFI_WL_HOSTWAKE_DRIVE_MODE
#define CYBSP_WIFI_WL_HOSTWAKE_DRIVE_MODE        (CYHAL_GPIO_DRIVE_NONE)
#endif

/** Default GPIO drive mode for CYBSP_WIFI_WL_HOSTWAKE. Override with Makefile define  */
#ifndef CYBSP_WIFI_WL_HOSTWAKE_INIT_STATE
#define CYBSP_WIFI_WL_HOSTWAKE_INIT_STATE       (WHD_FALSE)
#endif


/** \} group_bsp_wifi */

#define DEFAULT_OOB_PIN                     (0)
#define WLAN_POWER_UP_DELAY_MS              (250)
#define WLAN_CBUCK_DISCHARGE_MS             (10)

#if defined(COMPONENT_WIFI_INTERFACE_SDIO)

// Old versions of the BSP performed WIFI SDIO init as part of cybsp_init() this has been replaced
// with just minimal resource reservation when needed in the latest versions of BSPs.
#define CYBSP_WIFI_SDIO_NEEDS_INIT          (1)

#elif (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE)
  #define COMPONENT_WIFI_INTERFACE_USB
#else

#if defined(CYBSP_WIFI_INTERFACE_TYPE) && (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
  #if !defined(COMPONENT_WIFI_INTERFACE_SDIO)
    #define COMPONENT_WIFI_INTERFACE_SDIO
  #endif
#elif defined(CYBSP_WIFI_INTERFACE_TYPE) && (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)
  #if !defined(COMPONENT_WIFI_INTERFACE_SPI)
    #define COMPONENT_WIFI_INTERFACE_SPI
  #endif
#elif defined(CYBSP_WIFI_INTERFACE_TYPE) && (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_M2M_INTERFACE)
  #if !defined(COMPONENT_WIFI_INTERFACE_M2M)
    #define COMPONENT_WIFI_INTERFACE_M2M
  #endif
#elif !defined(COMPONENT_WIFI_INTERFACE_OCI)
  // For old versions of HAL/BSP fallback to the default interface
  #define COMPONENT_WIFI_INTERFACE_SDIO
#endif // if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)

#endif // if defined(COMPONENT_WIFI_INTERFACE_SDIO)

#if !(defined(COMPONENT_WIFI_INTERFACE_M2M) || defined(COMPONENT_WIFI_INTERFACE_OCI))

#define SDIO_ENUMERATION_TRIES              (500)
#define SDIO_RETRY_DELAY_MS                 (1)
#define SDIO_BUS_LEVEL_MAX_RETRIES          (5)

#define SDIO_FREQ_25MHZ                     (25000000)
#define SDIO_FREQ_50MHZ                     (50000000)

// Switching to 1.8 Request / Switching to 1.8V Accepted
#define SDIO_CMD5_S18R_BIT                  (1UL << 24)
// Number of I/O functions position in CMD5 response (R4)
#define SDIO_CMD5_RESP_IO_FUNC_POS          (28)
// Mask of I/O Number in CMD5 response (R4). This field is 3-bit wide
#define SDIO_CMD5_RESP_IO_FUNC_MSK          (0x7)
// The IO OCR register mask in CMD5 request / response (R4)
#define SDIO_CMD5_IO_OCR_MSK                (0x00FFFFFFUL)
// C bit in CMD5 response. Set to 1 if Card is ready to operate after initialization
#define SDIO_CMD5_RESP_IORDY                (1UL << 31)

// Per SDIO Specification - IO_RW_DIRECT Command (CMD52)

// This bit determines the direction of the I/O operation. If this bit is 0, this command shall read
// data from the SDIO
#define SDIO_CMD52_ARG_RW_READ              (0)
#define SDIO_CMD52_ARG_RW_WRITE             (1)
// The Read after Write flag. If this bit is set to 1 and the R/W flag is set to 1, then the command
// shall read the value of the register after the write
#define SDIO_CMD52_ARG_RAW_NOT_SET          (0)
#define SDIO_CMD52_ARG_RAW_SET              (1)
// The number of the function within the I/O card you wish to read or write. Function 0 selects the
// common I/O area (CIA)
#define SDIO_FUNC_NUM_0                     (0)

// Per SDIO Specification -Card Common Control Registers (CCCR)

// Bus Speed Select register
#define SDIO_CMD52_CCCR_SPEED_SLCT_ADDR     (0x00013)
// Select High Speed by setting BSS0 bit
#define SDIO_CMD52_CCCR_SPEED_SLCT_HS       (0x2)

// Per SDIO specification - IO_RW_DIRECT Response (R5)

// | Start | Dir | CMD Index | Stuff | Response Flags Bit | Read or Write Data | CRC7 | End |
// ------------------------------------------------------------------------------------------
// |   1   |  1  |     6     |  16   |          8         |         8          |  7   |  1  |

// Expected response for High Speed support check command
//                                   | CMD=DAT lines free | High-Speed support |
//                                   |     00010000b      |     00000001b      |
#define SDIO_CMD52_CCCR_SPEED_SELECT_RESP_HS_SUPPORTED  (0x00001001)
// Expected response for High Speed support check command
//                                   | CMD=DAT lines free | HS/SDR25 activated |
//                                   |     00010000b      |     00000010b      |
#define SDIO_CMD52_CCCR_SPEED_SELECT_RESP_HS_SELECTED   (0x00001002)

// *SUSPEND-FORMATTING*
#if (CY_USE_OOB_INTR != 0)
    // Setup configuration based on configurator or BSP, where configurator has precedence.
    #if defined(CYCFG_WIFI_HOST_WAKE_ENABLED)
        #define CY_WIFI_HOST_WAKE_GPIO  CYCFG_WIFI_HOST_WAKE_GPIO
        #define CY_WIFI_HOST_WAKE_IRQ_EVENT CYCFG_WIFI_HOST_WAKE_IRQ_EVENT
    #else
        // Setup host-wake pin
        #if defined(CYBSP_WIFI_HOST_WAKE)
            #define CY_WIFI_HOST_WAKE_GPIO CYBSP_WIFI_HOST_WAKE
        #else
            #error "CYBSP_WIFI_HOST_WAKE must be defined"
        #endif
        // Setup host-wake irq
        #if defined(CYBSP_WIFI_HOST_WAKE_IRQ_EVENT)
            #define CY_WIFI_HOST_WAKE_IRQ_EVENT CYBSP_WIFI_HOST_WAKE_IRQ_EVENT
        #else
            #error "CYBSP_WIFI_HOST_WAKE_IRQ_EVENT must be defined"
        #endif
    #endif // if defined(CYCFG_WIFI_HOST_WAKE_ENABLED)
#else // if (CY_USE_OOB_INTR != 0)
    #define CY_WIFI_HOST_WAKE_GPIO      CYHAL_NC_PIN_VALUE
    #define CY_WIFI_HOST_WAKE_IRQ_EVENT 0
#endif // (CY_USE_OOB_INTR != 0)
// *RESUME-FORMATTING*

// Add compatability for HAL 1.x
#if !defined(CYHAL_API_VERSION)
typedef cyhal_transfer_t            cyhal_sdio_transfer_type_t;
#define CYHAL_SDIO_XFER_TYPE_READ   CYHAL_READ
#define CYHAL_SDIO_XFER_TYPE_WRITE  CYHAL_WRITE
#endif

#endif // if !defined(COMPONENT_WIFI_INTERFACE_M2M)

static whd_driver_t whd_drv;

extern whd_resource_source_t resource_ops;

static whd_init_config_t init_config_default =
{
    .thread_stack_size  = CY_WIFI_THREAD_STACK_SIZE,
    .thread_stack_start = NULL,
    .thread_priority    = (uint32_t)CY_WIFI_THREAD_PRIORITY,
    .country            = CY_WIFI_COUNTRY
};

static whd_buffer_funcs_t buffer_if_default =
{
    .whd_host_buffer_get                       = cy_host_buffer_get,
    .whd_buffer_release                        = cy_buffer_release,
    .whd_buffer_get_current_piece_data_pointer = cy_buffer_get_current_piece_data_pointer,
    .whd_buffer_get_current_piece_size         = cy_buffer_get_current_piece_size,
    .whd_buffer_set_size                       = cy_buffer_set_size,
    .whd_buffer_add_remove_at_front            = cy_buffer_add_remove_at_front,
};

static whd_netif_funcs_t netif_if_default =
{
    .whd_network_process_ethernet_data = cy_network_process_ethernet_data,
};

#if !(defined(COMPONENT_WIFI_INTERFACE_M2M) || defined(COMPONENT_WIFI_INTERFACE_OCI))
#if !defined(COMPONENT_WIFI_INTERFACE_USB)
static const whd_oob_config_t OOB_CONFIG =
{
    .host_oob_pin      = CY_WIFI_HOST_WAKE_GPIO,
    .dev_gpio_sel      = DEFAULT_OOB_PIN,
    .is_falling_edge   = (CY_WIFI_HOST_WAKE_IRQ_EVENT == CYHAL_GPIO_IRQ_FALL)
        ? WHD_TRUE
        : WHD_FALSE,
    #if defined(WHD_OOB_CONFIG_VERSION) && ((WHD_OOB_CONFIG_VERSION) >= 2)
    .drive_mode        = CYBSP_WIFI_WL_HOSTWAKE_DRIVE_MODE,
    .init_drive_state  = CYBSP_WIFI_WL_HOSTWAKE_INIT_STATE,
    #endif
    .intr_priority     = CY_WIFI_OOB_INTR_PRIORITY
};
#endif /* !defined(COMPONENT_WIFI_INTERFACE_USB) */


//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_reset_wifi_chip
//--------------------------------------------------------------------------------------------------
static void _cybsp_wifi_reset_wifi_chip(void)
{
    // WiFi into reset
    // Allow CBUCK regulator to discharge
    (void)cyhal_system_delay_ms(WLAN_CBUCK_DISCHARGE_MS);
    // WiFi out of reset
    cyhal_gpio_write(CYBSP_WIFI_WL_REG_ON, true);
    (void)cyhal_system_delay_ms(WLAN_POWER_UP_DELAY_MS);
}


#endif // if !defined(COMPONENT_WIFI_INTERFACE_M2M)


#if defined(COMPONENT_WIFI_INTERFACE_SDIO)
//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_sdio_try_send_cmd
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_sdio_try_send_cmd(cyhal_sdio_t* sdio_object,
                                               cyhal_sdio_transfer_type_t direction,
                                               cyhal_sdio_command_t command, uint32_t argument,
                                               uint32_t* response)
{
    uint8_t   loop_count = 0;
    cy_rslt_t result     = CYBSP_RSLT_WIFI_SDIO_ENUM_TIMEOUT;
    do
    {
        result = cyhal_sdio_send_cmd(sdio_object, direction, command, argument, response);
        if (result != CY_RSLT_SUCCESS)
        {
            cyhal_system_delay_ms(SDIO_RETRY_DELAY_MS);
        }
        loop_count++;
    } while((result != CY_RSLT_SUCCESS) && (loop_count <= SDIO_BUS_LEVEL_MAX_RETRIES));
    return result;
}


#if !defined(CYHAL_UDB_SDIO)
//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_create_cmd_52_arg
//--------------------------------------------------------------------------------------------------
static uint32_t _cybsp_wifi_create_cmd_52_arg(uint8_t rw, uint8_t func, uint8_t raw, uint32_t addr,
                                              uint8_t data)
{
    return (((rw & 0x01) << 31)      /* set R/W flag */
            | ((func & 0x07) << 28)  /* set the function number */
            | ((raw & 0x01) << 27)   /* set the RAW flag */
            | ((addr & 0x1FFFF) << 9) /* set the address */
            | data);                 /* set the data */
}


#endif /* !defined(CYHAL_UDB_SDIO) */

//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_sdio_card_init
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_sdio_card_init(cyhal_sdio_t* sdio_object)
{
    cy_rslt_t result;
    uint32_t  loop_count = 0;
    uint32_t  rel_addr;

    uint32_t response       = 0;
    uint32_t no_argument    = 0;

    #if 0 //!defined(CYHAL_UDB_SDIO)
    uint32_t argument       = 0;
    uint32_t io_num         = 0;
    bool     abort_sdio     = false;
    #else
    const bool abort_sdio   = false;
    #endif /* !defined(CYHAL_UDB_SDIO) */

    do
    {
        // Send CMD0 to set it to idle state
        result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_WRITE,
                                               CYHAL_SDIO_CMD_GO_IDLE_STATE,
                                               no_argument, &response /*ignored*/);

        // CMD5.
        if (result == CY_RSLT_SUCCESS)
        {
            result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_READ,
                                                   CYHAL_SDIO_CMD_IO_SEND_OP_COND,
                                                   no_argument,
                                                   &response /*ignored on UDB-based SDIO*/);
        }

        // UDB-based SDIO does not support io volt switch sequence
        #if 0 // !defined(CYHAL_UDB_SDIO)
        if (result == CY_RSLT_SUCCESS)
        {
            // Check number of IO functions, that are supported by device
            io_num = (response >> SDIO_CMD5_RESP_IO_FUNC_POS) & SDIO_CMD5_RESP_IO_FUNC_MSK;
            if (io_num > 0)
            {
                // Sending 1.8V switch request
                argument = (response & SDIO_CMD5_IO_OCR_MSK) | SDIO_CMD5_S18R_BIT;

                // CMD5.
                result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_WRITE,
                                                       CYHAL_SDIO_CMD_IO_SEND_OP_COND,
                                                       argument, &response);

                if (CY_RSLT_SUCCESS == result)
                {
                    // IORDY = 1 (Card is ready to operate)
                    if (response & SDIO_CMD5_RESP_IORDY)
                    {
                        // Switching to 1.8V accepted
                        if (response & SDIO_CMD5_S18R_BIT)
                        {
                            // CMD11.
                            result = _cybsp_wifi_sdio_try_send_cmd(sdio_object,
                                                                   CYHAL_SDIO_XFER_TYPE_WRITE,
                                                                   CYHAL_SDIO_CMD_VOLTAGE_SWITCH,
                                                                   no_argument,
                                                                   &response);

                            if (CY_RSLT_SUCCESS == result)
                            {
                                #if defined(CYBSP_WIFI_SDIO_VOLT_SEL)
                                cyhal_gpio_t io_volt_sel_pin = CYBSP_WIFI_SDIO_VOLT_SEL;
                                #else
                                // No actual voltage switch will be done as no pin provided.
                                cyhal_gpio_t io_volt_sel_pin = NC;
                                #endif /* defined(CYBSP_WIFI_SDIO_VOLT_SEL) */

                                // Perform voltage switch sequence
                                // And, if pin provided, switch the voltage
                                result = cyhal_sdio_set_io_voltage(sdio_object, io_volt_sel_pin,
                                                                   CYHAL_SDIO_IO_VOLTAGE_1_8V,
                                                                   CYHAL_SDIO_IO_VOLT_ACTION_SWITCH_SEQ_ONLY);

                                if (CYHAL_SDIO_RSLT_ERR_UNSUPPORTED == result)
                                {
                                    // Changing IO voltage is not supported by current
                                    // implementation. No reason to try again.
                                    abort_sdio = true;
                                }
                            }
                        }
                        // Nothing to do for 'else'. 1.8V is not supported.
                    }
                    else
                    {
                        result = CYBSP_RSLT_WIFI_SDIO_ENUM_NOT_READY;
                    }
                }
            }
            else
            {
                result = CYBSP_RSLT_WIFI_SDIO_ENUM_IO_NOT_SUPPORTED;
                // IO is not supported by this SD device, no reason to try enumeration again
                abort_sdio = true;
            }
        }
        #endif /* !defined(CYHAL_UDB_SDIO) */

        if (CY_RSLT_SUCCESS == result)
        {
            // Send CMD3 to get RCA.
            result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_READ,
                                                   CYHAL_SDIO_CMD_SEND_RELATIVE_ADDR,
                                                   no_argument, &rel_addr);
        }

        if ((!abort_sdio) && (result != CY_RSLT_SUCCESS))
        {
            cyhal_system_delay_ms(SDIO_RETRY_DELAY_MS);
        }
        loop_count++;
    } while ((!abort_sdio) && (result != CY_RSLT_SUCCESS) &&
             (loop_count <= SDIO_ENUMERATION_TRIES));

    if (result == CY_RSLT_SUCCESS)
    {
        // Send CMD7 with the returned RCA to select the card
        result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_WRITE,
                                               CYHAL_SDIO_CMD_SELECT_CARD,
                                               rel_addr,
                                               &response /*ignored*/);
    }

    uint32_t sdio_frequency = SDIO_FREQ_25MHZ;

    // use constant 25 MHz frequency for UDB-based SDIO
    // and perform supported speed check and switch for SDHC-based SDIO
    #if !defined(CYHAL_UDB_SDIO)
    if (result == CY_RSLT_SUCCESS)
    {
        uint32_t tmp_arg = _cybsp_wifi_create_cmd_52_arg(SDIO_CMD52_ARG_RW_READ, SDIO_FUNC_NUM_0,
                                                         SDIO_CMD52_ARG_RAW_NOT_SET,
                                                         SDIO_CMD52_CCCR_SPEED_SLCT_ADDR, 0x00);
        result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_WRITE,
                                               CYHAL_SDIO_CMD_IO_RW_DIRECT,
                                               tmp_arg, &response);

        if (result == CY_RSLT_SUCCESS)
        {
            if (SDIO_CMD52_CCCR_SPEED_SELECT_RESP_HS_SUPPORTED == response)
            {
                tmp_arg = _cybsp_wifi_create_cmd_52_arg(SDIO_CMD52_ARG_RW_WRITE, SDIO_FUNC_NUM_0,
                                                        SDIO_CMD52_ARG_RAW_NOT_SET,
                                                        SDIO_CMD52_CCCR_SPEED_SLCT_ADDR,
                                                        SDIO_CMD52_CCCR_SPEED_SLCT_HS);
                result = _cybsp_wifi_sdio_try_send_cmd(sdio_object, CYHAL_SDIO_XFER_TYPE_WRITE,
                                                       CYHAL_SDIO_CMD_IO_RW_DIRECT,
                                                       tmp_arg, &response);

                if (result == CY_RSLT_SUCCESS)
                {
                    if (SDIO_CMD52_CCCR_SPEED_SELECT_RESP_HS_SELECTED == response)
                    {
                        // High Speed mode switch allowed, configure clock frequency for 50 MHz
                        sdio_frequency = SDIO_FREQ_50MHZ;
                    }
                }
                else
                {
                    result = CYBSP_RSLT_WIFI_SDIO_HS_SWITCH_FAILED;
                }
            }
        }
    }
    #endif /* !defined(CYHAL_UDB_SDIO) */

    if (result == CY_RSLT_SUCCESS)
    {
        cyhal_sdio_cfg_t config = { .frequencyhal_hz = sdio_frequency, .block_size = 0 };
        result = cyhal_sdio_configure(sdio_object, &config);
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_sdio_init_bus2
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_sdio_init_bus2(cyhal_sdio_t* obj)
{
    cy_rslt_t result = _cybsp_wifi_sdio_card_init(obj);
    if (result == CY_RSLT_SUCCESS)
    {
        // If the configurator reserved the pin, we need to release it here since
        // WHD will try to reserve it again. WHD has no idea about configurators
        // and expects it can reserve the pin that it is going to manage.
        #if defined(CYCFG_WIFI_HOST_WAKE_ENABLED)
        cyhal_resource_inst_t pinRsc = cyhal_utils_get_gpio_resource(CY_WIFI_HOST_WAKE_GPIO);
        cyhal_hwmgr_free(&pinRsc);
        #endif

        whd_sdio_config_t whd_sdio_config =
        {
            .sdio_1bit_mode        = WHD_FALSE,
            .high_speed_sdio_clock = WHD_FALSE,
            .oob_config            = OOB_CONFIG
        };
        whd_bus_sdio_attach(whd_drv, &whd_sdio_config, obj);
    }
    return result;
}


#if defined(CYBSP_WIFI_SDIO_NEEDS_INIT)
#if (CY_CPU_CORTEX_M55)
CY_SECTION_SHAREDMEM static cyhal_sdio_t sdio_obj;
#else
static cyhal_sdio_t sdio_obj;
#endif
#endif

//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_sdio_init_bus
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_sdio_init_bus(void)
{
    #if defined(CYBSP_WIFI_SDIO_NEEDS_INIT)
    cy_rslt_t result = cyhal_sdio_init(&sdio_obj, CYBSP_WIFI_SDIO_CMD, CYBSP_WIFI_SDIO_CLK,
                                       CYBSP_WIFI_SDIO_D0, CYBSP_WIFI_SDIO_D1, CYBSP_WIFI_SDIO_D2,
                                       CYBSP_WIFI_SDIO_D3);

    if (result == CY_RSLT_SUCCESS)
    {
        result = _cybsp_wifi_sdio_init_bus2(&sdio_obj);
        if (result != CY_RSLT_SUCCESS)
        {
            cyhal_sdio_free(&sdio_obj);
        }
    }
    #else // if defined(CYBSP_WIFI_SDIO_NEEDS_INIT)
    cyhal_sdio_t* sdio_p = cybsp_get_wifi_sdio_obj();
    cy_rslt_t result = _cybsp_wifi_sdio_init_bus2(sdio_p);
    #endif // if defined(CYBSP_WIFI_SDIO_NEEDS_INIT)

    return result;
}


#elif defined(COMPONENT_WIFI_INTERFACE_SPI)

//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_spi_init_bus
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_spi_init_bus(void)
{
    static cyhal_spi_t spi_obj;
    cy_rslt_t rslt = cyhal_spi_init(&spi_obj,
                                    CYBSP_WIFI_SPI_MOSI, CYBSP_WIFI_SPI_MISO, CYBSP_WIFI_SPI_SCLK,
                                    CYBSP_WIFI_SPI_SSEL,
                                    NULL, 32, CYHAL_SPI_MODE_00_MSB, false);
    if (CY_RSLT_SUCCESS == rslt)
    {
        rslt = cyhal_spi_set_frequency(&spi_obj, 50000000); // 50 MHz operation
        if (CY_RSLT_SUCCESS == rslt)
        {
            whd_spi_config_t whd_spi_config =
            {
                .is_normal_mode      = false,
                .oob_config          = OOB_CONFIG
            };
            rslt = whd_bus_spi_attach(whd_drv, &whd_spi_config, &spi_obj);
        }
        else
        {
            cyhal_spi_free(&spi_obj);
        }
    }

    return rslt;
}


#elif defined(COMPONENT_WIFI_INTERFACE_M2M)

static cyhal_m2m_t m2m_obj;

//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_m2m_init_bus
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_m2m_init_bus(void)
{
    cy_rslt_t rslt;

    whd_m2m_config_t whd_m2m_config =
    {
        .is_normal_mode = false,
    };
    // Note: The m2m itself is initialized in the WHD
    rslt = whd_bus_m2m_attach(whd_drv, &whd_m2m_config, &m2m_obj);

    return rslt;
}


#elif defined(COMPONENT_WIFI_INTERFACE_OCI)

//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_custom_init_bus
//--------------------------------------------------------------------------------------------------
static cy_rslt_t _cybsp_wifi_custom_init_bus(void)
{
    cy_rslt_t rslt;

    whd_oci_config_t whd_oci_config =
    {
        .is_normal_mode = false,
    };
    // Note: The oci itself is initialized in the WHD
    rslt = whd_bus_oci_attach(whd_drv, &whd_oci_config);

    return rslt;
}


#endif // defined(COMPONENT_WIFI_INTERFACE_M2M)


//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_bus_init
//--------------------------------------------------------------------------------------------------
extern cy_rslt_t _cybsp_wifi_usb_init_bus(void);

static inline cy_rslt_t _cybsp_wifi_bus_init(void)
{
    #if !(defined(COMPONENT_WIFI_INTERFACE_M2M) || defined(COMPONENT_WIFI_INTERFACE_OCI))
    _cybsp_wifi_reset_wifi_chip();
    #endif
    #if defined(COMPONENT_WIFI_INTERFACE_SDIO)
    return _cybsp_wifi_sdio_init_bus();
    #elif defined(COMPONENT_WIFI_INTERFACE_SPI)
    return _cybsp_wifi_spi_init_bus();
    #elif defined(COMPONENT_WIFI_INTERFACE_M2M)
    return _cybsp_wifi_m2m_init_bus();
    #elif defined(COMPONENT_WIFI_INTERFACE_OCI)
    return _cybsp_wifi_custom_init_bus();
    #elif defined(COMPONENT_WIFI_INTERFACE_USB)
	return _cybsp_wifi_usb_init_bus();
    #endif
}


//--------------------------------------------------------------------------------------------------
// _cybsp_wifi_bus_detach
//--------------------------------------------------------------------------------------------------
static inline void _cybsp_wifi_bus_detach(void)
{
    #if defined(COMPONENT_WIFI_INTERFACE_SDIO)
    whd_bus_sdio_detach(whd_drv);
    #elif defined(COMPONENT_WIFI_INTERFACE_SPI)
    whd_bus_spi_detach(whd_drv);
    #elif defined(COMPONENT_WIFI_INTERFACE_M2M)
    whd_bus_m2m_detach(whd_drv);
    #elif defined(COMPONENT_WIFI_INTERFACE_OCI)
    whd_bus_oci_detach(whd_drv);
    #elif defined(COMPONENT_WIFI_INTERFACE_USB)
    whd_bus_usb_detach(whd_drv);
    #endif
}


//--------------------------------------------------------------------------------------------------
// cybsp_wifi_init_primary_extended
//--------------------------------------------------------------------------------------------------
cy_rslt_t cybsp_wifi_init_primary_extended(whd_interface_t* interface,
                                           whd_init_config_t* init_config,
                                           whd_resource_source_t* resource_if,
                                           whd_buffer_funcs_t* buffer_if,
                                           whd_netif_funcs_t* netif_if)
{
    #if defined(COMPONENT_WIFI_INTERFACE_M2M) || defined(COMPONENT_WIFI_INTERFACE_USB)
    cy_rslt_t result = CY_RSLT_SUCCESS;
    #else
    cy_rslt_t result = cyhal_gpio_init(CYBSP_WIFI_WL_REG_ON, CYHAL_GPIO_DIR_OUTPUT,
                                       CYBSP_WIFI_WL_REG_ON_GPIO_DRIVE_MODE, false);
    #endif

    if (result == CY_RSLT_SUCCESS)
    {
        if (init_config == NULL)
        {
            init_config = &init_config_default;
        }
        if (resource_if == NULL)
        {
            resource_if = &resource_ops;
        }
        if (buffer_if == NULL)
        {
            buffer_if = &buffer_if_default;
        }
        if (netif_if == NULL)
        {
            netif_if = &netif_if_default;
        }

        if (result == CY_RSLT_SUCCESS)
        {
            result = whd_init(&whd_drv, init_config, resource_if, buffer_if, netif_if);
            if (result == CY_RSLT_SUCCESS)
            {
                result = _cybsp_wifi_bus_init();

                if (result == CY_RSLT_SUCCESS)
                {
                    result = whd_wifi_on(whd_drv, interface);

                    if (result != CY_RSLT_SUCCESS)
                    {
                        _cybsp_wifi_bus_detach();
                    }
                }

                if (result != CY_RSLT_SUCCESS)
                {
                    whd_deinit(*interface);
                }
            }
        }

        #if !(defined(COMPONENT_WIFI_INTERFACE_M2M) || defined(COMPONENT_WIFI_INTERFACE_OCI))
        if (result != CY_RSLT_SUCCESS)
        {
            cyhal_gpio_free(CYBSP_WIFI_WL_REG_ON);
        }
        #endif
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
// cybsp_wifi_init_secondary
//--------------------------------------------------------------------------------------------------
cy_rslt_t cybsp_wifi_init_secondary(whd_interface_t* interface, whd_mac_t* mac_address)
{
    return whd_add_secondary_interface(whd_drv, mac_address, interface);
}


//--------------------------------------------------------------------------------------------------
// cybsp_wifi_deinit
//--------------------------------------------------------------------------------------------------
cy_rslt_t cybsp_wifi_deinit(whd_interface_t interface)
{
    cy_rslt_t result = whd_wifi_off(interface);

    if (result == CY_RSLT_SUCCESS)
    {
        _cybsp_wifi_bus_detach();
        #if !defined(COMPONENT_WIFI_INTERFACE_M2M) || !defined(COMPONENT_WIFI_INTERFACE_USB)
        cyhal_gpio_free(CYBSP_WIFI_WL_REG_ON);
        #endif
        // While deinit() takes an interface, it only uses it to get the underlying whd driver to
        // cleanup. As a result, we only need to call this on one of the interfaces.
        result = whd_deinit(interface);

        #if defined(CYBSP_WIFI_SDIO_NEEDS_INIT)
        if (result == CY_RSLT_SUCCESS)
        {
            cyhal_sdio_free(&sdio_obj);
        }
        #endif
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
// cybsp_get_wifi_driver
//--------------------------------------------------------------------------------------------------
whd_driver_t cybsp_get_wifi_driver(void)
{
    return whd_drv;
}


#if defined(__cplusplus)
}
#endif
