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

#ifndef CONFIG_H
#define CONFIG_H

/* ===================================================================
 * config.h
 *
 * config.h is derived from config.h.in -- do not edit config.h
 *
 * This contains variables that the configure script checks and
 * then defines or undefines. The source code checks for these
 * variables to know if certain features are present.
 * =================================================================== */

/* Define if threads exist (using pthreads or Win32 threads) */
/* #undef HAVE_THREAD */
/* #undef HAVE_POSIX_THREAD */
/* #undef HAVE_WIN32_THREAD */
/* #undef _REENTRANT */

/* Define if on OSF1 and need special extern "C" around some header files */
/* #undef SPECIAL_OSF1_EXTERN */

/* Define if the strings.h header file exists */

#define HAVE_STRINGS_H

#if defined(__ARMCC_VERSION) && !defined(SSIZE_MAX)
typedef signed   int  ssize_t;  ///< Signed size type, usually encodes negative errors
#endif

#ifndef HAVE_SNPRINTF
#define HAVE_SNPRINTF
#endif

#define HAVE_INT64_T

/* Define if you have these functions. */
/* #define HAVE_SNPRINTF */
/* #undef HAVE_INET_PTON */
/* #undef HAVE_INET_NTOP */

/* #undef HAVE_GETTIMEOFDAY */
#define gettimeofday(tv, timezone)      anycloud_gettimeofday(tv, timezone)

typedef unsigned long useconds_t;

/* #undef HAVE_PTHREAD_CANCEL */
/* #undef HAVE_USLEEP */
/* #undef HAVE_QUAD_SUPPORT */
/* #undef HAVE_PRINTF_QD */

/* standard C++, which isn't always... */
/* #undef bool */

#endif /* CONFIG_H */
