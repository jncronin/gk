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
 * socket.c
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * set/getsockopt and read/write wrappers
 * ------------------------------------------------------------------- */

/* IPERF_MODIFIED Start */

#include "iperf_util.h"
#include "iperf_sockets_internal.h"
/* IPERF_MODIFIED End */
#include "headers.h"
#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------
 * If inMSS > 0, set the TCP maximum segment size  for inSock.
 * Otherwise leave it as the system default.
 * ------------------------------------------------------------------- */

/* IPERF_MODIFIED Start */
#if defined(__ARMCC_VERSION)
#define EINTR           4       /* Interrupted system call */
#define EAGAIN          11      /* Try again */
#endif
/* fcntl.h defines */
#define F_GETFL 3
#define F_SETFL 4
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0004       ///< Non-blocking mode
#endif
/* IPERF_MODIFIED End */

/* IPERF_MODIFIED Start */
const char warn_mss_no_attempt[] = "\
WARNING: attempt to set TCP maximum segment size failed.\n\
Setting the MSS may not be implemented on this OS.\n";
/* IPERF_MODIFIED End */

const char warn_mss_fail[] = "\
WARNING: attempt to set TCP maxmimum segment size to %d failed.\n\
Setting the MSS may not be implemented on this OS.\n";

const char warn_mss_notset[] =
"WARNING: attempt to set TCP maximum segment size to %d, but got %d\n";

void setsock_tcp_mss( int inSock, int inMSS ) {
#ifdef TCP_MAXSEG
    int rc;
    int newMSS;
    Socklen_t len;

    assert( inSock != INVALID_SOCKET );

    if ( inMSS > 0 ) {
        /* set */
        newMSS = inMSS;
        len = sizeof( newMSS );
        /* IPERF_MODIFIED Start */
        rc = iperf_setsockopt( inSock, IPPROTO_TCP, TCP_MAXSEG, (char*) &newMSS,  len );
        /* IPERF_MODIFIED End */
        if ( rc == SOCKET_ERROR ) {
            fprintf( stderr, warn_mss_fail, newMSS );
            return;
        }

        /* verify results */
        /* IPERF_MODIFIED Start */
        rc = iperf_getsockopt( inSock, IPPROTO_TCP, TCP_MAXSEG, (char*) &newMSS, &len );
        /* IPERF_MODIFIED End */
        WARN_errno( rc == SOCKET_ERROR, "getsockopt TCP_MAXSEG" );
        if ( newMSS != inMSS ) {
            fprintf( stderr, warn_mss_notset, inMSS, newMSS );
        }
    }
/* IPERF_MODIFIED Start */
#else
    IPERF_DEBUGF( SOCKET_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_LEVEL_WARNING, ( "%s", warn_mss_no_attempt ) );
/* IPERF_MODIFIED End */
#endif /* TCP_MAXSEG */
} /* end setsock_tcp_mss */

/* -------------------------------------------------------------------
 * returns the TCP maximum segment size
 * ------------------------------------------------------------------- */

int getsock_tcp_mss( int inSock ) {
    int theMSS = 0;

#ifdef TCP_MAXSEG
    int rc;
    Socklen_t len;
    assert( inSock >= 0 );

    /* query for MSS */
    len = sizeof( theMSS );
    rc = getsockopt( inSock, IPPROTO_TCP, TCP_MAXSEG, (char*) &theMSS, &len );
    WARN_errno( rc == SOCKET_ERROR, "getsockopt TCP_MAXSEG" );
/* IPERF_MODIFIED Start */
#else
    IPERF_DEBUGF( SOCKET_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_LEVEL_WARNING, ( "Unable get set socket TCP MSS - TCP_MAXSEG is undefined.\n" ) );
/* IPERF_MODIFIED End */
#endif /* TCP_MAXSEG */
    return theMSS;
} /* end getsock_tcp_mss */

/* -------------------------------------------------------------------
 * Attempts to reads n bytes from a socket.
 * Returns number actually read, or -1 on error.
 * If number read < inLen then we reached EOF.
 *
 * from Stevens, 1998, section 3.9
 * ------------------------------------------------------------------- */

ssize_t readn( int inSock, void *outBuf, size_t inLen ) {
    size_t  nleft;
    ssize_t nread;
    char *ptr;

    assert( inSock >= 0 );
    assert( outBuf != NULL );
    assert( inLen > 0 );

    ptr   = (char*) outBuf;
    nleft = inLen;

    while ( nleft > 0 ) {
        /* IPERF_MODIFIED Start */
        nread = iperf_read( inSock, ptr, nleft );
        /* IPERF_MODIFIED End */
        if ( nread < 0 ) {
            if ( errno == EINTR )
                nread = 0;  /* interupted, call read again */
            else
                return -1;  /* error */
        } else if ( nread == 0 )
            break;        /* EOF */

        nleft -= nread;
        ptr   += nread;
    }

    return(inLen - nleft);
} /* end readn */

/* -------------------------------------------------------------------
 * Similar to read but supports recv flags
 * Returns number actually read, or -1 on error.
 * If number read < inLen then we reached EOF.
 * from Stevens, 1998, section 3.9
 * ------------------------------------------------------------------- */
int recvn( int inSock, char *outBuf, int inLen, int flags ) {
    int  nleft;
    int nread;
    char *ptr;

    assert( inSock >= 0 );
    assert( outBuf != NULL );
    assert( inLen > 0 );

    ptr   = outBuf;
    nleft = inLen;

    while ( nleft > 0 ) {
        /* IPERF_MODIFIED Start */
        nread = iperf_recv( inSock, ptr, nleft, flags );
        /* IPERF_MODIFIED End */
        if ( nread < 0 ) {
            if ( errno == EAGAIN ) {
                nread = 0;  /* Socket read timeout */
		break;
            } else {
		WARN_errno( 1, "recvn" );
                return -1;  /* error */
	    }
	} else if ( nread == 0 ) {
	    WARN_errno( 1, "recvn abort" );
            break;        /* EOF */
	}
        nleft -= nread;
        ptr   += nread;
    }
    return(inLen - nleft);
} /* end readn */

/* -------------------------------------------------------------------
 * Attempts to write  n bytes to a socket.
 * returns number actually written, or -1 on error.
 * number written is always inLen if there is not an error.
 *
 * from Stevens, 1998, section 3.9
 * ------------------------------------------------------------------- */

ssize_t writen( int inSock, const void *inBuf, size_t inLen ) {
    size_t  nleft;
    ssize_t nwritten;
    const char *ptr;

    assert( inSock >= 0 );
    assert( inBuf != NULL );
    assert( inLen > 0 );

    ptr   = (char*) inBuf;
    nleft = inLen;

    while ( nleft > 0 ) {
        /* IPERF_MODIFIED Start */
        nwritten = iperf_write( inSock, ptr, nleft );
        /* IPERF_MODIFIED End */
        if ( nwritten <= 0 ) {
            if ( errno == EINTR )
                nwritten = 0; /* interupted, call write again */
            else
                return -1;    /* error */
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }

    return inLen;
} /* end writen */


/*
 * Set a socket to blocking or non-blocking
*
 * Returns true on success, or false if there was an error
*/
#define FALSE 0
#define TRUE 1
bool setsock_blocking (int fd, bool blocking) {
   if (fd < 0) return FALSE;

#ifdef WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? TRUE : FALSE;
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return FALSE;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? TRUE : FALSE;
#endif
}
#ifdef __cplusplus
} /* end extern "C" */
#endif
