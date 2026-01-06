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
 * util.h
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * various C utility functions.
 * ------------------------------------------------------------------- */

#ifndef UTIL_H
#define UTIL_H

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

/* IPERF_MODIFIED Start */
#include "headers.h"
#include <stdbool.h>
#include <cyabs_rtos.h>
#include "iperf_sockets.h"
/* IPERF_MODIFIED End */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------
 * set/getsockopt wrappers for SO_RCVBUF and SO_SNDBUF; TCP_MAXSEG
 * socket.c
 * ------------------------------------------------------------------- */
int setsock_tcp_windowsize( int inSock, int inTCPWin, int inSend );
int getsock_tcp_windowsize( int inSock, int inSend );

void setsock_tcp_mss( int inSock, int inTCPWin );
int  getsock_tcp_mss( int inSock );
bool setsock_blocking(int fd, bool blocking);

int recvn( int inSock, char *outBuf, int inLen, int flags );
/* -------------------------------------------------------------------
 * signal handlers
 * signal.c
 * ------------------------------------------------------------------- */
typedef void Sigfunc(int);
void sig_exit( int inSigno );

typedef Sigfunc *SigfuncPtr;

SigfuncPtr my_signal( int inSigno, SigfuncPtr inFunc );

#ifdef WIN32

#ifdef HAVE_SIGNAL_H
  #define _NSIG NSIG
#else
/* under windows, emulate unix signals */
enum {
    SIGINT,
    SIGTERM,
    SIGPIPE,
    _NSIG
};
#endif

BOOL WINAPI sig_dispatcher( DWORD type );

#endif

/* -------------------------------------------------------------------
 * error handlers
 * error.c
 * ------------------------------------------------------------------- */
void warn      ( const char *inMessage, const char *inFile, int inLine );
void warn_errno( const char *inMessage, const char *inFile, int inLine );

/*
 * The STRINGIFY and TOSTRING macros were adopted from
 * http://www.decompile.com/cpp/faq/file_and_line_error_string.htm
 */
#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY( x )

/** Print a standard message to the console */
#define IPERF_BASIC_MSG(msg) do { printf msg; } while(0)

/*
 * Print a message to the console, together with the file name and line number 
 * from which the message originated, and the thread number of the thread that
 * is printing the message.
 */
#include "iperf_thread.h"
#define IPERF_DETAILED_MSG(msg)                                   \
	    IPERF_BASIC_MSG(("[" __FILE__ ":" TOSTRING(__LINE__) "]: "));       \
	    IPERF_BASIC_MSG(("{Thread %lu} ", (unsigned long) thread_getid())); \
	    IPERF_BASIC_MSG(msg)

#ifdef NDEBUG
    /*
     * If we are not in debug build, then we want nice-looking messages.
     */
    #define IPERF_MSG(msg) IPERF_BASIC_MSG(msg)
#else
    /*
     * In debug messages, we want messages that contain some extra useful 
     * information, hence we will use IPERF_DETAILED_MSG instead of IPERF_MSG.
     */
    #define IPERF_MSG(msg) IPERF_DETAILED_MSG(msg)
#endif /* NDEBUG */


/* FAIL */
    #ifdef HAVE_THREAD
        #define FAIL(cond, msg, settings)  \
            do {                           \
                if (cond) {                \
                    IPERF_MSG(msg);        \
                    thread_stop(settings); \
                }                          \
            } while (0)
    #else
        #define FAIL(cond, msg, settings)  \
            do {                           \
                if (cond) {                \
                    printf((msg));         \
                    cy_rtos_exit_thread(); \
                }                          \
            } while (0)
    #endif /* HAVE_THREAD */

/* WARN */
    #define WARN(cond, msg)     \
        do {                    \
            if (cond) {         \
                printf((msg));  \
            }                   \
        } while (0)

/* FAIL_errno */
    #ifdef HAVE_THREAD
        #define FAIL_errno(cond, msg, settings) \
            do {                                \
                if (cond) {                     \
                    IPERF_MSG(msg);             \
                    thread_stop(settings);      \
                }                               \
            } while (0)
    #else
        #define FAIL_errno(cond, msg, settings) \
            do {                                \
                if (cond) {                     \
                    printf((msg));              \
                    cy_rtos_exit_thread();      \
                }                               \
            } while (0)
    #endif /* HAVE_THREAD */

/* WARN_errno */
    #define WARN_errno(cond, msg) \
        do {                      \
            if (cond) {           \
                printf((msg));    \
            }                     \
        } while (0)

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(x) (void) x
#endif /* UNUSED_PARAMETER */

/* -------------------------------------------------------------------
 * initialize buffer to a pattern
 * ------------------------------------------------------------------- */
void pattern( char *outBuf, int inBytes );

/* -------------------------------------------------------------------
 * input and output numbers, converting with kilo, mega, giga
 * stdio.c
 * ------------------------------------------------------------------- */
double byte_atof( const char *inString );
double bitorbyte_atof( const char *inString );
intmax_t byte_atoi( const char  *inString );
unsigned int bitorbyte_atoi( const char *inString );
void byte_snprintf( char* outString, int inLen, double inNum, char inFormat );

/* -------------------------------------------------------------------
 * redirect the stdout to a specified file
 * stdio.c
 * ------------------------------------------------------------------- */
void redirect(const char *inOutputFileName);

/* -------------------------------------------------------------------
 * delete macro
 * ------------------------------------------------------------------- */
#define DELETE_PTR( ptr )                       \
  do {                                          \
    if ( ptr != NULL ) {                        \
      delete ptr;                               \
      ptr = NULL;                               \
    }                                           \
  } while( false )

#define DELETE_ARRAY( ptr )                     \
  do {                                          \
    if ( ptr != NULL ) {                        \
      delete [] ptr;                            \
      ptr = NULL;                               \
    }                                           \
  } while( false )

#define FREE_ARRAY( ptr )                     \
  do {                                          \
    if ( ptr != NULL ) {                        \
      free(ptr); \
      ptr = NULL;                               \
    }                                           \
  } while( false )

/* IPERF_MODIFIED Start */
#define FREE_PTR(ptr)      \
    do {                   \
        if (ptr != NULL) { \
            free(ptr);     \
            ptr = NULL;    \
        }                  \
    } while (0)
/* IPERF_MODIFIED End */
#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif /* UTIL_H */
