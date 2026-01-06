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

#ifndef MFG_TEST_H_
#define MFG_TEST_H_

/* Used in cdc_ioctl_t.flags field. */
#define REMOTE_SET_IOCTL			1
#define REMOTE_GET_IOCTL			2
#define REMOTE_REPLY				4
#define REMOTE_SHELL_CMD			8
#define REMOTE_FINDSERVER_IOCTL		16 /* Find the remote server. */
#define REMOTE_ASD_CMD				32 /* ASD integration. */
#define RDHD_SET_IOCTL			64
#define RDHD_GET_IOCTL			128
#define REMOTE_VISTA_CMD			256 /* For remote vista-specific commands. */
#define REMOTE_NEGOTIATE_CMD			512 /* For RWL negotiation. */
#define NEGOTIATE_GET_OS			0   /* Detect the remote OS. */


#ifndef INTF_NAME_SIZ
#define INTF_NAME_SIZ	16
#endif

#ifndef MFGTEST_NO_HARDWARE
#define MFG_DPRINT_ERR(a, args...)
#define MFG_DPRINT_INFO(a, args...)
#define MFG_DPRINT_DBG(a, args...)      printf((args))
#else /* MFGTEST_NO_HARDWARE */
#define MFG_DPRINT_ERR(a, ...)
#define MFG_DPRINT_INFO(a, ...)
#define MFG_DPRINT_DBG(a, ...)
#endif /* MFGTEST_NO_HARDWARE */

#define ERR		0
#define OUTPUT	0

/******************************************************
 *                    Structures
 ******************************************************/
 /** wl tool command data. */
typedef struct cdc_ioctl {
	uint32_t cmd;      /**< ioctl command value. */
	uint32_t len;      /**< Lower 16: output buflen; upper 16: input buflen (excludes header). */
	uint32_t flags;    /**< Flag defines given below. */
	uint32_t status;   /**< Status code returned from the device. */
} cdc_ioctl_t;

/** Used to send ioctls over the transport pipe. */
typedef struct remote_ioctl {
	cdc_ioctl_t	msg;     /**< ioctl message. */
	uint32_t    data_len;    /**< Length of the wl tool data. */
	char        intf_name[INTF_NAME_SIZ];   /**< Interface name. */
} rem_ioctl_t;
#define REMOTE_SIZE	sizeof(rem_ioctl_t)

#define MCSSET_LEN    16
#define ETHER_ADDR_LENGTH 6

#define WL_OTPREAD_VER 1

/** Used to send the otp read command over the transport pipe. */
typedef struct {
	uint16_t version;	    /**< cmd structure version. */
	uint16_t cmd_len;	    /*<* cmd struct len. */
	uint32_t rdmode;            /**< otp read mode. */
	uint32_t rdoffset;	    /**< Byte offset into the otp to start read. */
	uint32_t rdsize;            /**< Number of bytes to read. */
} wltool_otpread_cmd_t;

#endif /* MFG_TEST_H_ */
