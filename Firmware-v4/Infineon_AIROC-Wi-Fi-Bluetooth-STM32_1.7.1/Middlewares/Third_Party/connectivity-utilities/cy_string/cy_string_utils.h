/*
 * Copyright 2019-2024, Cypress Semiconductor Corporation (an Infineon company) or
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

/*
 * @file
 * The string utilities module is a collection of string conversion helpers to convert between integer and strings
 *
 */

#ifndef __CY_STRING_H__
#define __CY_STRING_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup string_utils 
 * The string utilities module is a collection of string conversion helpers to convert between integer and strings.
 */

/*****************************************************************************/
/**
 *
 *  @addtogroup group_string_func
 *
 * The string utilities module is a collection of string conversion helpers to convert between integer and strings
 *
 *  @{
 */
/*****************************************************************************/

/*!
 ******************************************************************************
 * Convert a decimal or hexidecimal string to an integer.
 *
 * @param[in] str  The string containing the value.
 *
 * @return    The value represented by the string.
 */
uint32_t cy_generic_string_to_unsigned( const char* str );

/**
 * Converts a decimal/hexidecimal string (with optional sign) to a signed long int
 * Better than strtol or atol or atoi because the return value indicates if an error occurred
 *
 * @param[in] string     : The string buffer to be converted
 * @param[in] str_length : The maximum number of characters to process in the string buffer
 * @param[out] value_out : The unsigned in that will receive value of the the decimal string
 * @param[in] is_hex     : 0 = Decimal string, 1 = Hexidecimal string
 *
 * @return the number of characters successfully converted (including sign).  i.e. 0 = error
 *
 */
uint8_t cy_string_to_signed( const char* string, uint16_t str_length, int32_t* value_out, uint8_t is_hex );


/**
 * Converts a decimal/hexidecimal string to an unsigned long int
 * Better than strtol or atol or atoi because the return value indicates if an error occurred
 *
 * @param[in] string     : The string buffer to be converted
 * @param[in] str_length : The maximum number of characters to process in the string buffer
 * @param[out] value_out : The unsigned in that will receive value of the the decimal string
 * @param[in] is_hex     : 0 = Decimal string, 1 = Hexidecimal string
 *
 * @return the number of characters successfully converted.  i.e. 0 = error
 *
 */
uint8_t cy_string_to_unsigned( const char* string, uint8_t str_length, uint32_t* value_out, uint8_t is_hex );

/** @} */

#ifdef __cplusplus
}
#endif


#endif /* __CY_STRING_H__ */
