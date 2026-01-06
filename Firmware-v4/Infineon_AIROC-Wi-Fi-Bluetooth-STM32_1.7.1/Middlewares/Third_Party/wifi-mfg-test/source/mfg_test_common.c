/*
 * Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
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
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */
 /** @file
 *
 * Description: This file contains the common logic that handles wl commands and
 *              the interface with WHD iovar/iotcl APIs.
 *
 * Related Document: See README.md.
  *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>


#ifndef MFGTEST_NO_HARDWARE
#include "cyhal.h"
#include "cybsp.h"
#else /* MFGTEST_NO_HARDWARE */
#include "wifi_mfgtest_stubs.h"
#endif /* MFGTEST_NO_HARDWARE */

#include "mfg_test.h"
#include "mfg_test_comp_ioctl.h"

/******************************************************
 *               Function Declarations
 ******************************************************/
static int wl_remote_CDC_rx( rem_ioctl_t *rem_ptr, unsigned char *readbuf, uint32_t buflen, int debug);
static int wl_remote_rx_data(void* buf_ptr);
static int wl_remote_tx_response(void* buf_ptr, int cmd);
static int wl_remote_CDC_tx( uint32_t cmd,  unsigned char *buf, uint32_t buf_len, uint32_t data_len,
               uint32_t flags, int debug);
static int wl_write_serial_data( unsigned char* write_buf, unsigned long size, unsigned long *numwritten);
static int wl_put_bytes (unsigned char *buf, int size );
static int wl_remote_CDC_rx_hdr( rem_ioctl_t *rem_ptr );
static int wl_read_serial_data( unsigned char *readbuf, uint32_t len , uint32_t *numread);
static int wl_remote_rx_header( rem_ioctl_t *rem_ptr );

/******************************************************
 *               Variable Definitions
 ******************************************************/
static rem_ioctl_t rem_cdc;
#define IOCTL_MED_LEN   (8192)
static char g_rem_ifname[32] = "wl";

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Function Name: wl_remote_command_handler
 ******************************************************************************
 * Summary:
 *  Function that takes the command from the wl tool and passes it down to the wlan firmware
 *  using ioctl/iovars.
 *
 * Parameters:
 *  char *buf : Input buffer containing the wl tool command.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
int wl_remote_command_handler( unsigned char *buf )
{
    int result = 0;
    unsigned char *buf_ptr = NULL;
    memset(&rem_cdc, 0, sizeof(rem_cdc));

    /* Receive the CDC header. */
    if ((wl_remote_rx_header(&rem_cdc)) < 0 )
    {
        MFG_DPRINT_DBG(OUTPUT, "\r\n Waiting for client to transmit command\r\n");
        return -1;
    }
    MFG_DPRINT_INFO(OUTPUT, "REC : cmd %d\t msg len %d  msg flag %d\t msg status %d\r\n",
                    rem_cdc.msg.cmd, rem_cdc.msg.len,
                    rem_cdc.msg.flags, rem_cdc.msg.status);

   /*
    * Allocate the buffer only if there is a response message expected.
    * Some commands such as up/down do not output anything.
    */
    if (rem_cdc.msg.len)
    {
        if ((buf_ptr = (unsigned char *)malloc(rem_cdc.msg.len)) == NULL)
        {
            MFG_DPRINT_ERR(ERR, "malloc of %d bytes failed\r\n", rem_cdc.msg.len);
            return -1;
        }
        memset(buf_ptr, 0, rem_cdc.msg.len);

        /* Receive the data. */
        if ((result = wl_remote_rx_data(buf_ptr)) == -1 )
        {
           rem_cdc.msg.status = result;
           if (buf_ptr)
           {
              free(buf_ptr);
           }
           return -1;
        }
        if (( result = wl_remote_tx_response(buf_ptr, 0)) != 0)
        {
            rem_cdc.msg.status = result;
            MFG_DPRINT_ERR(ERR, "\r\nReturn results failed\r\n");
        }
        if (buf_ptr)
        {
           free(buf_ptr);
        }
    }
    else
    {
       if (( result = wl_remote_tx_response(buf_ptr, 0)) != 0)
       {
          rem_cdc.msg.status = result;
          MFG_DPRINT_ERR(ERR, "\r\nReturn results failed\r\n");
       }
    }
    return result;
}

/******************************************************************************
 * Function Name: wl_remote_rx_data
 ******************************************************************************
 * Summary:
 *  Function that reads the CDC header sent by the wl tool before the command buffer
 *  using ioctl/iovars.
 *
 * Parameters:
 *  char *buf : Input buffer containing the wl tool command.
 *
 * Return:
 *  0 - Successful read of data.
 *  -1 - Failed to read data.
 *
 ******************************************************************************/
static
int wl_remote_rx_data(void* buf_ptr)
{
    if ((wl_remote_CDC_rx( &rem_cdc, (unsigned char *)buf_ptr, rem_cdc.msg.len, 0)) == -1)
    {
        MFG_DPRINT_ERR(ERR, "Reading CDC %d data bytes failed\r\n", rem_cdc.msg.len);
        return -1;
    }
    return 0;
}

/******************************************************************************
 * Function Name: wl_remote_CDC_rx
 ******************************************************************************
 * Summary:
 *  Function that receives the associated data as described in the CDC header over
 *  a serial port from the wl tool and writes to the passed buffer.
 *
 * Parameters:
 *  rem_ioctl_t *rem_ptr   : Pointer to the received CDC header.
 *  unsigned char *readbuf : Buffer to which the received data is copied.
 *  uint32_t buf_len       : Length of the buffer.
 *  int      debug         : Not used.
 *
 * Return:
 *  int 0 : Success
 *      -1: Failure
 *
 ******************************************************************************/
static
int wl_remote_CDC_rx( rem_ioctl_t *rem_ptr, unsigned char *readbuf, uint32_t buflen, int debug)
{
    uint32_t numread = 0;
    int result = 0;

    if (rem_ptr->data_len > rem_ptr->msg.len)
    {
       MFG_DPRINT_ERR(ERR, "wl_remote_CDC_rx: remote data len (%d) > msg len (%d)\r\n",
                      rem_ptr->data_len, rem_ptr->msg.len);
       return -1;
    }

    if (wl_read_serial_data( readbuf, rem_ptr->data_len, &numread) < 0)
    {
        MFG_DPRINT_ERR(ERR, "wl_read_serial_data: Data Receive failed \r\n");
        return -1;
    }
    return result;
}

/******************************************************************************
 * Function Name: wl_remote_CDC_rx_hdr
 ******************************************************************************
 * Summary:
 *  Function that receives the CDC header from the wl tool over a serial port.
 *
 * Parameters:
 *  rem_ioctl_t *rem_ptr : Pointer to copy the CDC header.
 *
 * Return:
 *  int 0 : Success
 *      -1: Failure
 *
 ******************************************************************************/
static
int wl_remote_CDC_rx_hdr( rem_ioctl_t *rem_ptr )
{
    uint32_t numread = 0;
    int result = 0;
    uint32_t len;
    len = sizeof(rem_ioctl_t);

    if (wl_read_serial_data( (unsigned char *)rem_ptr, len,	&numread) < 0)
    {
        MFG_DPRINT_ERR(ERR, "wl_remote_CDC_rx_hdr: Header Read failed \r\n");
        return -1;
    }
    return result;
}

/******************************************************************************
 * Function Name: wl_read_serial_data
 ******************************************************************************
 * Summary:
 *  Function that reads the data from the serial port.
 *
 * Parameters:
 *  unsigned char *readbuf : Buffer to which the received data is copied.
 *  uint32_t len           : Length of the buffer.
 *  uint32_t *numread      : Length of the data read.
 *
 * Return:
 *  int 0 : Success
 *      -1: Failure
 *
 ******************************************************************************/
static
int wl_read_serial_data( unsigned char *readbuf, uint32_t len, uint32_t *numread )
{
    int result = 0;
    int c;
    int i = 0;

    while ( i < (int)len )
    {
        c = getchar();
        readbuf[i++] = c;
    } /* end of while ( i < (int)len ) */

    if ( i > 0 )
    {
        *numread = i;
    } /* end of if */

    return result;
}

/******************************************************************************
 * Function Name: wl_remote_rx_header
 ******************************************************************************
 * Summary:
 *  Wrapper function that receives the CDC header from the wl tool.
 *
 * Parameters:
 *  rem_ioctl_t *rem_ptr : Pointer to the structure for the received CDC header.
 *
 * Return:
 *  int 0 : Success
 *      -1: Failure
 *
 ******************************************************************************/
static
int wl_remote_rx_header( rem_ioctl_t *rem_ptr )
{
   if ((wl_remote_CDC_rx_hdr( rem_ptr)) < 0)
   {
       MFG_DPRINT_DBG(OUTPUT, "\r\n Waiting for client to transmit command\r\n");
       return -1;
   }

   MFG_DPRINT_INFO(OUTPUT, "%d %d %d %d\r\n", rem_ptr->msg.cmd,
                   rem_ptr->msg.len, rem_ptr->msg.flags, rem_ptr->data_len);
   return 0;
}

/******************************************************************************
 * Function Name: wl_remote_tx_response
 ******************************************************************************
 * Summary:
 *  This function is used for transmitting the response over UART transport.
 *  The serial data is sent to the driver using the remote_CDC_dongle_tx function,
 *  which in turn may fragment the data and send it in chunks to the client.
 *
 * Parameters:
 *  void *buf_ptr : wBuffer pointer.
 *  int cmd       : ioctl command.
 *
 * Return:
 *  0 - Success
 *  Not 0 - Failure
 *
 ******************************************************************************/
static
int wl_remote_tx_response( void* buf_ptr, int cmd)
{
    int error = 0;
    int outlen = 0;

    if ( rem_cdc.msg.flags & REMOTE_GET_IOCTL )
    {
       error =  wl_ioctl( rem_cdc.msg.cmd, buf_ptr, rem_cdc.data_len, 0, &outlen);
    }
    else
    {
       error = wl_ioctl( rem_cdc.msg.cmd, buf_ptr, rem_cdc.data_len, true, &outlen);
    }

    if ( error != 0 )
    {
       rem_cdc.msg.status = error;
    }

    if ((error = wl_remote_CDC_tx( cmd, (unsigned char *)buf_ptr, outlen,
	                               outlen, REMOTE_REPLY, 0)) != 0)
    {
        MFG_DPRINT_ERR(ERR, "wl_server: Return results failed\r\n");
    }
    return error;
}

/******************************************************************************
 * Function Name: wl_remote_CDC_tx
 ******************************************************************************
 * Summary:
 *  Function that writes the CDC header and writes the response data to the serial
 *  port.
 *
 * Parameters:
 *  uint32_t cmd : wl tool command.
 *  unsigned char *buf : Buffer with the response data.
 *  uint32_t buf_len : Length of the buffer.
 *  uint32_t data_len : Size of the data to be written.
 *  uint32_t flasg : Update the flags field in the CDC header with this data.
 *  int      debug : Not used.
 *
 * Return:
 *  int 0 : Success
 *      -1: Failure
 *
 ******************************************************************************/
static
int wl_remote_CDC_tx( uint32_t cmd, unsigned char *buf, uint32_t buf_len, uint32_t data_len, uint32_t flags, int debug)
{
    unsigned long numwritten = 0;
    rem_ioctl_t *rem_ptr = &rem_cdc;
    int ret;

    memset(rem_ptr, 0, sizeof(rem_ioctl_t));
    rem_ptr->msg.cmd = cmd;
    rem_ptr->msg.len = data_len;
    rem_ptr->msg.flags = flags;
    rem_ptr->data_len = data_len;

    if (strlen(g_rem_ifname) != 0)
    {
       strncpy(rem_ptr->intf_name, g_rem_ifname, (int)INTF_NAME_SIZ);
       rem_ptr->intf_name[INTF_NAME_SIZ - 1] = '\0';
    }

    if (data_len > buf_len)
    {
        MFG_DPRINT_ERR(ERR, "wl_remote_CDC_tx: data_len (%d) > buf_len (%d)\r\n", data_len, buf_len);
        return -1;
    }

    /* Send the CDC header first. */
    if ((ret = wl_write_serial_data((unsigned char *)rem_ptr, REMOTE_SIZE, &numwritten)) == -1)
    {
        MFG_DPRINT_ERR(ERR, "CDC_Tx: Data: Write failed \r\n");
        return ret;
    }

    if ( data_len > 0 )
    {
       /* Send the data next. */
       if ((ret = wl_write_serial_data( (unsigned char*)buf, data_len, &numwritten)) == -1)
       {
          MFG_DPRINT_ERR(ERR, "CDC_Tx: Data: Write failed \r\n");
          return ret;
       }
    }
    return 0;
}

/******************************************************************************
 * Function Name: wl_write_serial_data
 ******************************************************************************
 * Summary:
 *  Function that writes the buffer to a serial port.
 *
 * Parameters:
 *  char *write_buf : Output buffer to be written to the serial port.
 *
 *  unsigned long int size: Size of the data to be written.
 *
 * unsigned long *numwrittent:  Number of bytes written to the serial port.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static
int wl_write_serial_data( unsigned char* write_buf, unsigned long size, unsigned long *numwritten)
{
    int result = 0;

    if ( size == 0 )
    {
       return 0;
    }
    result = wl_put_bytes ( write_buf, size );
    *numwritten = size;
    return result;
}

/******************************************************************************
 * Function Name: wl_put_bytes
 ******************************************************************************
 * Summary:
 *  Function that writes the buffer to the serial port to communicate with the wl tool
 *  executing on the host.
 *
 * Parameters:
 *  char *buf : Input buffer containing the data.
 *
 *  int size  : Size of the data to be written to the serial port.
 *
 * Return:
 *  0
 *
 ******************************************************************************/
static
int wl_put_bytes ( unsigned char *buf, int size )
{
   int i;

   for ( i = 0; i < size; i ++ )
   {
	  putchar(buf[i]);
   }
   return 0;
}

/******************************************************************************
 * Function Name: wl_set_sta_interface_handle
 ******************************************************************************
 * Summary:
 *  This function takes the passed "sta interface" handle and passes it to "iovar/ioctl"
 *  implementing logic to stash away for iovar/ioctl communication with the wlan
 *  firmware.
 *
 * Parameters:
 *  void *interface : STA interface handle for iovar/ioctl APIs.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void wl_set_sta_interface_handle( void *interface)
{
    wl_ioctl_set_sta_interface_handle(interface);
}

#ifdef __cplusplus
}
#endif

/* [] END OF FILE */
