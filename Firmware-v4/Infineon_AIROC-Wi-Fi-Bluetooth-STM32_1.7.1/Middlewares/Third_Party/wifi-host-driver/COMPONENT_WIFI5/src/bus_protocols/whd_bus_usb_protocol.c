/*
 * Copyright 2023, Cypress Semiconductor Corporation (an Infineon company)
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
 */

/** @file
 *  Broadcom WLAN USB Protocol interface
 *
 *  Implements the WHD Bus Protocol Interface for USB
 *  Provides functions for initializing, de-intitializing 802.11 device,
 *  sending/receiving raw packets etc
 *
 */

#include "cybsp.h"
#include "whd_utils.h"

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE)
#include "whd_bus_usb_protocol.h"


/******************************************************
*             Structures
******************************************************/

struct whd_bus_priv
{
    void* usb_obj;
};

struct rdl_state_le
{
    uint32_t state;
    uint32_t bytes;
};

#pragma pack(1)

/* SDIO bus specific header - Software header */
typedef struct
{
    uint8_t sequence;               /* Rx/Tx sequence number */
    uint8_t channel_and_flags;      /*  4 MSB Channel number, 4 LSB arbitrary flag */
    uint8_t next_length;            /* Length of next data frame, reserved for Tx */
    uint8_t header_length;          /* Data offset */
    uint8_t wireless_flow_control;  /* Flow control bits, reserved for Tx */
    uint8_t bus_data_credit;        /* Maximum Sequence number allowed by firmware for Tx */
    uint8_t _reserved[2];           /* Reserved */
} sdpcm_sw_header_t;

/* SDPCM header definitions */
typedef struct
{
    uint16_t frametag[2];
    sdpcm_sw_header_t sw_header;
} sdpcm_header_t;

#pragma pack()


/******************************************************
*             Static Function Declarations
******************************************************/

static bool whd_bus_usb_dl_needed(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_resetcfg(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_download_firmware(whd_driver_t whd_driver);
static whd_bool_t whd_bus_usb_wake_interrupt_present(whd_driver_t whd_driver);
static uint32_t whd_bus_usb_packet_available_to_read(whd_driver_t whd_driver);

whd_result_t whd_bus_usb_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer);
whd_result_t whd_bus_usb_read_frame(whd_driver_t whd_driver, whd_buffer_t* buffer);
static whd_result_t whd_bus_usb_irq_enable(whd_driver_t whd_driver, whd_bool_t enable);
static whd_result_t whd_bus_usb_irq_register(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_reinit_stats(whd_driver_t whd_driver,
                                             whd_bool_t wake_from_firmware);
static whd_result_t whd_bus_usb_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print);
static void whd_bus_usb_init_stats(whd_driver_t whd_driver);
static uint32_t whd_bus_usb_get_max_transfer_size(whd_driver_t whd_driver);
static whd_bool_t whd_bus_usb_use_status_report_scheme(whd_driver_t whd_driver);
static uint8_t whd_bus_usb_backplane_read_padd_size(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus);
static whd_result_t whd_bus_usb_poke_wlan(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_wakeup(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_sleep(whd_driver_t whd_driver);
static whd_result_t whd_bus_usb_wait_for_wlan_event(whd_driver_t whd_driver,
                                                    cy_semaphore_t* transceive_semaphore);

/*****************************************************
 *             Global Function definitions
 ******************************************************/

whd_driver_t cybsp_get_wifi_driver(void);


/* Implement whd_bsp_integration USB bus functions */
cy_rslt_t _cybsp_wifi_usb_init_bus(void)
{
    // TODO: need get USB handle (USBH_HandleTypeDef) by public API (from wifi_bt_if.c).
    // similar how we have stm32_cypal_wifi_sdio_init() or stm32_cypal_wifi_spi_init()

    return whd_bus_usb_attach(cybsp_get_wifi_driver(),
                              (void*)NULL /* for emUSB we do not pass any handle */);
}


/* ------------------------------------------------------------------------------------- */

uint32_t whd_bus_usb_attach(whd_driver_t whd_driver, /*cyhal_usb_t*/ void* usb_obj)
{
    struct whd_bus_info* whd_bus_info;

    whd_bus_info = (whd_bus_info_t*)whd_mem_malloc(sizeof(whd_bus_info_t));

    if (whd_bus_info == NULL)
    {
        WPRINT_WHD_ERROR(("Memory allocation failed for whd_bus_info in %s\n", __FUNCTION__));
        return WHD_BUFFER_UNAVAILABLE_PERMANENT;
    }
    memset(whd_bus_info, 0, sizeof(whd_bus_info_t));

    whd_driver->bus_if = whd_bus_info;

    whd_driver->bus_priv = (struct whd_bus_priv*)whd_mem_malloc(sizeof(struct whd_bus_priv));

    if (whd_driver->bus_priv == NULL)
    {
        WPRINT_WHD_ERROR(("Memory allocation failed for whd_bus_priv in %s\n", __FUNCTION__));
        return WHD_BUFFER_UNAVAILABLE_PERMANENT;
    }
    memset(whd_driver->bus_priv, 0, sizeof(struct whd_bus_priv));

    whd_driver->bus_priv->usb_obj = usb_obj;

    whd_bus_info->whd_bus_init_fptr = whd_bus_usb_init;
    whd_bus_info->whd_bus_deinit_fptr = whd_bus_usb_deinit;

    whd_bus_info->whd_bus_send_buffer_fptr = whd_bus_usb_send_buffer;
    whd_bus_info->whd_bus_read_frame_fptr = whd_bus_usb_read_frame;

    whd_bus_info->whd_bus_packet_available_to_read_fptr = whd_bus_usb_packet_available_to_read;
    whd_bus_info->whd_bus_poke_wlan_fptr = whd_bus_usb_poke_wlan;
    whd_bus_info->whd_bus_wait_for_wlan_event_fptr = whd_bus_usb_wait_for_wlan_event;

    whd_bus_info->whd_bus_ack_interrupt_fptr = whd_bus_usb_ack_interrupt;
    whd_bus_info->whd_bus_wake_interrupt_present_fptr = whd_bus_usb_wake_interrupt_present;

    whd_bus_info->whd_bus_wakeup_fptr = whd_bus_usb_wakeup;
    whd_bus_info->whd_bus_sleep_fptr = whd_bus_usb_sleep;

    whd_bus_info->whd_bus_backplane_read_padd_size_fptr = whd_bus_usb_backplane_read_padd_size;
    whd_bus_info->whd_bus_use_status_report_scheme_fptr = whd_bus_usb_use_status_report_scheme;

    whd_bus_info->whd_bus_get_max_transfer_size_fptr = whd_bus_usb_get_max_transfer_size;

    whd_bus_info->whd_bus_init_stats_fptr = whd_bus_usb_init_stats;
    whd_bus_info->whd_bus_print_stats_fptr = whd_bus_usb_print_stats;
    whd_bus_info->whd_bus_reinit_stats_fptr = whd_bus_usb_reinit_stats;
    whd_bus_info->whd_bus_irq_register_fptr = whd_bus_usb_irq_register;
    whd_bus_info->whd_bus_irq_enable_fptr = whd_bus_usb_irq_enable;

    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_detach
 **************************************************************************************************/
void whd_bus_usb_detach(whd_driver_t whd_driver)
{
    if (whd_driver->bus_if != NULL)
    {
        whd_mem_free(whd_driver->bus_if);
        whd_driver->bus_if = NULL;
    }
    if (whd_driver->bus_priv != NULL)
    {
        whd_mem_free(whd_driver->bus_priv);
        whd_driver->bus_priv = NULL;
    }
}


/***************************************************************************************************
 * whd_bus_usb_init
 **************************************************************************************************/
whd_result_t whd_bus_usb_init(whd_driver_t whd_driver)
{
    /* Initialize BULK vendor class */
    whd_bus_usbh_class_init(whd_driver, true);

    if (whd_bus_usb_dl_needed(whd_driver))
    {
        whd_bus_usb_download_firmware(whd_driver);
    }

    whd_bus_set_state(whd_driver, true);
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_deinit
 **************************************************************************************************/
whd_result_t whd_bus_usb_deinit(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_dl_needed
 **************************************************************************************************/
static bool whd_bus_usb_dl_needed(whd_driver_t whd_driver)
{
    struct bootrom_id_le id;

    if (whd_driver == NULL)
    {
        return false;
    }

    /* Check if firmware is already downloaded  by querying runtime ID */
    id.chip = 0xDEAD;
    whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_GETVER, &id, sizeof(id));

    WPRINT_WHD_INFO(("Chip %x rev 0x%x\n", id.chip, id.chiprev));

    if (id.chip == WHD_USB_POSTBOOT_ID)
    {
        WPRINT_WHD_INFO(("Firmware already downloaded\n"));

        whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_RESETCFG, &id, sizeof(id));
        return false;
    }

    return true;
}


/***************************************************************************************************
 * whd_bus_usb_resetcfg
 **************************************************************************************************/
static whd_result_t whd_bus_usb_resetcfg(whd_driver_t whd_driver)
{
    whd_result_t result;
    struct bootrom_id_le id;
    uint32_t loop_cnt = 0;

    do
    {
        cy_rtos_delay_milliseconds(WHD_USB_RESET_GETVER_SPINWAIT);
        loop_cnt++;

        /* Check if after firmware download we get runtime ID */
        id.chip = 0xDEAD;
        result = whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_GETVER, &id, sizeof(id));
        if (result != WHD_SUCCESS)
        {
            return result;
        }

        if (id.chip == WHD_USB_POSTBOOT_ID)
        {
            break;
        }
    } while (loop_cnt < WHD_USB_RESET_GETVER_LOOP_CNT);

    if (id.chip == WHD_USB_POSTBOOT_ID)
    {
        (void)whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_RESETCFG, &id, sizeof(id));
        return 0;
    }
    else
    {
        WPRINT_WHD_ERROR(("%s: Cannot talk to Dongle. Firmware is not UP, %lu ms\n",
                          __FUNCTION__, WHD_USB_RESET_GETVER_SPINWAIT * loop_cnt));
        return 1;
    }
}


/***************************************************************************************************
 * whd_bus_usb_download_firmware
 **************************************************************************************************/
static whd_result_t whd_bus_usb_download_firmware(whd_driver_t whd_driver)
{
    whd_result_t result;

    uint32_t image_size;
    uint32_t size;
    uint32_t sent = 0;
    uint32_t buff[WHD_USB_TRX_RDL_CHUNK];

    struct rdl_state_le state;

    WPRINT_WHD_INFO(("\n\rStart FW download\n\r"));

    result = whd_resource_size(whd_driver, WHD_RESOURCE_WLAN_FIRMWARE, &image_size);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR(("Fatal error: download_resource doesn't exist, %s failed at line %d \n",
                          __func__, __LINE__));
    }

    if (image_size <= 0)
    {
        WPRINT_WHD_ERROR(("Fatal error: download_resource can't load with invalid size,"
                          "%s failed at line %d \n", __func__, __LINE__));
        result = WHD_BADARG;
    }

    /* Prepare USB boot loader for runtime image and check we are in the
     * Waiting state */
    result = whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_START, &state, sizeof(state));
    if ((result != WHD_SUCCESS) || (state.state != WHD_USB_DL_WAITING))
    {
        WPRINT_WHD_ERROR(("%s: Failed to DL_START\n", __FUNCTION__));
        return 1;
    }

    /* Download firmware */
    while (state.bytes != image_size)
    {
        /* Wait until the usb device reports it received all
         * the bytes we sent */
        if ((state.bytes == sent) && (state.bytes != image_size))
        {
            #if 0
            if ((image_size - sent) < WHD_USB_TRX_RDL_CHUNK)
            {
                size = image_size - sent;
            }
            else
            {
                size = WHD_USB_TRX_RDL_CHUNK;
            }
            #endif

            /* Read resource */
            CHECK_RETURN(whd_resource_read(whd_driver, WHD_RESOURCE_WLAN_FIRMWARE,
                                           /* offset */ state.bytes,
                                           /* size   */ WHD_USB_TRX_RDL_CHUNK,
                                           /* size   */ &size,
                                           /* buffer */ buff));

            /* Simply avoid having to send a ZLP by ensuring we never have an even
             * multiple of 64 */
            if (!(size % 64))
            {
                size -= 4;
            }

            /* Send data by USB Bulk */
            result = whd_bus_usb_bulk_send(whd_driver, (uint8_t*)buff, size);
            if (result != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR(("%s: Failed to write firmware image\n", __FUNCTION__));
            }

            sent += size;
        }

        /* Read the status and restart if an error is reported */
        result = whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_GETSTATE, &state, sizeof(state));
        if (result)
        {
            WPRINT_WHD_ERROR(("%s: DL_GETSTATE Failed\n", __FUNCTION__));
            return 1;
        }

        if ((state.state == WHD_USB_DL_BAD_HDR) || (state.state == WHD_USB_DL_BAD_CRC))
        {
            WPRINT_WHD_ERROR(("%s: Bad Hdr or Bad CRC state %lu\n\n", __FUNCTION__, state.state));
            return 1;
        }
    }
    WPRINT_WHD_INFO(("FW download complete, wrote %u bytes\n\r", state.bytes));

    /* Start the image */
    WPRINT_WHD_INFO(("\n\rStart the FW image \n\r"));
    if (state.state == WHD_USB_DL_RUNNABLE)
    {
        whd_bus_usb_dl_go(whd_driver);

        if (whd_bus_usb_resetcfg(whd_driver))
        {
            return 1;
        }

        /* The USB Dongle may go for re-enumeration. */
    }
    else
    {
        WPRINT_WHD_ERROR(("%s: Dongle not runnable\n", __FUNCTION__));
        return 1;
    }

    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_wake_interrupt_present
 **************************************************************************************************/
static whd_bool_t whd_bus_usb_wake_interrupt_present(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_packet_available_to_read
 **************************************************************************************************/
static uint32_t whd_bus_usb_packet_available_to_read(whd_driver_t whd_driver)
{
    return whd_bus_rx_queue_size();
}


/***************************************************************************************************
 * whd_bus_usb_send_buffer
 **************************************************************************************************/
whd_result_t whd_bus_usb_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    whd_result_t status = WHD_SUCCESS;

    uint8_t* data =
        (uint8_t*)((whd_transfer_bytes_packet_t*)(whd_buffer_get_current_piece_data_pointer(
                                                      whd_driver,
                                                      buffer) + sizeof(whd_buffer_t)))->data;
    uint16_t size =
        (uint16_t)(whd_buffer_get_current_piece_size(whd_driver, buffer) - sizeof(whd_buffer_t));

    /* The packet to sent has sdpcm header, so cast to sdpcm_header_t
     * to check packet type */
    sdpcm_header_t* sdpcm_header = (sdpcm_header_t*)data;
    uint32_t sdpcm_header_size = sdpcm_header->sw_header.header_length;

    /* Check the SDPCM channel to decide what to do with packet. */
    switch (sdpcm_header->sw_header.channel_and_flags & 0x0f)
    {
        case DATA_HEADER:
        {
            /* We need to send only BDC header + data, so find offset without sdpcm_header_t */
            bdc_header_t* bdc_header = (bdc_header_t*)(data + sdpcm_header_size);
            uint32_t bdc_size = size - sdpcm_header_size;

            status =
                whd_bus_usb_bulk_send(whd_driver, bdc_header,
                                      bdc_size - 4 /* TODO: add define for -4 */);
            break;
        }

        case CONTROL_HEADER:  /* Sent IOCTL/IOVAR packet (CDC packet) */
        {
            whd_buffer_t rec_buffer = NULL;

            /* We need to send only CDC header + data, so find offset without sdpcm_header_t */
            cdc_header_t* cdc_header = (cdc_header_t*)(data + sdpcm_header_size);
            uint32_t cdc_size = size - sdpcm_header_size;


            /* Send control request */
            CHECK_RETURN(whd_bus_usb_send_ctrl(whd_driver, cdc_header, &cdc_size));

            /* Receive control response */
            CHECK_RETURN(whd_bus_usb_receive_ctrl_buffer(whd_driver, &rec_buffer));

            /* Process CDC data... */
            whd_process_cdc(whd_driver, rec_buffer);
            break;
        }

        default:
            whd_minor_assert("whd_bus_usb_send_buffer: SDPCM packet of unknown"
                             " channel received - dropping packet", 0 != 0);
            break;
    }

    whd_buffer_release(whd_driver, buffer, WHD_NETWORK_TX);

    return status;
}


/***************************************************************************************************
 * whd_bus_usb_read_frame
 **************************************************************************************************/
whd_result_t whd_bus_usb_read_frame(whd_driver_t whd_driver, whd_buffer_t* buffer)
{
    /* Ensure the wlan backplane bus is up */
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver));

    /* Check if we have something in rx_queue */
    if (whd_bus_rx_queue_size())
    {
        /* Take a queue data */
        whd_bus_rx_queue_dequeue(buffer);
        return WHD_SUCCESS;
    }

    return 1;
}


/***************************************************************************************************
 * whd_bus_usb_irq_enable
 **************************************************************************************************/
static whd_result_t whd_bus_usb_irq_enable(whd_driver_t whd_driver, whd_bool_t enable)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_irq_register
 **************************************************************************************************/
static whd_result_t whd_bus_usb_irq_register(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_reinit_stats
 **************************************************************************************************/
static whd_result_t whd_bus_usb_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_print_stats
 **************************************************************************************************/
static whd_result_t whd_bus_usb_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_init_stats
 **************************************************************************************************/
static void whd_bus_usb_init_stats(whd_driver_t whd_driver)
{
}


/***************************************************************************************************
 * whd_bus_usb_get_max_transfer_size
 **************************************************************************************************/
static uint32_t whd_bus_usb_get_max_transfer_size(whd_driver_t whd_driver)
{
    return WHD_USB_MAX_BULK_TRANSFER_SIZE;
}


/***************************************************************************************************
 * whd_bus_usb_use_status_report_scheme
 **************************************************************************************************/
static whd_bool_t whd_bus_usb_use_status_report_scheme(whd_driver_t whd_driver)
{
    return true;
}


/***************************************************************************************************
 * whd_bus_usb_backplane_read_padd_size
 **************************************************************************************************/
static uint8_t whd_bus_usb_backplane_read_padd_size(whd_driver_t whd_driver)
{
    return 0;
}


/***************************************************************************************************
 * whd_bus_usb_ack_interrupt
 **************************************************************************************************/
static whd_result_t whd_bus_usb_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_poke_wlan
 **************************************************************************************************/
static whd_result_t whd_bus_usb_poke_wlan(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_wakeup
 **************************************************************************************************/
static whd_result_t whd_bus_usb_wakeup(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_sleep
 **************************************************************************************************/
static whd_result_t whd_bus_usb_sleep(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}


/***************************************************************************************************
 * whd_bus_usb_wait_for_wlan_event
 **************************************************************************************************/
void whd_usb_rx_thread_notify(void);

/***************************************************************************************************
 * whd_bus_usb_wait_for_wlan_event
 **************************************************************************************************/
static whd_result_t whd_bus_usb_wait_for_wlan_event(whd_driver_t whd_driver,
                                                    cy_semaphore_t* transceive_semaphore)
{
    whd_result_t result;

    whd_usb_rx_thread_notify();

    result = cy_rtos_get_semaphore(transceive_semaphore, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    return result;
}


#endif /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE) */
