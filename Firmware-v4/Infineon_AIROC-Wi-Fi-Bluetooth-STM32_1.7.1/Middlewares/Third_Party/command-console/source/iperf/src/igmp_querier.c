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

/*
 * Copyright (c) 2018
 * Broadcom Corporation
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.  Neither the name of the Broadcom nor the names of
 * contributors may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USEn,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author Robert J. McMahon, Broadcom LTD
 * Date April 2016
 *
 * IGMP 2 querier that can be controlled remotely
 *
 * Author Robert J. McMahon (rmcmahon)
 * Last modified: 06/25/2010
 *
 * $Copyright Open Broadcom Corporation$
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* IPERF_MODIFIED Start */
#ifdef WIN32
/* IPERF_MODIFIED End */
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "headers.h"

#define IGMP_HEADER_SIZE 8

void sigint();
void siguser();
void sigalrm();
static int querier_sent_count = 0;
static int sendcount = 0;
static short daemonmode = 0;
static char mcast_queryaddr[20] = "224.0.0.1";
static int send_igmp_allhosts_querier (char *);


int main (int argc, char **argv) {
    char *tvalue = NULL;
    char *cvalue = NULL;
    int c;
    int queryinterval;
    int pid;

    opterr = 0;
    while ((c = getopt (argc, argv, "c:dg:t:")) != -1) {
	switch (c) {
	case 'c':
	    cvalue = optarg;
	    break;
	case 'g' :
	    strcpy(mcast_queryaddr, optarg);
	    break;
	case 't':
	    tvalue = optarg;
	    break;
	case 'd':
	    daemonmode = 1;
	    break;
	case '?':
	    if (optopt == 't')
		fprintf (stderr, "Option -%c requires an integer argument.\n", optopt);
	    else if (optopt == 'c')
		fprintf (stderr, "Option -%c requires an integer argument.\n", optopt);
	    else if (isprint (optopt))
		fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	    else
		fprintf (stderr,
			 "Unknown option character `\\x%x'.\n",
			 optopt);
	default:
	    fprintf(stderr,"Usage -c <count>, -d daemon, -g <group>, -t <period in seconds>\n");
	    exit(-1);
	}
    }
    if (tvalue != NULL) {
	queryinterval = atoi(tvalue);
    } else {
	queryinterval = 0;
    }
    if (cvalue != NULL) {
	sendcount = atoi(cvalue);
	if (tvalue == NULL)
	    queryinterval = 0;
    } else {
	sendcount = 1;
    }
    signal(SIGINT, sigint);
    signal(SIGUSR1, siguser);
    signal(SIGALRM, sigalrm);
    pid = (int) getpid();
    if (daemonmode) {
	if (tvalue != NULL) {
	    sendcount = 1;
	    printf("IGMP All Hosts Querier (pid=%d) started as a daemon with interval of %d seconds to %s\n", pid, queryinterval, mcast_queryaddr);
	} else {
	    sendcount = 0;
	    printf("IGMP All Hosts Querier (pid=%d) to %s started as a daemon only\n", pid, mcast_queryaddr);
	}
    } else {
	printf("IGMP All Hosts Querier (pid=%d) sending %d reports with interval of %d seconds\n", pid, sendcount, queryinterval);
    }
    fflush(stdout);
    while (daemonmode || sendcount) {
	if (sendcount-- > 0) {
	    send_igmp_allhosts_querier(mcast_queryaddr);
	}
	if (queryinterval) {
	    alarm(queryinterval);
	    pause();
	}
	if (daemonmode && sendcount <= 0)
	    pause();
    }
}

void sigint (void) {
    exit(0);
}

void siguser (void) {
    if (!send_igmp_allhosts_querier(mcast_queryaddr)) {
	exit (-1);
    }
}

void sigalrm (void) {
    if (daemonmode)
	sendcount++;
}

static int send_igmp_allhosts_querier (char *mcast_queryaddr) {
    int sid=0;
    int rc;
    char buf[IGMP_HEADER_SIZE];
    unsigned int ttl=1;
    char type = 17;
    char maxresptime = 1;
    //	unsigned int groupaddr = 0;
    char checksum_upper = 0xee;
    char checksum_lower = 0xfe;
    struct sockaddr_in msock;
    const time_t timer = time(NULL);

    bzero (&buf, sizeof(buf));
    /*
     * Note: with byte writes shouldn't need to worry about network/host
     * byte ordering, though should double check on non intel system
     */
    buf[0] = type;
    buf[1] = maxresptime;
    buf[2] = checksum_upper;
    buf[3] = checksum_lower;

    bzero (&msock, sizeof(msock));
    msock.sin_family = AF_INET;

    inet_pton(AF_INET, mcast_queryaddr, &msock.sin_addr);
    setuid(geteuid());
    sid = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
    if (sid != -1) {
	rc = setsockopt(sid, IPPROTO_IP, IP_TTL, (char *) &ttl, sizeof(ttl));
	if (rc != -1) {
	    rc = sendto(sid, &buf, IGMP_HEADER_SIZE, 0, (const struct sockaddr *) &msock, sizeof(msock));
	    if (rc == -1) {
		fprintf(stderr, "IGMP Query sendto error = %s\n", strerror(errno));
	    }
	} else {
	    fprintf(stderr, "IGMP Query setsockopt error = %s\n", strerror(errno));
	}
    } else {
	fprintf(stderr, "IGMP Query, socket error = %s\n", strerror(errno));
    }
    if (sid > 0) {
	close(sid);
	rc=1;
    } else {
	rc = 0;
    }
    setuid(getuid());
    if (rc) {
	printf("Sent IGMP all hosts querier to %s (count = %d) at %s",mcast_queryaddr, ++querier_sent_count, ctime(&timer));
	fflush(stdout);
    }
    return rc;
}
/* IPERF_MODIFIED Start */
#endif
/* IPERF_MODIFIED End */
