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

#include "whd.h"
#include "bus_protocols/whd_bus_protocol_interface.h"

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE)

#include "cyabs_rtos.h"
#include "whd_bus.h"
#include "whd_bus_common.h"
#include "whd_chip_reg.h"
#include "whd_chip_constants.h"
#include "whd_int.h"
#include "whd_chip.h"
#include "whd_sdpcm.h"
#include "whd_debug.h"
#include "whd_sdio.h"
#include "whd_buffer_api.h"
#include "whd_resource_if.h"
#include "whd_types_int.h"
#include "whd_types.h"
#include "whd_proto.h"


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
*             Macros
******************************************************/

/* Control messages: bRequest values */
#define WHD_USB_DL_GETSTATE            0x0      /* returns the rdl_state_t struct */
#define WHD_USB_DL_CHECK_CRC           0x1      /* currently unused */
#define WHD_USB_DL_GO                  0x2      /* execute downloaded image */
#define WHD_USB_DL_START               0x3      /* initialize dl state */
#define WHD_USB_DL_REBOOT              0x4      /* reboot the device in 2 seconds */
#define WHD_USB_DL_GETVER              0x5      /* returns the bootrom_id_t struct */
#define WHD_USB_DL_GO_PROTECTED        0x6      /* execute the downloaded code and set reset
                                                 * event to occur in 2 seconds.  It is the
                                                 * responsibility of the downloaded code to
                                                 * clear this event */
#define WHD_USB_DL_EXEC                0x7      /* jump to a supplied address */
#define WHD_USB_DL_RESETCFG            0x8      /* To support single enum on dongle
                                                 * - Not used by bootloader */
#define WHD_USB_DL_DEFER_RESP_OK       0x9      /* Potentially defer the response to setup
                                                 * if resp unavailable */
#define WHD_USB_DL_RDHW                0x10     /* Read a hardware address (Ctl-in) */
#define WHD_USB_DL_RDHW32              0x10     /* Read a 32 bit word */
#define WHD_USB_DL_RDHW16              0x11     /* Read 16 bits */
#define WHD_USB_DL_RDHW8               0x12     /* Read an 8 bit byte */
#define WHD_USB_DL_WRHW                0x14     /* Write a hardware address (Ctl-out) */
#define WHD_USB_DL_WRHW_BLK            0x13     /* Block write to hardware access */

/* States */
#define WHD_USB_DL_WAITING             0        /* waiting to rx first pkt */
#define WHD_USB_DL_READY               1        /* hdr was good, waiting for more of the
                                                 * compressed image  */
#define WHD_USB_DL_BAD_HDR             2        /* hdr was corrupted */
#define WHD_USB_DL_BAD_CRC             3        /* compressed image was corrupted */
#define WHD_USB_DL_RUNNABLE            4        /* download was successful,waiting for go cmd */
#define WHD_USB_DL_START_FAIL          5        /* failed to initialize correctly */
#define WHD_USB_DL_NVRAM_TOOBIG        6        /* host specified nvram data exceeds DL_NVRAM
                                                 * value */
#define WHD_USB_DL_IMAGE_TOOBIG        7        /* firmware image too big */


#define WHD_USB_POSTBOOT_ID            (0xA123) /* ID to detect if dongle has boot up */
#define WHD_USB_TRX_RDL_CHUNK          (1500)

#define WHD_USB_RESET_GETVER_SPINWAIT  (10)    /* in unit of ms */
#define WHD_USB_RESET_GETVER_LOOP_CNT  (10)


#define BCM_MSG_IFNAME_MAX             (16)    /** Maximum length of an interface name in a
                                                   wl_event_msg_t structure*/

#define WHD_USB_MAX_BULK_TRANSFER_SIZE (512)

/* Define max buffer size for receive packet */
#ifndef WHD_USB_MAX_RECEIVE_BUF_SIZE
    #if defined (WLAN_MFG_FIRMWARE)
        #define WHD_USB_MAX_RECEIVE_BUF_SIZE (WHD_USB_MAX_BULK_TRANSFER_SIZE * 10)
    #else
        #define WHD_USB_MAX_RECEIVE_BUF_SIZE (WHD_USB_MAX_BULK_TRANSFER_SIZE * 4)
    #endif /* */
#endif /* WHD_USB_MAX_RECEIVE_BUFFER_SIZE */

/******************************************************
*             Structures
******************************************************/
struct bootrom_id_le
{
    uint32_t chip;            /* Chip id */
    uint32_t chiprev;         /* Chip rev */
    uint32_t ramsize;         /* Size of  RAM */
    uint32_t remapbase;       /* Current remap base address */
    uint32_t boardtype;       /* Type of board */
    uint32_t boardrev;        /* Board revision */
};


/******************************************************
*             Function declarations
******************************************************/

whd_result_t whd_bus_usb_init(whd_driver_t whd_driver);
whd_result_t whd_bus_usb_deinit(whd_driver_t whd_driver);

uint32_t whd_bus_usb_attach(whd_driver_t whd_driver, /*cyhal_usb_t*/ void* usb_obj);
void whd_bus_usb_detach(whd_driver_t whd_driver);


/* Function from USB Host implementation */
void whd_bus_usbh_class_init(whd_driver_t whd_driver, bool wait_usb);

whd_result_t whd_bus_usb_dl_cmd(whd_driver_t whd_driver, uint8_t cmd, void* buffer,
                                uint32_t buflen);
whd_result_t whd_bus_usb_dl_go(whd_driver_t whd_driver);

whd_result_t whd_bus_usb_bulk_send(whd_driver_t whd_driver, void* buffer, int len);
whd_result_t whd_bus_usb_bulk_receive(whd_driver_t whd_driver, void* buffer, int len);
whd_result_t whd_bus_usb_bulk_receive_timeout(whd_driver_t whd_driver, void* buffer, int len,
                                              uint32_t timeout);

uint32_t whd_bus_usb_bulk_get_num_bytes_in_rx_buff(whd_driver_t whd_driver);

whd_result_t whd_bus_usb_readreg(whd_driver_t whd_driver, uint32_t regaddr, uint32_t datalen,
                                 uint32_t* value);
whd_result_t whd_bus_usb_writereg(whd_driver_t whd_driver, uint32_t regaddr, uint32_t datalen,
                                  uint32_t data);

whd_result_t whd_bus_usb_send_ctrl(whd_driver_t whd_driver, void* buffer, uint32_t* len);
whd_result_t whd_bus_usb_receive_ctrl_buffer(whd_driver_t whd_driver, whd_buffer_t* buffer);

/* rx queue helper function */
whd_result_t whd_bus_rx_queue_init(void);
whd_result_t whd_bus_rx_queue_enqueue(whd_buffer_t* data);
whd_result_t whd_bus_rx_queue_dequeue(whd_buffer_t* data);
bool whd_bus_rx_queue_is_full(void);
size_t whd_bus_rx_queue_size(void);
#ifdef __cplusplus
} /*extern "C" */
#endif

#endif /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE) */
