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
 * tcp_window_size.c
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * set/getsockopt
 * ------------------------------------------------------------------- */

#include "headers.h"

/* IPERF_MODIFIED Start */
/* As build system looks for all the header files with the name and adding include paths. It includes mutex.h present in some other component
 * Hence files are renamed by appending iperf.
 */
#include "iperf_util.h"
#include "iperf_sockets.h"
/* IPERF_MODIFIED End */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------
 * If inTCPWin > 0, set the TCP window size (via the socket buffer
 * sizes) for inSock. Otherwise leave it as the system default.
 *
 * This must be called prior to calling listen() or connect() on
 * the socket, for TCP window sizes > 64 KB to be effective.
 *
 * This now works on UNICOS also, by setting TCP_WINSHIFT.
 * This now works on AIX, by enabling RFC1323.
 * returns -1 on error, 0 on no error.
 * ------------------------------------------------------------------- */

int setsock_tcp_windowsize( int inSock, int inTCPWin, int inSend ) {
#ifdef SO_SNDBUF
    int rc;
    int newTCPWin;

    assert( inSock >= 0 );

    if ( inTCPWin > 0 ) {

#ifdef TCP_WINSHIFT

        /* UNICOS requires setting the winshift explicitly */
        if ( inTCPWin > 65535 ) {
            int winShift = 0;
            int scaledWin = inTCPWin >> 16;
            while ( scaledWin > 0 ) {
                scaledWin >>= 1;
                winShift++;
            }

            /* set TCP window shift */
            rc = setsockopt( inSock, IPPROTO_TCP, TCP_WINSHIFT,
                             (char*) &winShift, sizeof( winShift ));
            if ( rc < 0 ) {
                return rc;
            }

            /* Note: you cannot verify TCP window shift, since it returns
             * a structure and not the same integer we use to set it. (ugh) */
        }
#endif /* TCP_WINSHIFT  */

#ifdef TCP_RFC1323
        /* On AIX, RFC 1323 extensions can be set system-wide,
         * using the 'no' network options command. But we can also set them
         * per-socket, so let's try just in case. */
        if ( inTCPWin > 65535 ) {
            /* enable RFC 1323 */
            int on = 1;
            rc = setsockopt( inSock, IPPROTO_TCP, TCP_RFC1323,
                             (char*) &on, sizeof( on ));
            if ( rc < 0 ) {
                return rc;
            }
        }
#endif /* TCP_RFC1323 */

        if ( !inSend ) {
            /* receive buffer -- set
             * note: results are verified after connect() or listen(),
             * since some OS's don't show the corrected value until then. */
            newTCPWin = inTCPWin;
            /* IPERF_MODIFIED Start */
            rc = iperf_setsockopt( inSock, SOL_SOCKET, SO_RCVBUF,
                             (char*) &newTCPWin, sizeof( newTCPWin ));
            /* IPERF_MODIFIED End */
        } else {
            /* send buffer -- set
             * note: results are verified after connect() or listen(),
             * since some OS's don't show the corrected value until then. */
            newTCPWin = inTCPWin;
            /* IPERF_MODIFIED Start */
            rc = iperf_setsockopt( inSock, SOL_SOCKET, SO_SNDBUF,
                             (char*) &newTCPWin, sizeof( newTCPWin ));
            /* IPERF_MODIFIED End */
        }
        if ( rc < 0 ) {
            return rc;
        }
    }
/* IPERF_MODIFIED Start */
#else /* ifndef SO_SNDBUF */
    fprintf( stderr, "Unable to set socket TCP window size on this RTOS - SO_SNDBUF is undefined.\n" );
#endif /* SO_SNDBUF */
/* IPERF_MODIFIED End */
    return 0;
} /* end setsock_tcp_windowsize */

/* -------------------------------------------------------------------
 * returns the TCP window size (on the sending buffer, SO_SNDBUF),
 * or -1 on error.
 * ------------------------------------------------------------------- */

int getsock_tcp_windowsize( int inSock, int inSend ) {
    int theTCPWin = 0;

#ifdef SO_SNDBUF
    int rc;
    Socklen_t len;

    /* send buffer -- query for buffer size */
    len = sizeof( theTCPWin );
    if ( inSend ) {
        /* IPERF_MODIFIED Start */
        rc = iperf_getsockopt( inSock, SOL_SOCKET, SO_SNDBUF,
                               (char*) &theTCPWin, &len );
        /* IPERF_MODIFIED End */
    } else {
        /* IPERF_MODIFIED Start */
        rc = iperf_getsockopt( inSock, SOL_SOCKET, SO_RCVBUF,
                               (char*) &theTCPWin, &len );
        /* IPERF_MODIFIED End */
    }
    if ( rc < 0 ) {
        return rc;
    }

#endif

    return theTCPWin;
} /* end getsock_tcp_windowsize */

#ifdef __cplusplus
} /* end extern "C" */
#endif

