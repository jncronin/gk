/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
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

#ifndef HAVE_GETTIMEOFDAY

#if defined(HAVE_CONFIG_H) && !defined(INCLUDED_CONFIG_H_)
/* NOTE: config.h doesn't have guard includes! */
#define INCLUDED_CONFIG_H_
#include "config.h"
#endif /* defined(HAVE_CONFIG_H) && !defined(INCLUDED_CONFIG_H_) */

#include "headers.h"
#include "gettimeofday.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int anycloud_gettimeofday(struct timeval* tv, void* timezone)
{
    cy_time_t time_ms;

    (void)timezone; /* Unused parameter */
    (void) cy_rtos_get_time(&time_ms);
    tv->tv_sec =  ( time_ms / 1000 );
    tv->tv_usec = ( time_ms - ( tv->tv_sec * 1000 ) ) * 1000;

    return 0;
}

#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif /* !defined(HAVE_GETTIMEOFDAY) */
