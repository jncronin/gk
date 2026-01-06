/*
 * Copyright 2017, Cypress Semiconductor Corporation or a subsidiary of 
 * Cypress Semiconductor Corporation. All Rights Reserved.
 * 
 * This software, associated documentation and materials ("Software"),
 * is owned by Cypress Semiconductor Corporation
 * or one of its subsidiaries ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products. Any reproduction, modification, translation,
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
 *  NVRAM variables taken from BCM943438WLPTH nvram file (2.4 GHz, 20 MHz BW mode)
 */

#ifndef INCLUDED_NVRAM_IMAGE_H_
#define INCLUDED_NVRAM_IMAGE_H_

#include <string.h>
#include <stdint.h>
#include "generated_mac_address.txt"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef APP_TEST_CONSOLE
//#define NVRAM_PWR_19DB
#endif //APP_TEST_CONSOLE

#if defined(NVRAM_PWR_16DB)
#pragma message "Warning TX power set for 16dB"
#elif defined(NVRAM_PWR_17DB)
#pragma message "Warning TX power set for 17dB"
#elif defined(NVRAM_PWR_18DB)
#pragma message "Warning TX power set for 18dB"
#elif defined(NVRAM_PWR_19DB)
#pragma message "Warning TX power set for 19dB"
#else
#pragma message "Default TX power set for 20dB"
#endif

/**
 * Character array of NVRAM image
 */
static const char wifi_nvram_image[] =
        // # The following parameter values are just placeholders, need to be updated.
        "manfid=0x2d0"                                                       "\x00"
        "prodid=0x0726"                                                      "\x00"
        "vendid=0x14e4"                                                      "\x00"
        "devid=0x43e2"                                                       "\x00"
        "boardtype=0x0726"                                                   "\x00"
        "boardrev=0x1101"                                                    "\x00"
        "boardnum=22"                                                        "\x00"
		"xtalfreq=37400"                                                     "\x00"
        "sromrev=11"                                                         "\x00"
        "boardflags=0x00404201"                                              "\x00"
	    "boardflags3=0x04000000"                                             "\x00"
        NVRAM_GENERATED_MAC_ADDRESS                                          "\x00"
        "nocrc=1"                                                            "\x00"
        "ag0=0"                                                              "\x00"
        "aa2g=1"                                                             "\x00"
        "ccode=ALL"														     "\x00"
        "swdiv_en=1"                                                         "\x00"
        "swdiv_gpio=1"                                                       "\x00"
        "pa0itssit=0x20"                                                     "\x00"
        "extpagain2g=0"                                                      "\x00"
        //#PA parameters for 2.4GHz, measured at CHIP OUTPUT
		 "pa2ga0=-165,6894,-786"           	                            	 "\x00"
        "AvVmid_c0=0x0,0xc8"                                                 "\x00"
        "cckpwroffset0=5"                                                    "\x00"
        //# PPR params
#if defined(NVRAM_PWR_16DB)
        "maxp2ga0=64"                                                        "\x00"
#elif defined(NVRAM_PWR_17DB)
        "maxp2ga0=68"                                                        "\x00"
#elif defined(NVRAM_PWR_18DB)
        "maxp2ga0=72"                                                        "\x00"
#elif defined(NVRAM_PWR_19DB)
        "maxp2ga0=76"                                                        "\x00"
#else
        "maxp2ga0=80"                                                        "\x00"
#endif
        "txpwrbckof=6"                                                       "\x00"
        "cckbw202gpo=0"                                                      "\x00"
        "legofdmbw202gpo=0x66111111"                                         "\x00"
        "mcsbw202gpo=0x77711111"                                             "\x00"
        "propbw202gpo=0xdd"                                                  "\x00"
        //# OFDM IIR :
        "ofdmdigfilttype=18"                                                 "\x00"
        "ofdmdigfilttypebe=18"                                               "\x00"
        //# PAPD mode:
        "papdmode=1"                                                         "\x00"
        "papdvalidtest=1"                                                    "\x00"
        "pacalidx2g=48"                                                      "\x00"
        "papdepsoffset=-22"                                                  "\x00"
        "papdendidx=58"                                                      "\x00"
        //# LTECX flags
       // "ltecxmux=1"                                                         "\x00"
        //"ltecxpadnum=0x02030401"                                             "\x00"
       // "ltecxfnsel=0x3003"                                                  "\x00"
       // "ltecxgcigpio=0x3012"                                                "\x00"
        //#il0macaddr=00:90:4c:c5:12:38
        "wl0id=0x431b"                                                       "\x00"
        "deadman_to=0xffffffff"                                              "\x00"
        //# muxenab: 0x1 for UART enable, 0x2 for GPIOs, 0x8 for JTAG, 0x10 for HW OOB
        "muxenab=0x11"                                                        "\x00"
        //# CLDO PWM voltage settings - 0x4 - 1.1 volt
        //#cldo_pwm=0x4                                                      "\x00"
        //#VCO freq 326.4MHz
        "spurconfig=0x3"                                                     "\x00"
        "\x00\x00";


#ifdef __cplusplus
} /* extern "C" */
#endif

#else /* ifndef INCLUDED_NVRAM_IMAGE_H_ */

#error Wi-Fi NVRAM image included twice

#endif /* ifndef INCLUDED_NVRAM_IMAGE_H_ */
