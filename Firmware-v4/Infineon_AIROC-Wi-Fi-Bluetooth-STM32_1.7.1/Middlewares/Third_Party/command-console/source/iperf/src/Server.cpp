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
 * Server.cpp
 * by Mark Gates <mgates@nlanr.net>
 *     Ajay Tirumala (tirumala@ncsa.uiuc.edu>.
 * -------------------------------------------------------------------
 * A server thread is initiated for each connection accept() returns.
 * Handles sending and receiving data, and then closes socket.
 * Changes to this version : The server can be run as a daemon
 * ------------------------------------------------------------------- */

#define HEADERS()

#include "headers.h"
#include "Server.hpp"
/* IPERF_MODIFIED Start */
/* As build system looks for all the header files with the name and adding include paths. It includes mutex.h present in some other component
 * Hence files are renamed by appending iperf.
 */
#include "iperf_list.h"
/* IPERF_MODIFIED End */
#include "Extractor.h"
#include "Reporter.h"
/* IPERF_MODIFIED Start */
/* As build system looks for all the header files with the name and adding include paths. It includes mutex.h present in some other component
 * Hence files are renamed by appending iperf.
 */
#include "iperf_locale.h"
/* IPERF_MODIFIED End */
#include "delay.h"
#include "PerfSocket.hpp"
#include "SocketAddr.h"
#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)
#include "checksums.h"
#endif

/* -------------------------------------------------------------------
 * Stores connected socket and socket info.
 * ------------------------------------------------------------------- */

Server::Server( thread_Settings *inSettings ) {
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Creating a new Server.\n" ) );
    reportstruct = NULL;
    /* IPERF_MODIFIED End */
    mSettings = inSettings;
    mBuf = NULL;

#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)
    if (isL2LengthCheck(mSettings)) {
	// For L2 UDP make sure we can receive a full ethernet packet plus a bit more
	if (mSettings->mBufLen < (2 * ETHER_MAX_LEN)) {
	    mSettings->mBufLen = (2 * ETHER_MAX_LEN);
	}
    }
#endif
    // initialize buffer, length checking done by the Listener
    /* IPERF_MODIFIED Start */
    mBuf = (char*) malloc( mSettings->mBufLen );
    /* IPERF_MODIFIED End */
    FAIL_errno( mBuf == NULL, "No memory for buffer\n", mSettings );
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( MEMALLOC_DEBUG | IPERF_DBG_TRACE, IPERF_MEMALLOC_MSG( mBuf, mSettings->mBufLen ) );
    /* IPERF_MODIFIED End */
    SockAddr_Ifrname(mSettings);
}

/* -------------------------------------------------------------------
 * Destructor close socket.
 * ------------------------------------------------------------------- */

Server::~Server() {
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Destroying Server.\n" ) );
    /* IPERF_MODIFIED End */

    if ( mSettings->mSock != INVALID_SOCKET ) {
        /* IPERF_MODIFIED Start */
        IPERF_DEBUGF( SERVER_DEBUG | SOCKET_DEBUG | IPERF_DBG_TRACE, ( "Closing Server socket %d.\n", mSettings->mSock ) );
        int rc = iperf_close( mSettings->mSock );
        /* IPERF_MODIFIED End */
        WARN_errno( rc == SOCKET_ERROR, "server close" );
        mSettings->mSock = INVALID_SOCKET;
    }

#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)
    if ( mSettings->mSockDrop != INVALID_SOCKET ) {
	int rc = close( mSettings->mSockDrop );
        WARN_errno( rc == SOCKET_ERROR, "server close drop" );
        mSettings->mSockDrop = INVALID_SOCKET;
    }
#endif
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( MEMFREE_DEBUG | IPERF_DBG_TRACE, IPERF_MEMFREE_MSG( mBuf ) );
    FREE_PTR( mBuf );
    /* IPERF_MODIFIED End */
}

bool Server::InProgress (void) {
#ifdef HAVE_SETITIMER
    if (sInterupted)
	return false;
#else
    if (
/* IPERF_MODIFIED Start */
#ifndef NO_INTERRUPTS
     sInterupted ||
#endif
/* IPERF_MODIFIED End */
	(isServerModeTime(mSettings) &&  mEndTime.before(reportstruct->packetTime)))
	return false;
#endif
    return true;
}

/* -------------------------------------------------------------------
 * Receive TCP data from the (connected) socket.
 * Sends termination flag several times at the end.
 * Does not close the socket.
 * ------------------------------------------------------------------- */
void Server::RunTCP( void ) {
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Running the Server.\n" ) );
    /* IPERF_MODIFIED End */
    long currLen;
    intmax_t totLen = 0;
    bool err  = 0;

    Timestamp time1, time2;
    double tokens=0.000004;

    InitTrafficLoop();

    while (InProgress() && !err) {
	reportstruct->emptyreport=0;
	// perform read
	if (isBWSet(mSettings)) {
	    time2.setnow();
	    tokens += time2.subSec(time1) * (mSettings->mUDPRate / 8.0);
	    time1 = time2;
	}
	if (tokens >= 0.0) {
        /* IPERF_MODIFIED Start */
        currLen = iperf_recv( mSettings->mSock, mBuf, mSettings->mBufLen, 0 );
        /* IPERF_MODIFIED End */
	    now.setnow();
	    reportstruct->packetTime.tv_sec = now.getSecs();
	    reportstruct->packetTime.tv_usec = now.getUsecs();
	    if (currLen <= 0) {
		reportstruct->emptyreport=1;
		// End loop on 0 read or socket error
		// except for socket read timeout
		if (currLen == 0 ||
#ifdef WIN32
		    (WSAGetLastError() != WSAEWOULDBLOCK)
#else
		    /* IPERF_MODIFIED Start */
		    (errno != EWOULDBLOCK)
		    /* IPERF_MODIFIED End */
#endif // WIN32
		    ) {
		    err = 1;
		}
		currLen = 0;
	    }
        /* IPERF_MODIFIED Start */
        IPERF_DEBUGF_COUNTER( SERVER_DEBUG | SOCKET_DEBUG | IPERF_DBG_TRACE, ( "Server read TCP packet from socket %d, Packet length=%lu\n", mSettings->mSock, (long unsigned) currLen ) );
        /* IPERF_MODIFIED End */
	    totLen += currLen;
	    if (isBWSet(mSettings))
		tokens -= currLen;
	    reportstruct->packetLen = currLen;
	    ReportPacket( mSettings->reporthdr, reportstruct );
	} else {
	    // Use a 4 usec delay to fill tokens
	    delay_loop(4);
	}
    }

    // stop timing
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | SOCKET_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Server has finished receiving packets.\n" ) );
    /* IPERF_MODIFIED End */
    now.setnow();
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | TIME_DEBUG | IPERF_DBG_TRACE, ( "Server has stopped timing. Stop time is %ld sec %ld us.\n", (long)reportstruct->packetTime.tv_sec, (long)reportstruct->packetTime.tv_usec ) );
    /* IPERF_MODIFIED End */
    reportstruct->packetTime.tv_sec = now.getSecs();
    reportstruct->packetTime.tv_usec = now.getUsecs();

    if(0.0 == mSettings->mInterval) {
	reportstruct->packetLen = totLen;
    }
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | IPERF_DBG_TRACE, ( "Server is reporting packet %d.\n", reportstruct->packetID ) );
    /* IPERF_MODIFIED End */
    ReportPacket( mSettings->reporthdr, reportstruct );
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | IPERF_DBG_TRACE, ( "Server is closing report. [reporterindex=%d] [agentindex=%d].\n", mSettings->reporthdr->reporterindex, mSettings->reporthdr->agentindex ) );
    /* IPERF_MODIFIED End */
    CloseReport( mSettings->reporthdr, reportstruct );

    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | MUTEX_DEBUG | IPERF_DBG_TRACE, ( "Server is locking clients mutex.\n" ) );
    /* IPERF_MODIFIED End */
    Mutex_Lock( &clients_mutex );
    Iperf_delete( &(mSettings->peer), &clients );
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | MUTEX_DEBUG | IPERF_DBG_TRACE, ( "Server is unlocking clients mutex.\n" ) );
    /* IPERF_MODIFIED End */
    Mutex_Unlock( &clients_mutex );

    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( MEMFREE_DEBUG | IPERF_DBG_TRACE, IPERF_MEMFREE_MSG( reportstruct ) );
    FREE_PTR( reportstruct );
    /* IPERF_MODIFIED End */
    EndReport( mSettings->reporthdr );
}

void Server::InitTimeStamping (void) {
#if HAVE_DECL_SO_TIMESTAMP
    iov[0].iov_base=mBuf;
    iov[0].iov_len=mSettings->mBufLen;

    message.msg_iov=iov;
    message.msg_iovlen=1;
    message.msg_name=&srcaddr;
    message.msg_namelen=sizeof(srcaddr);

    message.msg_control = (char *) ctrl;
    message.msg_controllen = sizeof(ctrl);

    int timestampOn = 1;
    /* IPERF_MODIFIED Start */
    if (iperf_setsockopt(mSettings->mSock, SOL_SOCKET, SO_TIMESTAMP, (int *) &timestampOn, sizeof(timestampOn)) < 0) {
    /* IPERF_MODIFIED End */
	WARN_errno( mSettings->mSock == SO_TIMESTAMP, "socket" );
    }
#endif
}

void Server::InitTrafficLoop (void) {
    InitReport(mSettings);
    PostFirstReport(mSettings);
    /* IPERF_MODIFIED Start */
    reportstruct = (ReportStruct*) malloc( sizeof( ReportStruct ) );
    memset(reportstruct, 0, sizeof(ReportStruct));
    /* IPERF_MODIFIED End */
    FAIL(reportstruct == NULL, "Out of memory! Closing server thread\n", mSettings);
    reportstruct->packetID = 0;
    reportstruct->l2len = 0;
    reportstruct->l2errors = 0x0;
    if (mSettings->mBufLen < (int) sizeof( UDP_datagram ) ) {
	mSettings->mBufLen = sizeof( UDP_datagram );
	fprintf( stderr, warn_buffer_too_small, mSettings->mBufLen );
    }

    InitTimeStamping();

    int sorcvtimer = 0;
    // sorcvtimer units microseconds convert to that
    // minterval double, units seconds
    // mAmount integer, units 10 milliseconds
    // divide by two so timeout is 1/2 the interval
    if (mSettings->mInterval) {
	sorcvtimer = (int) (mSettings->mInterval * 1e6) / 2;
    } else if (isServerModeTime(mSettings)) {
	sorcvtimer = (mSettings->mAmount * 1000) / 2;
    }
    if (sorcvtimer > 0) {
#ifdef WIN32
	// Windows SO_RCVTIMEO uses ms
	DWORD timeout = (double) sorcvtimer / 1e3;
#else
	struct timeval timeout;
	timeout.tv_sec = sorcvtimer / 1000000;
	timeout.tv_usec = sorcvtimer % 1000000;
#endif
        /* IPERF_MODIFIED Start */
        if (iperf_setsockopt( mSettings->mSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 ) {
        /* IPERF_MODIFIED End */
	    WARN_errno( mSettings->mSock == SO_RCVTIMEO, "socket" );
	}
    }
    if (isServerModeTime(mSettings)) {
#ifdef HAVE_SETITIMER
        int err;
        struct itimerval it;
	memset (&it, 0, sizeof (it));
	it.it_value.tv_sec = (int) (mSettings->mAmount / 100.0);
	it.it_value.tv_usec = (int) (10000 * (mSettings->mAmount -
					      it.it_value.tv_sec * 100.0));
	err = setitimer( ITIMER_REAL, &it, NULL );
	FAIL_errno( err != 0, "setitimer", mSettings );
#else
        mEndTime.setnow();
        mEndTime.add( mSettings->mAmount / 100.0 );
#endif
    }

    if (isTripTime(mSettings)) {
	/* IPERF_MODIFIED Start */
//	int n;
	const int len=3;
	/* IPERF_MODIFIED End */
	uint32_t buf[len];
	if (len && (recvn(mSettings->mSock, (char *)&buf[0], sizeof(buf), MSG_PEEK) != (int) sizeof(buf))) {
	    fprintf(stdout,"Warn: socket trip time read error\n");
	} else {
	    mSettings->reporthdr->report.clientStartTime.tv_sec = ntohl(buf[1]);
	    mSettings->reporthdr->report.clientStartTime.tv_usec = ntohl(buf[2]);
	}
    }
}

int Server::ReadWithRxTimestamp (int *readerr) {
    long currLen;
    int tsdone = 0;

#if HAVE_DECL_SO_TIMESTAMP
    cmsg = (struct cmsghdr *) &ctrl;
    currLen = recvmsg( mSettings->mSock, &message, mSettings->recvflags );
    if (currLen > 0) {
	if (cmsg->cmsg_level == SOL_SOCKET &&
	    cmsg->cmsg_type  == SCM_TIMESTAMP &&
	    cmsg->cmsg_len   == CMSG_LEN(sizeof(struct timeval))) {
	    memcpy(&(reportstruct->packetTime), CMSG_DATA(cmsg), sizeof(struct timeval));
	    tsdone = 1;
	}
    }
#else
    /* IPERF_MODIFIED Start */
    currLen = iperf_recv( mSettings->mSock, mBuf, mSettings->mBufLen, mSettings->recvflags);
    /* IPERF_MODIFIED End */
#endif
    if (currLen <=0) {
	// Socket read timeout or read error
	reportstruct->emptyreport=1;
	// End loop on 0 read or socket error

    /* IPERF_MODIFIED Start */
    //TODO: Updating errno to timeout should be handled in secure socket port layer
    // For socket timeout error, skip returning read error.
    // Socket timeout is expected when server is continuously waiting to receive but due to UDP packet loss the timeout error occurs which should be ignored.
    if (currLen == -3)
    {
        currLen= 0;
         goto exit;
    }
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
    FAIL_errno(currLen == -1, "UDP socket closed\n", mSettings);
#endif
    /* IPERF_MODIFIED End */

	// except for socket read timeout
	if (currLen == 0 ||
#ifdef WIN32
	    (WSAGetLastError() != WSAEWOULDBLOCK)
#else
	    /* IPERF_MODIFIED Start */
	    (errno != EWOULDBLOCK)
	    /* IPERF_MODIFIED End */
#endif
	    ) {
	    WARN_errno( currLen, "recvmsg");
	    *readerr = 1;
	}
	currLen= 0;
    }

exit:
    if (!tsdone) {
	now.setnow();
	reportstruct->packetTime.tv_sec = now.getSecs();
	reportstruct->packetTime.tv_usec = now.getUsecs();
    }
    return currLen;
}

// Returns true if the client has indicated this is the final packet
bool Server::ReadPacketID (void) {
    bool terminate = false;
    struct UDP_datagram* mBuf_UDP  = (struct UDP_datagram*) (mBuf + mSettings->l4payloadoffset);

    // terminate when datagram begins with negative index
    // the datagram ID should be correct, just negated

    if (isSeqNo64b(mSettings)) {
      // New client - Signed PacketID packed into unsigned id2,id
      reportstruct->packetID = ((uint32_t)ntohl(mBuf_UDP->id)) | ((uintmax_t)(ntohl(mBuf_UDP->id2)) << 32);

#ifdef SHOW_PACKETID
      printf("id 0x%x, 0x%x -> %" PRIdMAX " (0x%" PRIxMAX ")\n",
	     ntohl(mBuf_UDP->id), ntohl(mBuf_UDP->id2), reportstruct->packetID, reportstruct->packetID);
#endif
    } else {
      // Old client - Signed PacketID in Signed id
      reportstruct->packetID = (int32_t)ntohl(mBuf_UDP->id);
#ifdef SHOW_PACKETID
      printf("id 0x%x -> %" PRIdMAX " (0x%" PRIxMAX ")\n",
	     ntohl(mBuf_UDP->id), reportstruct->packetID, reportstruct->packetID);
#endif
    }
    if (reportstruct->packetID < 0) {
      reportstruct->packetID = - reportstruct->packetID;
      terminate = true;
    }
    // read the sent timestamp from the rx packet
    reportstruct->sentTime.tv_sec = ntohl( mBuf_UDP->tv_sec  );
    reportstruct->sentTime.tv_usec = ntohl( mBuf_UDP->tv_usec );

    return terminate;
}

void Server::L2_processing (void) {
#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)
    eth_hdr = (struct ether_header *) mBuf;
    ip_hdr = (struct iphdr *) (mBuf + sizeof(struct ether_header));
    // L4 offest is set by the listener and depends upon IPv4 or IPv6
    udp_hdr = (struct udphdr *) (mBuf + mSettings->l4offset);
    // Read the packet to get the UDP length
    int udplen = ntohs(udp_hdr->len);
    //
    // in the event of an L2 error, double check the packet before passing it to the reporter,
    // i.e. no reason to run iperf accounting on a packet that has no reasonable L3 or L4 headers
    //
    reportstruct->packetLen = udplen - sizeof(struct udphdr);
    reportstruct->expected_l2len = reportstruct->packetLen + mSettings->l4offset + sizeof(struct udphdr);
    if (reportstruct->l2len != reportstruct->expected_l2len) {
	reportstruct->l2errors |= L2LENERR;
	if (L2_quintuple_filter() != 0) {
	    reportstruct->l2errors |= L2UNKNOWN;
	    reportstruct->l2errors |= L2CSUMERR;
	    reportstruct->emptyreport = 1;
	}
    }
    if (!(reportstruct->l2errors & L2UNKNOWN)) {
	// perform UDP checksum test, returns zero on success
	int rc;
	rc = udpchecksum((void *)ip_hdr, (void *)udp_hdr, udplen, (isIPV6(mSettings) ? 1 : 0));
	if (rc) {
	    reportstruct->l2errors |= L2CSUMERR;
	    if ((!(reportstruct->l2errors & L2LENERR)) && (L2_quintuple_filter() != 0)) {
		reportstruct->emptyreport = 1;
		reportstruct->l2errors |= L2UNKNOWN;
	    }
	}
    }
#endif // HAVE_AF_PACKET
}

// Run the L2 packet through a quintuple check, i.e. proto/ip src/ip dst/src port/src dst
// and return zero is there is a match, otherwize return nonzero
int Server::L2_quintuple_filter(void) {
#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)

#define IPV4SRCOFFSET 12  // the ipv4 source address offset from the l3 pdu
#define IPV6SRCOFFSET 8 // the ipv6 source address offset

    // Get the expected values from the sockaddr structures
    // Note: it's expected the initiating socket has aready "connected"
    // and the sockaddr structs have been populated
    // 2nd Note:  sockaddr structs are in network byte order
    struct sockaddr *p = (sockaddr *)&mSettings->peer;
    struct sockaddr *l = (sockaddr *)&mSettings->local;
    // make sure sa_family is coherent for both src and dst
    if (!(((l->sa_family == AF_INET) && (p->sa_family == AF_INET)) || ((l->sa_family == AF_INET6) && (p->sa_family == AF_INET6)))) {
	return -1;
    }

    // check the L2 ethertype
    struct ether_header *l2hdr = (struct ether_header *)mBuf;

    if (!isIPV6(mSettings)) {
	if (ntohs(l2hdr->ether_type) != ETHERTYPE_IP)
	    return -1;
    } else {
	if (ntohs(l2hdr->ether_type) != ETHERTYPE_IPV6)
	    return -1;
    }
    // check the ip src/dst
    const uint32_t *data;
    udp_hdr = (struct udphdr *) (mBuf + mSettings->l4offset);

    // Check plain old v4 using v4 addr structs
    if (l->sa_family == AF_INET) {
	data = (const uint32_t *) (mBuf + sizeof(struct ether_header) + IPV4SRCOFFSET);
	if (((struct sockaddr_in *)(p))->sin_addr.s_addr != *data++)
	    return -1;
	if (((struct sockaddr_in *)(l))->sin_addr.s_addr != *data)
	    return -1;
	if (udp_hdr->source != ((struct sockaddr_in *)(p))->sin_port)
	    return -1;
	if (udp_hdr->dest != ((struct sockaddr_in *)(l))->sin_port)
	    return -1;
    } else {
	// Using the v6 addr structures
#  ifdef HAVE_IPV6
	struct in6_addr *v6peer = SockAddr_get_in6_addr(&mSettings->peer);
	struct in6_addr *v6local = SockAddr_get_in6_addr(&mSettings->local);
	if (isIPV6(mSettings)) {
	    int i;
	    data = (const uint32_t *) (mBuf + sizeof(struct ether_header) + IPV6SRCOFFSET);
	    // check for v6 src/dst address match
	    for (i = 0; i < 4; i++) {
		if (v6peer->s6_addr32[i] != *data++)
		    return -1;
	    }
	    for (i = 0; i < 4; i++) {
		if (v6local->s6_addr32[i] != *data++)
		    return -1;
	    }
	} else { // v4 addr in v6 family struct
	    data = (const uint32_t *) (mBuf + sizeof(struct ether_header) + IPV4SRCOFFSET);
	    if (v6peer->s6_addr32[3] != *data++)
		return -1;
	    if (v6peer->s6_addr32[3] != *data)
		return -1;
	}
	// check udp ports
	if (udp_hdr->source != ((struct sockaddr_in6 *)(p))->sin6_port)
	    return -1;
	if (udp_hdr->dest != ((struct sockaddr_in6 *)(l))->sin6_port)
	    return -1;
#  endif // HAVE_IPV6
    }
#endif // HAVE_AF_PACKET
    // made it through all the checks
    return 0;
}

void Server::Isoch_processing (int rxlen) {
#ifdef HAVE_ISOCHRONOUS
    // Ignore runt sized isoch packets
    if (rxlen < (int) (sizeof(UDP_datagram) +  sizeof(client_hdr_v1) + sizeof(client_hdr_udp_isoch_tests))) {
	reportstruct->burstsize = 0;
	reportstruct->remaining = 0;
	reportstruct->frameID = 0;
    } else {
	struct client_hdr_udp_isoch_tests *testhdr = (client_hdr_udp_isoch_tests *)(mBuf + sizeof(client_hdr_v1) + sizeof(UDP_datagram));
	struct UDP_isoch_payload* mBuf_isoch = &(testhdr->isoch);
	reportstruct->isochStartTime.tv_sec = ntohl(mBuf_isoch->start_tv_sec);
	reportstruct->isochStartTime.tv_usec = ntohl(mBuf_isoch->start_tv_usec);
	reportstruct->frameID = ntohl(mBuf_isoch->frameid);
	reportstruct->prevframeID = ntohl(mBuf_isoch->prevframeid);
	reportstruct->burstsize = ntohl(mBuf_isoch->burstsize);
	reportstruct->burstperiod = ntohl(mBuf_isoch->burstperiod);
	reportstruct->remaining = ntohl(mBuf_isoch->remaining);
    }
#endif
}

/* -------------------------------------------------------------------
 * Receive UDP data from the (connected) socket.
 * Sends termination flag several times at the end.
 * Does not close the socket.
 * ------------------------------------------------------------------- */
void Server::RunUDP( void ) {
    int rxlen;
    int readerr = 0;
    bool lastpacket = 0;

    InitTrafficLoop();

    // Exit loop on three conditions
    // 1) Fatal read error
    // 2) Last packet of traffic flow sent by client
    // 3) -t timer expires
    while (InProgress() && !readerr && !lastpacket) {
	// The emptyreport flag can be set
	// by any of the packet processing routines
	// If it's set the iperf reporter won't do
	// bandwidth accounting, basically it's indicating
	// that the reportstruct itself couldn't be
	// completely filled out.
	reportstruct->emptyreport=0;
	// read the next packet with timestamp
	// will also set empty report or not
	rxlen=ReadWithRxTimestamp(&readerr);
	if (!readerr && (rxlen > 0)) {
	    if (isL2LengthCheck(mSettings)) {
		reportstruct->l2len = rxlen;
		// L2 processing will set the reportstruct packet length with the length found in the udp header
		// and also set the expected length in the report struct.  The reporter thread
		// will do the compare and account and print l2 errors
		reportstruct->l2errors = 0x0;
		L2_processing();
	    } else {
		// Normal UDP rx, set the length to the socket received length
		reportstruct->packetLen = rxlen;
	    }
	    if (!(reportstruct->l2errors & L2UNKNOWN)) {
		// ReadPacketID returns true if this is the last UDP packet sent by the client
		// aslo sets the packet rx time in the reportstruct
		lastpacket = ReadPacketID();
		if (isIsochronous(mSettings)) {
		    Isoch_processing(rxlen);
		}
	    }
	}

	ReportPacket(mSettings->reporthdr, reportstruct);

    }

    CloseReport( mSettings->reporthdr, reportstruct );

    // send a acknowledgement back only if we're NOT receiving multicast
    if (!isMulticast( mSettings ) ) {
	// send back an acknowledgement of the terminating datagram
	write_UDP_AckFIN( );
    }

    Mutex_Lock( &clients_mutex );
    Iperf_delete( &(mSettings->peer), &clients );
    Mutex_Unlock( &clients_mutex );

    /* IPERF_MODIFIED Start */
    FREE_PTR( reportstruct );
    /* IPERF_MODIFIED End */
    EndReport( mSettings->reporthdr );
}
// end Recv

/* -------------------------------------------------------------------
 * Send an AckFIN (a datagram acknowledging a FIN) on the socket,
 * then select on the socket for some time. If additional datagrams
 * come in, probably our AckFIN was lost and they are re-transmitted
 * termination datagrams, so re-transmit our AckFIN.
 * ------------------------------------------------------------------- */

void Server::write_UDP_AckFIN( ) {

    int rc;

    fd_set readSet;
    FD_ZERO( &readSet );

    struct timeval timeout;

    int count = 0;
    while ( count < 10 ) {
        count++;

        UDP_datagram *UDP_Hdr;
        server_hdr *hdr;

        UDP_Hdr = (UDP_datagram*) mBuf;
        if (mSettings->mBufLen > (int) (sizeof(UDP_datagram) + sizeof(server_hdr))) {
	    int flags = (!isEnhanced(mSettings) ? HEADER_VERSION1 : (HEADER_VERSION1 | HEADER_EXTEND));
#ifdef HAVE_INT64_T
	    flags |=  HEADER_SEQNO64B;
#endif
            Transfer_Info *stats = GetReport( mSettings->reporthdr );
            hdr = (server_hdr*) (UDP_Hdr+1);
	    hdr->base.flags        = htonl((long) flags);
#ifdef HAVE_INT64_T
            hdr->base.total_len1   = htonl( (long) (stats->TotalLen >> 32) );
#else
            hdr->base.total_len1   = htonl(0x0);
#endif
            hdr->base.total_len2   = htonl( (long) (stats->TotalLen & 0xFFFFFFFF) );
            hdr->base.stop_sec     = htonl( (long) stats->endTime );
            hdr->base.stop_usec    = htonl( (long)((stats->endTime - (long)stats->endTime) * rMillion));
            hdr->base.error_cnt    = htonl( (long) (stats->cntError & 0xFFFFFFFF) );
            hdr->base.outorder_cnt = htonl( (long) (stats->cntOutofOrder  & 0xFFFFFFFF));
	    hdr->base.datagrams    = htonl( (long) (stats->cntDatagrams & 0xFFFFFFFF) );
	    if (flags & HEADER_SEQNO64B) {
	      hdr->extend2.error_cnt2    = htonl( (long) ( (long long)stats->cntError >> 32) );
	      hdr->extend2.outorder_cnt2 = htonl( (long) ( (long long)stats->cntOutofOrder >> 32)  );
	      hdr->extend2.datagrams2    = htonl( (long) ( (long long)stats->cntDatagrams >> 32) );
	    }

            hdr->base.jitter1      = htonl( (long) stats->jitter );
            hdr->base.jitter2      = htonl( (long) ((stats->jitter - (long)stats->jitter) * rMillion) );
	    if (flags & HEADER_EXTEND) {
		hdr->extend.minTransit1  = htonl( (long) stats->transit.totminTransit );
		hdr->extend.minTransit2  = htonl( (long) ((stats->transit.totminTransit - (long)stats->transit.totminTransit) * rMillion) );
		hdr->extend.maxTransit1  = htonl( (long) stats->transit.totmaxTransit );
		hdr->extend.maxTransit2  = htonl( (long) ((stats->transit.totmaxTransit - (long)stats->transit.totmaxTransit) * rMillion) );
		hdr->extend.sumTransit1  = htonl( (long) stats->transit.totsumTransit );
		hdr->extend.sumTransit2  = htonl( (long) ((stats->transit.totsumTransit - (long)stats->transit.totsumTransit) * rMillion) );
		hdr->extend.meanTransit1  = htonl( (long) stats->transit.totmeanTransit );
		hdr->extend.meanTransit2  = htonl( (long) ((stats->transit.totmeanTransit - (long)stats->transit.totmeanTransit) * rMillion) );
		hdr->extend.m2Transit1  = htonl( (long) stats->transit.totm2Transit );
		hdr->extend.m2Transit2  = htonl( (long) ((stats->transit.totm2Transit - (long)stats->transit.totm2Transit) * rMillion) );
		hdr->extend.vdTransit1  = htonl( (long) stats->transit.totvdTransit );
		hdr->extend.vdTransit2  = htonl( (long) ((stats->transit.totvdTransit - (long)stats->transit.totvdTransit) * rMillion) );
		hdr->extend.cntTransit   = htonl( stats->transit.totcntTransit );
		hdr->extend.IPGcnt = htonl( (long) (stats->cntDatagrams / (stats->endTime - stats->startTime)));
		hdr->extend.IPGsum = htonl(1);
	    }
        }

        // write data
#if defined(HAVE_LINUX_FILTER_H) && defined(HAVE_AF_PACKET)
	// If in l2mode, use the AF_INET socket to write this packet
	//
	write(((mSettings->mSockDrop > 0 ) ? mSettings->mSockDrop : mSettings->mSock), mBuf, mSettings->mBufLen);
#else
/* IPERF_MODIFIED Start */
        iperf_write(mSettings->mSock, mBuf, mSettings->mBufLen);
/* IPERF_MODIFIED End */
#endif
        // wait until the socket is readable, or our timeout expires
        FD_SET( mSettings->mSock, &readSet );
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        /* IPERF_MODIFIED Start */
        rc = iperf_select( mSettings->mSock+1, &readSet, NULL, NULL, &timeout );
        /* IPERF_MODIFIED End */
        FAIL_errno( rc == SOCKET_ERROR, "select", mSettings );

        if ( rc == 0 ) {
            // select timed out
            return;
        } else {
            // socket ready to read
            /* IPERF_MODIFIED Start */
            rc = iperf_read( mSettings->mSock, mBuf, mSettings->mBufLen );

            //WARN_errno( rc < 0, "read" );
            /* IPERF_MODIFIED End */
            if ( rc <= 0 ) {
                // Connection closed or errored
                // Stop using it.
                return;
            }
        }
    }

    fprintf( stderr, warn_ack_failed, mSettings->mSock, count );
}
// end write_UDP_AckFIN
