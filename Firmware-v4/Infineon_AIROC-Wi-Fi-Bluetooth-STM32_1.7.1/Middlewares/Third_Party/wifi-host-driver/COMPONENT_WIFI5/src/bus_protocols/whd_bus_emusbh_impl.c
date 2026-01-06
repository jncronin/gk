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
 *  Implementation of USB bus low level functions
 *  by using em-USB Host middleware.
 */

#include <stdlib.h>
#include "cyabs_rtos.h"
#include "cybsp_wifi.h"

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE)

#include "whd_bus_usb_protocol.h"
#include "USBH.h"
#include "USBH_BULK.h"

/******************************************************
*             Constants
******************************************************/

#define CY_USB_VENDOR_ID_CYPRESS    0x04b4
#define CY_USB_4373_DEVICE_ID       0xbd29  /* CYW4373 device */
#define BRCM_USB_BCMFW_DEVICE_ID    0x0bdc  /* Special entry for device with firmware loaded and
                                               running */

#define WHD_USB_RX_QUEUE_SIZE       5

/******************************************************
*             Structures
******************************************************/

typedef struct
{
    int32_t          usb_device_index;
    bool             usb_device_ready;
    bool             fw_started;

    cy_thread_t      bulk_receive_thread;
    USBH_BULK_HANDLE bulk_handle;

    uint8_t          ep_in;
    uint8_t          ep_out;
    uint8_t          ep_intr;

    cy_queue_t       rx_queue;
} whd_bus_usb_device_info_t;


/* Structures for backplane & jtag accesses */
typedef struct
{
    uint32_t        cmd;        /* tag to identify the cmd */
    uint32_t        addr;       /* backplane address for write */
    uint32_t        len;        /* length of data: 1, 2, 4 bytes */
    uint32_t        data;       /* data to write */
} whd_bus_usb_hwacc_t;


typedef struct
{
    uint16_t         vid;
    uint16_t         pid;
} whd_bus_usb_device_id_info_t;


/******************************************************
*             Variables
******************************************************/

static whd_bus_usb_device_info_t whd_bus_usb_device_info;
static USBH_NOTIFICATION_HOOK whd_bus_usb_notif_hool;

/* Table of VID/PID for supported devices */
const whd_bus_usb_device_id_info_t whd_bus_usb_device_ids_table[]  =
{
    { CY_USB_VENDOR_ID_CYPRESS, CY_USB_4373_DEVICE_ID    },
    { CY_USB_VENDOR_ID_CYPRESS, BRCM_USB_BCMFW_DEVICE_ID },
};

uint8_t whd_bus_usb_device_ids_table_size = sizeof(whd_bus_usb_device_ids_table) /
                                            sizeof(whd_bus_usb_device_id_info_t);


/******************************************************
*             Static Function Declarations
******************************************************/

static void whd_bus_usb_device_notify(void* usb_context, uint8_t usb_index,
                                      USBH_DEVICE_EVENT usb_event);
static void whd_bus_usb_on_device_ready(void);
static void whd_bus_usb_on_device_removed(void);

static whd_result_t whd_bus_usb_convert_status(USBH_STATUS status);
static void whd_usb_rx_thread(void* arg);

#ifdef WPRINT_ENABLE_WHD_INFO
static const char* whd_bus_usb_get_port_speed(USBH_SPEED Speed);
#endif /* WPRINT_ENABLE_WHD_INFO */


/***********************************************************************************
 * Function Name: whd_bus_usbh_class_init
 ***********************************************************************************
 * Summary:
 * This function initialize Bulk class and adds a callback in order to be notified
 * when a device is added or removed
 *
 * Parameters:
 * whd_driver_t whd_driver  :   pointer to whd driver (whd_driver_t)
 *
 * Return:
 * void
 *
 **********************************************************************************/
void whd_bus_usbh_class_init(whd_driver_t whd_driver, bool wait_usb)
{
    whd_result_t retval;

    /* Initialize Bulk class */
    USBH_BULK_Init(NULL);

    #if 1
    /* Create RX task */
    retval = cy_rtos_create_thread(&whd_bus_usb_device_info.bulk_receive_thread,
                                   (cy_thread_entry_fn_t)whd_usb_rx_thread,
                                   "whd_usb_rx", NULL, 2000,
                                   whd_driver->thread_info.thread_priority + 1, NULL);
    if (retval != WHD_SUCCESS)
    {
        /* Could not start WHD main thread */
        WPRINT_WHD_ERROR(("Could not start whd_usb_rx thread\n"));
    }
    #endif // if 1

    /* Initialize rx queue */
    if (whd_bus_rx_queue_init() != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR(("Could not allocate rx_queue\n"));
    }


    /* Adds a callback in order to be notified when a device is added or removed */
    for (uint32_t i = 0; i < whd_bus_usb_device_ids_table_size; i++)
    {
        USBH_INTERFACE_MASK interface_mask;
        interface_mask.Mask = USBH_INFO_MASK_VID | USBH_INFO_MASK_PID;
        interface_mask.VendorId = whd_bus_usb_device_ids_table[i].vid;
        interface_mask.ProductId = whd_bus_usb_device_ids_table[i].pid;

        USBH_BULK_AddNotification(&whd_bus_usb_notif_hool, whd_bus_usb_device_notify,
                                  (void*)whd_driver, &interface_mask);
    }

    /* Waiting connection of USB dangle */
    if (wait_usb)
    {
        WPRINT_WHD_INFO(("Waiting USB dongle...\n\r"));

        while (!whd_bus_usb_device_info.usb_device_ready)
        {
            USBH_OS_Delay(10);
        }
    }
}


/***************************************************************************************************
 * whd_usb_rx_thread_notify
 **************************************************************************************************/
void whd_usb_rx_thread_notify(void)
{
    cy_rtos_thread_set_notification(&whd_bus_usb_device_info.bulk_receive_thread);
}


/***************************************************************************************************
 * whd_usb_rx_thread
 **************************************************************************************************/
static void whd_usb_rx_thread(void* arg)
{
    (void)arg;
    whd_buffer_t rx_buffer = NULL;
    whd_driver_t whd_driver = cybsp_get_wifi_driver();

    WPRINT_WHD_INFO(("whd_usb_rx_thread started\n\r"));

    whd_bus_usb_device_info.fw_started = false;
    while (1)
    {
        /* This thread read the Bulk EP (full frame) and put the data in rx_queue,
         * USBH_BULK_Read block this thread until some data is come.
         *
         * If we receive the Bulk data, then notify WHD thread(whd_thread_notify).
         *
         * NOTE: this thread should have priority > WHD thread.
         */
        if ((whd_bus_usb_device_info.fw_started) && (whd_bus_is_up(whd_driver) == true))
        {
            whd_result_t status = WHD_SUCCESS;

            /* Check if rx_queue is not full */
            if (!whd_bus_rx_queue_is_full())
            {
                /* Allocate buffer for frame packet */
                if (whd_host_buffer_get(whd_driver, &rx_buffer, WHD_NETWORK_RX,
                                        (uint16_t)(WHD_USB_MAX_RECEIVE_BUF_SIZE), 0))
                {   /* We can't allocate buffer so sleep some some time to allow WHD process
                     * pending packets */
                    USBH_OS_Delay(1);
                    break;
                }

                /* Get pointer to data field of buffer */
                uint8_t* rec_buf = whd_buffer_get_current_piece_data_pointer(whd_driver, rx_buffer);

                /* Enables short read mode. So the function USBH_BULK_Read() returns
                 * as soon as data was read from the device. This allows the application
                 * read data where the number of bytes to read is undefined.
                 */
                USBH_BULK_AllowShortRead(whd_bus_usb_device_info.bulk_handle, 1);

                /* Read frame from in bulk */
                uint32_t rx_bytes;
                status = USBH_BULK_Read(whd_bus_usb_device_info.bulk_handle,
                                        whd_bus_usb_device_info.ep_in,
                                        rec_buf, WHD_USB_MAX_RECEIVE_BUF_SIZE, &rx_bytes, 0);

                if (status == WHD_SUCCESS)
                {
                    /* Set final size of received packet */
                    whd_buffer_set_size(whd_driver, rx_buffer, rx_bytes);

                    /* ADD pointer of rx_buffer to rx_queue
                     * NOTE: rx_queue will released in upper layers */

                    whd_bus_rx_queue_enqueue(rx_buffer);
                }
                else
                {
                    /* We have error during USBH_BULK_Read, so release allocated buffer */
                    whd_buffer_release(whd_driver, rx_buffer, WHD_NETWORK_RX);
                }
            }
            else
            {
                /* rx_queue is full, so notify whd_thread and set this one in suspend */
                whd_thread_notify(whd_driver);
                cy_rtos_thread_wait_notification(CY_RTOS_NEVER_TIMEOUT);
            }

            /* Ping whd thread to process data... */
            whd_thread_notify(whd_driver);
        }
        else
        {
            USBH_OS_Delay(1);
        }
    }
}


/***************************************************************************************************
 * whd_bus_usb_dl_cmd
 **************************************************************************************************/
whd_result_t whd_bus_usb_dl_cmd(whd_driver_t whd_driver, uint8_t cmd, void* buffer, uint32_t buflen)
{
    (void)whd_driver;
    USBH_STATUS status;
    uint32_t size = buflen;

    /* Sends a specific request (cmd) to the device */
    status = USBH_BULK_SetupRequest(whd_bus_usb_device_info.bulk_handle,
                                    /* RequestType   */ USB_TO_HOST | USB_REQTYPE_VENDOR | USB_INTERFACE_RECIPIENT,
                                    /* Request       */ cmd,
                                    /* wValue        */ 0,
                                    /* wIndex        */ (cmd == WHD_USB_DL_GO) ? 1 : 0,
                                    /* pData         */ buffer,
                                    /* pnum_bytesData*/ &size,
                                    /* timeout       */ 0);

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_dl_go
 **************************************************************************************************/
whd_result_t whd_bus_usb_dl_go(whd_driver_t whd_driver)
{
    (void)whd_bus_usb_dl_cmd(whd_driver, WHD_USB_DL_GO, NULL, 0);

    /* Force set not ready */
    whd_bus_usb_device_info.usb_device_ready = false;

    /* Wait new enumeration */
    while (!whd_bus_usb_device_info.usb_device_ready)
    {
        USBH_OS_Delay(10);
    }

    whd_bus_usb_device_info.fw_started = true;

    return 0;
}


/***************************************************************************************************
 * whd_bus_usb_bulk_send
 **************************************************************************************************/
whd_result_t whd_bus_usb_bulk_send(whd_driver_t whd_driver, void* buffer, int len)
{
    (void)whd_driver;
    USBH_STATUS status;
    uint32_t byte_w = 0;

    /* Writes data to the BULK device */
    do
    {
        status = USBH_BULK_Write(whd_bus_usb_device_info.bulk_handle,
                                 whd_bus_usb_device_info.ep_out,
                                 buffer, len, &byte_w, 0);
    } while (status == USBH_STATUS_BUSY);

    if (status != USBH_STATUS_SUCCESS)
    {
        USBH_Logf_Application("USBH_BULK_Write() failed: %d", status);
    }

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_bulk_receive
 **************************************************************************************************/
whd_result_t whd_bus_usb_bulk_receive(whd_driver_t whd_driver, void* buffer, int len)
{
    (void)whd_driver;
    USBH_STATUS status = 0;
    uint32_t byte_r = 0;

    /* Writes data to the BULK device */
    do
    {
        if (status == USBH_STATUS_BUSY)
        {
            USBH_OS_Delay(1);
        }
        status = USBH_BULK_Read(whd_bus_usb_device_info.bulk_handle,
                                whd_bus_usb_device_info.ep_in,
                                buffer, len, &byte_r, 0);
    } while (status == USBH_STATUS_BUSY);

    if (status != USBH_STATUS_SUCCESS)
    {
        USBH_Logf_Application("USBH_BULK_Read() failed: %d", status);
    }

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_bulk_receive
 **************************************************************************************************/
whd_result_t whd_bus_usb_bulk_receive_timeout(whd_driver_t whd_driver, void* buffer, int len,
                                              uint32_t timeout)
{
    (void)whd_driver;
    USBH_STATUS status = 0;
    uint32_t byte_r = 0;

    /* Writes data to the BULK device */
    do
    {
        status = USBH_BULK_Read(whd_bus_usb_device_info.bulk_handle,
                                whd_bus_usb_device_info.ep_in,
                                buffer, len, &byte_r, timeout);
    } while (status == USBH_STATUS_BUSY);

    if (status != USBH_STATUS_SUCCESS)
    {
        USBH_Logf_Application("whd_bus_usb_bulk_receive_timeout() failed: %d %d", status, whd_bus_usb_bulk_get_num_bytes_in_rx_buff(
                                  whd_driver));
    }

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_bulk_get_num_bytes_in_rx_buff
 **************************************************************************************************/
uint32_t whd_bus_usb_bulk_get_num_bytes_in_rx_buff(whd_driver_t whd_driver)
{
    (void)whd_driver;
    uint32_t rx_bytes;

    if (USBH_BULK_GetNumBytesInBuffer(whd_bus_usb_device_info.bulk_handle,
                                      /* EPAddr   */ whd_bus_usb_device_info.ep_in,
                                      /* pRxBytes */ &rx_bytes) != USBH_STATUS_SUCCESS)
    {
        rx_bytes = 0;
    }

    return rx_bytes;
}


/*
   The bootloader supports additional commands to read and write data to/from backplane addresses
    DL_RDHW8: read an 8-bit value from a backplane address
    DL_RDHW16: read a 16-bit value from a backplane address (must be 2-byte aligned)
    DL_RDHW32: read a 32-bit value from a backplane address (must be 4-byte aligned)

   For example, to read the first 16-bit word of 43236 CIS (the CIS region begins at offset 0x30 of
      OTP, which starts at backplane address 0x18000800):
    bmRequestType: 0xC1 (Read Vendor Interface)
    bRequest: 0x11 (DL_RDHW16)
    wValue:   0x0830 (lower 16 bits of backplane address)
    wIndex:   0x1800 (upper 16 bits of backplane address)
    wLength:  sizeof(hwacc_t)

    response buffer should be a pointer to type hwacc_t; if successful, the value read will be in
       the hwacc_t.data field
    DL_WRHW: write a 8/16/32 bit value to a backplane address, observing byte alignment requirements

   For example, to write a 32-bit word to backplane address 0x18000634:
    bmRequestType: 0x41 (Write Vendor Interface)
    bRequest: 0x14 (DL_WRHW)
    wValue:   0x0001
    wIndex:   0x0000
    wLength:  sizeof(hwacc_t)

    buffer should be a pointer to type hwacc_t:
    hwacc.cmd  = 0x14 (DL_WRHW)
    hwacc.addr = 0x18000634
    hwacc.data = 32-bit value to write
    hwacc.len  = 4
 */

whd_result_t whd_bus_usb_readreg(whd_driver_t whd_driver, uint32_t regaddr, uint32_t datalen,
                                 uint32_t* value)
{
    USBH_STATUS status;
    whd_bus_usb_hwacc_t hwacc;
    uint32_t cmd;

    if (datalen == 1)
    {
        cmd = WHD_USB_DL_RDHW8;
    }
    else if (datalen == 2)
    {
        cmd = WHD_USB_DL_RDHW16;
    }
    else
    {
        cmd = WHD_USB_DL_RDHW32;
    }

    uint32_t size = sizeof(whd_bus_usb_hwacc_t);

    /* Sends a specific request (cmd) to the device */
    status = USBH_BULK_SetupRequest(whd_bus_usb_device_info.bulk_handle,
                                    /* RequestType   */ USB_TO_HOST | USB_REQTYPE_VENDOR | USB_INTERFACE_RECIPIENT,
                                    /* Request       */ cmd,
                                    /* wValue        */ (uint16_t)(regaddr),
                                    /* wIndex        */ (uint16_t)(regaddr >> 16),
                                    /* pData         */ &hwacc,
                                    /* pnum_bytesData */ &size,
                                    /* timeout       */ 0);

    *value = hwacc.data;

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_writereg
 **************************************************************************************************/
whd_result_t whd_bus_usb_writereg(whd_driver_t whd_driver, uint32_t regaddr, uint32_t datalen,
                                  uint32_t data)
{
    USBH_STATUS status;
    whd_bus_usb_hwacc_t hwacc;
    uint32_t cmd = WHD_USB_DL_WRHW;

    hwacc.cmd = WHD_USB_DL_WRHW;
    hwacc.addr = regaddr;
    hwacc.data = data;
    hwacc.len = datalen;

    uint32_t size = sizeof(whd_bus_usb_hwacc_t);

    /* Sends a specific request (cmd) to the device */
    status = USBH_BULK_SetupRequest(whd_bus_usb_device_info.bulk_handle,
                                    /* RequestType   */ USB_TO_DEVICE | USB_REQTYPE_VENDOR | USB_INTERFACE_RECIPIENT,
                                    /* Request       */ cmd,
                                    /* wValue        */ 1,
                                    /* wIndex        */ 0,
                                    /* pData         */ &hwacc,
                                    /* pnum_bytesData */ &size,
                                    /* timeout       */ 0);

    return whd_bus_usb_convert_status(status);
}


/* Device data transfer functions */
whd_result_t whd_bus_usb_send_ctrl(whd_driver_t whd_driver, void* buffer, uint32_t* len)
{
    /* TODO: {PROBLEM} sometime during send iocntrol, USB stack return the
       USBH_STATUS_NOTRESPONDING:
     * if re-try failed operation (send same cmd again), i got successful result,
     * but device does not send scan events...
     *
     * if skip to checking the return of failed operation(just ignore USBH_STATUS_NOTRESPONDING),
     * scan evens normal came...
     *
     * Need collect the USB traffic log and maybe somebody from FW team can help to understand
     * what is going wrong and how we can handle this in correct way.
     */
    USBH_STATUS status;

    do
    {
        /* Sends a specific wr request (cmd) to the device */
        status =  USBH_BULK_SetupRequest(whd_bus_usb_device_info.bulk_handle,
                                         /* RequestType   */ USB_TO_DEVICE | USB_REQTYPE_CLASS | USB_INTERFACE_RECIPIENT,
                                         /* Request       */ 0,
                                         /* wValue        */ 0,
                                         /* wIndex        */ 0,
                                         /* pData         */ buffer,
                                         /* pnum_bytesData */ len,
                                         /* timeout       */ 0);

        //printf("::>> send_ctrl: status: %x \n\r", status);

        /* Workaround for problem described above */
        #if 1
        if (status == USBH_STATUS_NOTRESPONDING)
        {
            WPRINT_WHD_ERROR((
                                 "USBH_BULK_SetupRequest get USBH_STATUS_NOTRESPONDING in %s. As workaround skip this.\n",
                                 __FUNCTION__));
            status = USBH_STATUS_SUCCESS;
        }
        #endif
    } while(status);

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_receive_ctrl_buffer
 **************************************************************************************************/
whd_result_t whd_bus_usb_receive_ctrl_buffer(whd_driver_t whd_driver, whd_buffer_t* buffer)
{
    USBH_STATUS status;

    uint32_t size = WHD_USB_MAX_RECEIVE_BUF_SIZE;

    whd_host_buffer_get(whd_driver, buffer, WHD_NETWORK_RX,
                        (unsigned short)(WHD_USB_MAX_RECEIVE_BUF_SIZE +
                                         (uint16_t)sizeof(whd_buffer_header_t)),
                        (whd_sdpcm_has_tx_packet(whd_driver) ? 0 : WHD_RX_BUF_TIMEOUT));

    do
    {
        /* Sends a specific rd request (cmd) to the device */
        status = USBH_BULK_SetupRequest(whd_bus_usb_device_info.bulk_handle,
                                        /* RequestType   */ USB_TO_HOST | USB_REQTYPE_CLASS | USB_INTERFACE_RECIPIENT,
                                        /* Request       */ 1,
                                        /* wValue        */ 0,
                                        /* wIndex        */ 0,
                                        whd_buffer_get_current_piece_data_pointer(whd_driver,
                                                                                  *buffer),
                                        &size,
                                        /* timeout       */ 0);
        //  printf("::<< receive_ctrl: status: %x len: %u \n\r", status, size);
    } while(status);

    return whd_bus_usb_convert_status(status);
}


/***************************************************************************************************
 * whd_bus_usb_device_notify
 **************************************************************************************************/
static void whd_bus_usb_device_notify(void* usb_context, uint8_t usb_index,
                                      USBH_DEVICE_EVENT usb_event)
{
    (void)usb_context;

    switch (usb_event)
    {
        // TODO: replace USBH_Logf_Application to some WHD Debug API.

        case USBH_DEVICE_EVENT_ADD:
        {
            whd_bus_usb_device_info.usb_device_index = usb_index;

            whd_bus_usb_on_device_ready();
            whd_bus_usb_device_info.usb_device_ready = true;
            break;
        }

        case USBH_DEVICE_EVENT_REMOVE:
        {
            whd_bus_usb_device_info.usb_device_index = -1;

            whd_bus_usb_on_device_removed();
            whd_bus_usb_device_info.usb_device_ready = false;
            break;
        }

        default:
        {
            USBH_Logf_Application(":: Invalid event [%d] ==\n\n", usb_event);
            break;
        }
    }
}


/***************************************************************************************************
 * whd_bus_usb_on_device_ready
 **************************************************************************************************/
static void whd_bus_usb_on_device_ready(void)
{
    USBH_BULK_DEVICE_INFO dev_info;
    USBH_BULK_EP_INFO     ep_info;
    USBH_STATUS           status;
    uint8_t               i;
    static uint8_t        ac_data[512];


    memset(&dev_info, 0, sizeof(dev_info));
    memset(&ac_data, 0, sizeof(ac_data));

    /* Open the device, the device index is retrieved from the notification callback. */
    whd_bus_usb_device_info.bulk_handle = USBH_BULK_Open(whd_bus_usb_device_info.usb_device_index);
    if (whd_bus_usb_device_info.bulk_handle)
    {
        /* Print device info. */
        status = USBH_BULK_GetDeviceInfo(whd_bus_usb_device_info.bulk_handle, &dev_info);
        if (status != USBH_STATUS_SUCCESS)
        {
            WPRINT_WHD_ERROR(("%s: USBH_BULK_GetDeviceInfo failed (0x%x)\n", __FUNCTION__, status));
            return;
        }

        WPRINT_WHD_INFO(("\n\rDetected device info:\n\r"));
        WPRINT_WHD_INFO(("Vendor  Id = 0x%.4x\n\r", dev_info.VendorId));
        WPRINT_WHD_INFO(("Product Id = 0x%.4x\n\r", dev_info.ProductId));

        #ifdef WPRINT_ENABLE_WHD_INFO
        uint32_t              num_bytes;
        uint8_t* p;

        USBH_BULK_GetSerialNumber(whd_bus_usb_device_info.bulk_handle, sizeof(ac_data), ac_data,
                                  &num_bytes);
        p = ac_data;
        for (i = 0; i < num_bytes; i++)
        {
            if (ac_data[i] != 0)
            {
                *p++ = ac_data[i];
            }
        }
        *p = 0;

        WPRINT_WHD_INFO(("Serial no. = %s\n\r", ac_data));
        WPRINT_WHD_INFO(("Speed      = %s\n\r", whd_bus_usb_get_port_speed(dev_info.Speed)));
        WPRINT_WHD_INFO(("\n\r"));

        #endif /* WPRINT_ENABLE_WHD_INFO */

        /* Retrieve the endpoint addresses. */
        whd_bus_usb_device_info.ep_in   = 0;
        whd_bus_usb_device_info.ep_out  = 0;
        whd_bus_usb_device_info.ep_intr = 0;

        for (i = 0; i < dev_info.NumEPs; i++)
        {
            if (USBH_BULK_GetEndpointInfo(whd_bus_usb_device_info.bulk_handle, i,
                                          &ep_info) != USBH_STATUS_SUCCESS)
            {
                USBH_Logf_Application("USBH_BULK_GetEndpointInfo() failed");
                break;
            }

            if ((ep_info.Direction == USB_IN_DIRECTION) &&
                (ep_info.Type == USB_EP_TYPE_INT))
            {
                whd_bus_usb_device_info.ep_intr = ep_info.Addr;
            }

            if ((ep_info.Direction == USB_IN_DIRECTION) &&
                (ep_info.Type == USB_EP_TYPE_BULK))
            {
                whd_bus_usb_device_info.ep_in = ep_info.Addr;
            }

            if ((ep_info.Direction == USB_OUT_DIRECTION) &&
                (ep_info.Type == USB_EP_TYPE_BULK))
            {
                whd_bus_usb_device_info.ep_out = ep_info.Addr;
            }
        }

        if ((whd_bus_usb_device_info.ep_in == 0) || (whd_bus_usb_device_info.ep_out == 0))
        {
            WPRINT_WHD_ERROR(("%s: Endpoint(s) not found.\n", __FUNCTION__));
            return;
        }
    }
}


/***************************************************************************************************
 * whd_bus_usb_on_device_removed
 **************************************************************************************************/
static void whd_bus_usb_on_device_removed(void)
{
    USBH_BULK_Close(whd_bus_usb_device_info.bulk_handle);
}


/***************************************************************************************************
 * whd_bus_usb_convert_status
 **************************************************************************************************/
static whd_result_t whd_bus_usb_convert_status(USBH_STATUS status)
{
    return (status == USBH_STATUS_SUCCESS) ? WHD_SUCCESS : WHD_HAL_ERROR;
}


/***************************************************************************************************
 * whd_bus_usb_get_port_speed
 **************************************************************************************************/

#ifdef WPRINT_ENABLE_WHD_INFO
static const char* whd_bus_usb_get_port_speed(USBH_SPEED Speed)
{
    switch (Speed)
    {
        case USBH_LOW_SPEED:
            return "LowSpeed";

        case USBH_FULL_SPEED:
            return "FullSpeed";

        case USBH_HIGH_SPEED:
            return "HighSpeed";

        default:
            break;
    }
    return "Unknown";
}


#endif /* WPRINT_ENABLE_WHD_INFO */

/* Initialize rx_queue */
whd_result_t whd_bus_rx_queue_init(void)
{
    if (cy_rtos_queue_init(&whd_bus_usb_device_info.rx_queue,
                           WHD_USB_RX_QUEUE_SIZE, sizeof(whd_buffer_t*)))
    {
        return WHD_QUEUE_ERROR;
    }
    return WHD_SUCCESS;
}


/* Checks if the queue is full */
bool whd_bus_rx_queue_is_full(void)
{
    size_t num_spaces;
    cy_rtos_queue_space(&whd_bus_usb_device_info.rx_queue,
                        &num_spaces);

    return num_spaces ? false : true;
}


/* Returns the total number of elements in the queue */
size_t whd_bus_rx_queue_size(void)
{
    size_t num = 0;
    cy_rtos_queue_count(&whd_bus_usb_device_info.rx_queue, &num);
    return num;
}


/* Adds an element to the end of the queue  */
whd_result_t whd_bus_rx_queue_enqueue(whd_buffer_t* data)
{
    cy_rslt_t status = cy_rtos_queue_put(&whd_bus_usb_device_info.rx_queue,
                                         &data, CY_RTOS_NEVER_TIMEOUT);

    return status ? WHD_QUEUE_ERROR : WHD_SUCCESS;
}


/* Removes an element from the front of the queue */
whd_result_t whd_bus_rx_queue_dequeue(whd_buffer_t* data)
{
    cy_rslt_t status = cy_rtos_queue_get(&whd_bus_usb_device_info.rx_queue,
                                         data, CY_RTOS_NEVER_TIMEOUT);
    return status ? WHD_QUEUE_ERROR : WHD_SUCCESS;
}


#endif /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_USB_INTERFACE) */
