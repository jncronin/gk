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

/*******************************************************************************
 * @file
 * Header for the natural numbers library
 *******************************************************************************
 */

#ifndef INCLUDED_NN_H
#define INCLUDED_NN_H

#include <stdint.h>

#include "cy_wps_common.h"
#include "cy_wps_structures.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t len;
    uint32_t num[1];
} NN_t;

void     NN_Clr          ( NN_t* number );
uint32_t NN_Add          ( NN_t* result, const NN_t*x, const NN_t*y );
uint32_t NN_Sub          ( NN_t* result, const NN_t*x, const NN_t*y );
void     NN_Mul          ( NN_t* result, const NN_t*x, const NN_t*y );
void     NN_AddMod       ( NN_t* result, const NN_t*x, const NN_t*y, const NN_t*modulus );
void     NN_SubMod       ( NN_t* result, const NN_t*x, const NN_t*y, const NN_t*modulus );
void     NN_MulMod       ( NN_t* result, const NN_t*x, const NN_t*y, const NN_t*modulus );
void     NN_ExpMod       ( NN_t* result, NN_t*x, NN_t*modulus, NN_t*e, NN_t*w );
void     NN_MulModMont   ( NN_t* result, const NN_t*x, const NN_t*y, const NN_t*m, uint32_t t );
void     NN_ExpModMont   ( NN_t* result, NN_t*x, NN_t*m, NN_t*e, NN_t*w );
uint32_t NN_EmTick       ( const NN_t* mod );
void     NN_ErModEm      ( NN_t* result, const NN_t*m );
uint64_t NN_Mul32x32u64  ( uint32_t a, uint32_t b );
void     wps_NN_set      ( cy_wps_NN_t* m, const uint8_t* buffer);
void     wps_NN_get      ( const cy_wps_NN_t* m, uint8_t* buffer);

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif /* ifndef INCLUDED_NN_H */

