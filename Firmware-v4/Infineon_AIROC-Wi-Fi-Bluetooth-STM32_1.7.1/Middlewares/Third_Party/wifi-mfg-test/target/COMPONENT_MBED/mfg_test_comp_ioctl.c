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
 * Description: This file contains the IOTCL/IOVAR Logic that handles
 *              wl commands & interface with WHD iovar/iotcl APIS.
 *
 * Related Document: See README.md
  *
 */

/* Wi-Fi Host driver includes. */
#ifndef MFGTEST_NO_HARDWARE
#include "whd.h"
#include "whd_wifi_api.h"
#include "whd_network_types.h"
#include "whd_buffer_api.h"
#include "whd_wlioctl.h"
#else /* MFGTEST_NO_HARDWARE */
#include "wifi_mfgtest_stubs.h"
#endif /* MFGTEST_NO_HARDWARE */

/******************************************************
 *               External Function Declarations
 ******************************************************/
extern char *strtok_r( char *, const char *, char ** );

/******************************************************
 *               Function Declarations
 ******************************************************/
cy_rslt_t cywifi_set_iovar_value( const char *iovar, uint32_t value );
cy_rslt_t cywifi_set_ioctl_value( uint32_t ioctl, uint32_t value);
cy_rslt_t cywifi_set_ioctl_buffer( uint32_t ioctl, uint8_t *buffer, uint16_t len);
cy_rslt_t cywifi_get_iovar_value( const char *iovar, uint32_t *value );
cy_rslt_t cywifi_get_ioctl_buffer( uint32_t ioctl, uint8_t *buffer, uint16_t len);
cy_rslt_t cywifi_get_ioctl_value( uint32_t ioctl, uint32_t *value);

/******************************************************
 *               Variable Definitions
 ******************************************************/
static whd_interface_t sta_interface;

#ifdef __cplusplus
extern "C" {
#endif


/*
 * This function is used for sending wl_ioctl/iovar to WHD driver
 *
 * @ param1 int  cmd : cmd for IOCTL
 * @ param2 void *buf: Buffer to fill for iovar set/get request/response
 * @ param3 int  len : Length of Buffer
 * @ param4 bool set : GET/SET GET is zero/SET is 1.
 * @ param5 int  outlen: output data length
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
int wl_ioctl( int cmd, void *buf, int len, bool set, int *outlen)
{
    uint32_t value = 0;
    int result = 0;
    char *token = NULL;
    char *saveptr = (char *)buf;
    int datalen = len;

    if ( ( cmd != WLC_GET_VAR) && ( cmd != WLC_SET_VAR ))
    {
       if  ( ( len <= 4 ) && (set ))
       {
          if ( ( len > 0 ) && ( buf ) )
          {
            memcpy( &value, buf, len );
          }
            result = cywifi_set_ioctl_value(cmd, value);
       }
       else if ( ( len <= 4) )
       {
          result = cywifi_get_ioctl_value(cmd, &value);
          memcpy(buf, &value, len );
       }
       else if ( ( len > 4) && ( set )  )
       {
          result = cywifi_set_ioctl_buffer(cmd, (uint8_t *) buf, len);
       }
       else if ( ( len > 4 ) && ( !set ))
       {
          result = cywifi_get_ioctl_buffer(cmd, (uint8_t *) buf, len);
       }
   }
   else
   {
      token = strtok_r((char *)buf, "'\0'", &saveptr);
      datalen = len - (strlen(token) + 1);

      if ( ( datalen <= 4 ) && ( cmd == WLC_SET_VAR ) )
      {
         memcpy(&value, (char *)(((char *)buf) + strlen(token) + 1)  , sizeof(value));
         result = cywifi_set_iovar_value( (const char *)buf, value);
      }
      else if  (( datalen <= 4 ) && ( cmd == WLC_GET_VAR ) )
      {
         result = cywifi_get_iovar_value((const char *)buf, &value);
         memcpy(buf, &value, len );
      }
      else if ( ( datalen > 4) && ( set )  )
      {
         result = cywifi_set_ioctl_buffer(cmd, (uint8_t *) buf, len);
      }
      else if ( ( datalen > 4 ) && ( !set ))
      {
         result = cywifi_get_ioctl_buffer(cmd, (uint8_t *) buf, len);
      }
   }

   if ( set )
   {
      *outlen = 0;
   }
   else
   {
      *outlen = len;
   }
   return result;
}

/*
 * This function is used save the STA interface handle for iovar/ioctl
 * with Wlan firmware
 *
 * @ param1 void *interface : STA interface handle
 * @ return void
 */
void wl_ioctl_set_sta_interface_handle( void *interface)
{
   sta_interface = interface;
}


/*
 * This function is used for set IOVAR to WHD driver
 *
 * @ param1 const char *iovar : Variable for IOVAR
 * @ param2 uint32_t value    : value of IOVAR
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
cy_rslt_t cywifi_set_iovar_value( const char *iovar, uint32_t value )
{
    cy_rslt_t res;
    res = whd_wifi_set_iovar_value(sta_interface, iovar, value);
	return res;
}

/*
 * This function is used for set IOCTL to WHD driver
 *
 * @ param1 uint32_t ioctl : IOCTL command
 * @ param2 uint32_t value : value of IOCTL
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
cy_rslt_t cywifi_set_ioctl_value( uint32_t ioctl, uint32_t value)
{
    cy_rslt_t res;
    res = whd_wifi_set_ioctl_value(sta_interface, ioctl, value);
    return res;
}

/*
 * This function is used for set IOCTL buffer to WHD driver
 *
 * @ param1 uint32_t ioctl : IOCTL command
 * @ param2 void *buf: Buffer with fill for IOCTL set request
 * @ param3 int  len : Length of Buffer
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
cy_rslt_t cywifi_set_ioctl_buffer( uint32_t ioctl, uint8_t *buffer, uint16_t len)
{
    cy_rslt_t res;
    res = whd_wifi_set_ioctl_buffer(sta_interface, ioctl, buffer, len);
    return res;
}

/*
 * This function is used to get IOVAR to WHD driver
 *
 * @ param1 const char *iovar : Variable for IOVAR
 * @ param2 uint32_t *value   : returned value of IOVAR
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
cy_rslt_t cywifi_get_iovar_value( const char *iovar, uint32_t *value )
{
    cy_rslt_t res;
    res = whd_wifi_get_iovar_value(sta_interface, iovar, value);
    return res;
}

/*
 * This function is used to get IOCTL to WHD driver
 *
 * @ param1 uint32_t ioctl  : IOCTL command
 * @ param2 uint32_t *value : returned value of IOCTL
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
cy_rslt_t cywifi_get_ioctl_value( uint32_t ioctl, uint32_t *value)
{
    cy_rslt_t res;
    res = whd_wifi_get_ioctl_value(sta_interface, ioctl, value);
    return res;
}

/*
 * This function is used to get IOCTL buffer to WHD driver
 *
 * @ param1 uint32_t ioctl : IOCTL command
 * @ param2 void *buf: Buffer for returned values of IOCTL get request
 * @ param3 int  len : Length of Buffer
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
cy_rslt_t cywifi_get_ioctl_buffer( uint32_t ioctl, uint8_t *buffer, uint16_t len)
{
    cy_rslt_t res;
    res = whd_wifi_get_ioctl_buffer(sta_interface, ioctl, buffer, len );
    return res;
}

#ifdef __cplusplus
}
#endif

/* [] END OF FILE */
