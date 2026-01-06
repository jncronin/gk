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
 * Description: This file is the public interface of mfg_test_comp_ioctl.c.
 *
 * Related Document: See README.md.
  *
 */

#ifndef MFG_TEST_COMP_IOCTL_H_
#define MFG_TEST_COMP_IOCTL_H_


/*******************************************************************************
* Macros
********************************************************************************/

/*******************************************************************************
* Function Prototypes
********************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/** This function is the interface for IOCTL commands to the wlan firmware.
 *
 * @param cmd         : cmd index of the command.
 * @param buf         : Pointer to the binary data buffer pointer.
 * @param len         : Length of the command data.
 * @param set         : Set flag.
 * @param outlen      : Pointer to provide the length of the data written.
 * @return            : Success or failure status of the handling command.
 *
 *******************************************************************************/
int wl_ioctl( int cmd, void *buf, int len, bool set, int *outlen);
/** This function sets the pointer to the interface in the shared code for IOVAR/IOCTL.
 *
 * @param interface   : Pointer to the interface handle.
 *
 *******************************************************************************/
void wl_ioctl_set_sta_interface_handle( void *interface);

#ifdef __cplusplus
}
#endif

#endif /* MFG_TEST_COMP_IOCTL_H_ */

/* [] END OF FILE */
