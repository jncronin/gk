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

/*---------------------------------------------------------------
 * Copyright (c) 1999,2000,2001,2002,2003
 * The Board of Trustees of the University of Illinois
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (Iperf) and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the names of the University of Illinois, NCSA,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 * National Laboratory for Applied Network Research
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * http://www.ncsa.uiuc.edu
 * ________________________________________________________________
 *
 * headers.h
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * All system headers required by iperf.
 * This could be processed to form a single precompiled header,
 * to avoid overhead of compiling it multiple times.
 * This also verifies a few things are defined, for portability.
 * ------------------------------------------------------------------- */

#ifndef HEADERS_H
#define HEADERS_H

/* IPERF_MODIFIED Start */
#define HAVE_INET_NTOP 1
#define HAVE_INET_PTON 1

/* IPERF_MODIFIED End */

#ifdef __cplusplus
extern "C" {
#endif

#include "iperf_netdb_internal.h"
#include "iperf_sockets.h"

#ifdef __cplusplus
}
#endif

/* turn off assert debugging if we aren't debugging */
#if !defined(NDEBUG) && !defined(DEBUG)
    #define NDEBUG
#endif /* !defined(NDEBUG) && !defined(DEBUG) */

#ifdef WIN32

/* Windows config file */
//    #include "config.win32.h"

/* Windows headers */
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
    #define _WIN32_WINNT 0x0501 /* use (at least) WinXP API */
    #define WIN32_LEAN_AND_MEAN /* exclude unnecesary headers */
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <process.h>

/* define EINTR, just to help compile; it isn't useful */
    #ifndef EINTR
        #define EINTR  WSAEINTR
    #endif // EINTR

/* Visual C++ has INT64, but not 'long long'.
 * Metrowerks has 'long long', but INT64 doesn't work. */
    #ifdef __MWERKS__
        #define int64_t  long long
    #else
        #define int64_t  INT64
    #endif // __MWERKS__

/* Visual C++ has _snprintf instead of snprintf */
    #ifndef __MWERKS__
        #define snprintf _snprintf
    #endif // __MWERKS__

/* close, read, and write only work on files in Windows.
 * I get away with #defining them because I don't read files. */
    #define close( s )       closesocket( s )
    #define read( s, b, l )  recv( s, (char*) b, l, 0 )
    #define write( s, b, l ) send( s, (char*) b, l, 0 )

/* usleep is in unistd.h, but that would conflict with the
close/read/write redefinitin above */
int usleep(useconds_t usec);
#else
    /* RTOS configuration file */
    #include "rtos_config.h"
    /* Disable some unneeded/unimplemented features */
    #ifndef NO_SIGNALS
        #define NO_SIGNALS
    #endif /* NO_SIGNALS */
    #ifndef NO_FILE_IO
        #define NO_FILE_IO
    #endif /* NO_FILE_IO */
    #ifndef NO_DAEMON
        #define NO_DAEMON
    #endif /* NO_DAEMON */
    #ifndef NO_ENVIRONMENT
        #define NO_ENVIRONMENT
    #endif /* NO_ENVIRONMENT */
    #ifndef NO_EXIT
        #define NO_EXIT
    #endif /* NO_EXIT */
    #ifndef NO_INTERRUPTS
        #define NO_INTERRUPTS
    #endif /* NO_INTERRUPTS */
    #ifndef NO_SERVICE
        #define NO_SERVICE
    #endif /* NO_SERVICE */
    #ifndef NO_ITIMER
        #define NO_ITIMER
    #endif /* NO_ITIMER */

    #ifndef NO_THREADS
        #define NO_THREADS
    #endif /* NO_THREADS */

    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR   -1
    #endif /* SOCKET_ERROR */
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET -1
    #endif /* INVALID_SOCKET */

    /* standard C headers */
    #include <stdlib.h>
    #include <stdio.h>
    #include <assert.h>
    #include <stdint.h>
    #include <ctype.h>
    #include <errno.h>
    #include <string.h>
    #include <time.h>
    #include <math.h>

/* IPERF_MODIFIED_Start */
typedef socklen_t     Socklen_t;
/* IPERF_MODIFIED_End */

#endif /* not defined WIN32 */

#ifndef INET6_ADDRSTRLEN
    #define INET6_ADDRSTRLEN 40
#endif /* INET6_ADDRSTRLEN */
#ifndef INET_ADDRSTRLEN
    #define INET_ADDRSTRLEN 15
#endif /* INET_ADDRSTRLEN */

//#ifdef __cplusplus
    #ifdef HAVE_IPV6
        #define REPORT_ADDRLEN (INET6_ADDRSTRLEN + 1)
typedef struct sockaddr_storage iperf_sockaddr;
    #else
        #define REPORT_ADDRLEN (INET_ADDRSTRLEN + 1)
typedef struct sockaddr_in iperf_sockaddr;
    #endif /* HAVE_IPV6 */
//#endif

/* we don't want the main function to be called "main" */
#define IPERF_MAIN iperf
// Rationalize stdint definitions and sizeof, thanks to ac_create_stdint_h.m4
// from the gnu archive

#include <stdint.h>
typedef uint64_t max_size_t;

// inttypes.h is already included

#ifdef HAVE_FASTSAMPLING
#define IPERFTimeFrmt "%4.4f-%4.4f"
#define IPERFTimeSpace "            "
#else
#define IPERFTimeFrmt "%4.2f-%4.2f"
#define IPERFTimeSpace "        "
#endif

/* in case the OS doesn't have these, we provide our own implementations */
#include "gettimeofday.h"
#include "snprintf.h"

#ifndef SHUT_RD
    #define SHUT_RD   0
    #define SHUT_WR   1
    #define SHUT_RDWR 2
#endif /* SHUT_RD */

#define IPERF_BUFFERLEN             (2 * 1024)
#define NUM_REPORT_STRUCTS          (10)
#define NUM_MULTI_SLOTS             (5)

#include "header_version.h"
#define HEADER_VERSION              HEADER_VERSION1

#include "iperf_debug.h"

/*============================================================================*/
/* Sanity checks                                                              */
/*============================================================================*/

/* Make sure that we only have one thread type defined */
#define __NUM_THREAD_TYPES_0 0
#ifdef HAVE_POSIX_THREAD
    #define __NUM_THREAD_TYPES_1 __NUM_THREAD_TYPES_0 + 1
#else
    #define __NUM_THREAD_TYPES_1 __NUM_THREAD_TYPES_0
#endif /* HAVE_POSIX_THREAD */
#ifdef HAVE_WIN32_THREAD
    #define __NUM_THREAD_TYPES_2 __NUM_THREAD_TYPES_1 + 1
#else
    #define __NUM_THREAD_TYPES_2 __NUM_THREAD_TYPES
#endif /* HAVE_WIN32_THREAD */
#ifdef HAVE_FREERTOS_THREAD
    #define __NUM_THREAD_TYPES_3 __NUM_THREAD_TYPES_2 + 1
#else
    #define __NUM_THREAD_TYPES_3 __NUM_THREAD_TYPES_2
#endif /* HAVE_FREERTOS_THREAD */
#ifdef HAVE_THREADX_THREAD
    #define __NUM_THREAD_TYPES_4 __NUM_THREAD_TYPES_3 + 1
#else
    #define __NUM_THREAD_TYPES_4 __NUM_THREAD_TYPES_3
#endif /* HAVE_THREADX_THREAD */
#define __NUM_THREAD_TYPES __NUM_THREAD_TYPES_4

#if __NUM_THREAD_TYPES > 1
#error "Too many thread types defined."
#endif /* __NUM_THREAD_TYPES > 1 */
/* IPERF_MODIFIED End */
#endif /* HEADERS_H */
