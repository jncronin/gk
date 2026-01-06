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

#include "cy_result.h"
#ifndef WPA3_EXT_NO_HARDWARE
#include "whd_endian.h"
#else
#include <stdbool.h>
#endif

#ifndef CYPRESS_WEAK
#if defined(__ICCARM__)
#define CYPRESS_WEAK            __WEAK
#define CYPRESS_PACKED(struct)  __packed struct
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define CYPRESS_WEAK            __attribute__((weak))
#define CYPRESS_PACKED(struct)  struct __attribute__((packed))
#else
#define CYPRESS_WEAK           __attribute__((weak))
#define CYPRESS_PACKED(struct) struct __attribute__((packed))
#endif  /* defined(__ICCARM__) */
#endif /* CYPRESS_WEAK */

#define WPA3_EXT_SUPP_RSLT_SUCCESS 0
#define WPA3_EXT_SUPP_RSLT_NO_MEM  1
#define WPA3_EXT_SUPP_RSLT_AUTH_EXCHG_FAIL 2
#define WPA3_EXT_SUPP_RSLT_AUTH_BAD_ALGO 3
#define WPA3_EXT_SUPP_RSLT_SCALAR_ELEMENT_RANGE_ERROR 4
#define WPA3_EXT_SUPP_ERROR 5
#define WPA3_EXT_CRYPTO_ERROR 6
#define WPA3_EXT_PWE_GEN_FAILED 7
#define WPA3_EXT_SUPP_CONFIRM_VERIFY_FAILURE  9
#define WPA3_EXT_SUPP_SILENTLY_DISCARD 10

#define WPA3_DEFINE_PLUS         1      /**< positive sign bit */
#define WPA3_DEFINE_MINUS       -1      /**< negative sign bit */

//#define WPA3_EXT_LOG_ENABLE
//#define WPA3_EXT_SUPPLICANT_DEBUG

#ifdef WPA3_EXT_LOG_ENABLE
#define WPA3_EXT_LOG_MSG(args) { printf args;}
#else
#define WPA3_EXT_LOG_MSG(args)
#endif

//#define WPA3_EXT_HEX_LOG_ENABLE

#ifdef WPA3_EXT_HEX_LOG_ENABLE
#define WPA3_EXT_HEX_MPI_DUMP(args) {wpa3_print_mbedtls_mpi args; }
#define WPA3_EXT_HEX_BUF_DUMP(args) {wpa3_print_buf args; }
#else
#define WPA3_EXT_HEX_MPI_DUMP(args)
#define WPA3_EXT_HEX_BUF_DUMP(args)
#endif

/** This function allocates buffer
 * @param   buf               : The pointer to the whd buffer
 * @param   size              : The size of the buffer
 * @return  cy_rslt_t         : CY_RSLT_SUCESS
 *                            : CY_RSLT_MW_ERROR
 *
 *******************************************************************************/
cy_rslt_t wpa3_buffer_alloc( whd_buffer_t *buf, uint16_t size);

/** This function frees buffer
 * @param   buf               : The pointer to the whd buffer
 * @return  cy_rslt_t         : CY_RSLT_SUCESS
 *                            : CY_RSLT_MW_ERROR
 *
 *******************************************************************************/
cy_rslt_t wpa3_buffer_free( whd_buffer_t buf);

/** This function implements constant time memcmp
 * @param   a       : The pointer to a
 * @param   b       : The pointer to b
 * @param  len      : The length of the data to compare
 * @return  int     : The result is 0 if a == b else non-zero
 *
 *******************************************************************************/
int wpa3_constant_time_memcmp( uint8_t *a, uint8_t *b, uint16_t len );

/** This function checks if the buffer is odd
 * @param   buf     : The pointer to buffer
 * @param   len     : The length of the buffer
 * @return  bool    : The result is true if odd else false
 *
 *******************************************************************************/
bool wpa3_is_buf_val_odd(uint8_t * buf, uint16_t len);

/** This function prints the buffer
 * @param   buf     : The pointer to buffer
 * @param   len     : The length of the buffer
 *
 *******************************************************************************/
void wpa3_print_buf(uint8_t *buf, int len);

/** This function prints the mbedtls_mpi
 * @param   n       : The pointer to mbedtls_mpi
 *
 *******************************************************************************/
void wpa3_print_mbedtls_mpi(mbedtls_mpi *n);

/** This function compares values in constant time.
 * @param  a        : The value of a
 * @param  b        : The value of b
 * @return          : returns 0 if both are same else 1
 *
 *******************************************************************************/
int wpa3_const_time_int_cmp(uint8_t a, uint8_t b);
