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
 * Launch.cpp
 * by Kevin Gibbs <kgibbs@nlanr.net>
 * -------------------------------------------------------------------
 * Functions to launch new server and client threads from C while
 * the server and client are in C++.
 * The launch function for reporters is in Reporter.c since it is
 * in C and does not need a special launching function.
 * ------------------------------------------------------------------- */

/* IPERF_MODIFIED Start */
/* As build system looks for all the header files with the name and adding include paths. It includes mutex.h present in some other component
 * Hence files are renamed by appending iperf.
 */
#include "iperf_thread.h"
/* IPERF_MODIFIED End */
#include "headers.h"
#include "Settings.hpp"
#include "Client.hpp"
#include "Listener.hpp"
#include "Server.hpp"
#include "PerfSocket.hpp"

#ifdef HAVE_SCHED_SETSCHEDULER
#include <sched.h>
#endif
#ifdef HAVE_MLOCKALL
#include <sys/mman.h>
#endif
static void set_scheduler(thread_Settings *thread) {
#ifdef HAVE_SCHED_SETSCHEDULER
    if ( isRealtime( thread ) ) {
	struct sched_param sp;
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	// SCHED_OTHER, SCHED_FIFO, SCHED_RR
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0)  {
	    perror("Client set scheduler");
#ifdef HAVE_MLOCKALL
	} else if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
	    // lock the threads memory
	    perror ("mlockall");
#endif // MLOCK
	}
    }
#endif // SCHED
}

/*
 * listener_spawn is responsible for creating a Listener class
 * and launching the listener. It is provided as a means for
 * the C thread subsystem to launch the listener C++ object.
 */
void listener_spawn( thread_Settings *thread ) {
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( LISTENER_DEBUG | THREAD_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Spawning a listener.\n" ) );
    /* IPERF_MODIFIED End */
    Listener *theListener = NULL;

    // start up a listener
    theListener = new Listener( thread );

    /* IPERF_MODIFIED Start */
    FAIL( theListener == NULL, ( "No memory for Listener class.\n" ), thread );
    IPERF_DEBUGF( MEMALLOC_DEBUG | IPERF_DBG_TRACE, IPERF_MEMALLOC_MSG( theListener, sizeof( Listener ) ) );
    /* IPERF_MODIFIED End */

    // Start listening
    theListener->Run();
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( MEMFREE_DEBUG | IPERF_DBG_TRACE, IPERF_MEMFREE_MSG( theListener ) );
    /* IPERF_MODIFIED End */
    DELETE_PTR( theListener );
}

/*
 * server_spawn is responsible for creating a Server class
 * and launching the server. It is provided as a means for
 * the C thread subsystem to launch the server C++ object.
 */
void server_spawn( thread_Settings *thread) {
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( SERVER_DEBUG | THREAD_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Spawning a server.\n" ) );
    /* IPERF_MODIFIED End */

    Server *theServer = NULL;

    // Start up the server
    theServer = new Server( thread );

    /* IPERF_MODIFIED Start */
    FAIL( theServer == NULL, ( "No memory for Server class.\n" ), thread );
    IPERF_DEBUGF( MEMALLOC_DEBUG | IPERF_DBG_TRACE, IPERF_MEMALLOC_MSG( theServer, sizeof( Server ) ) );
    /* IPERF_MODIFIED End */

    // set traffic thread to realtime if needed
    set_scheduler(thread);
    // Run the test
    if ( isUDP( thread ) ) {
	theServer->RunUDP();
    } else {
	theServer->RunTCP();
    }
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( MEMFREE_DEBUG | IPERF_DBG_TRACE, IPERF_MEMFREE_MSG( theServer ) );
    /* IPERF_MODIFIED End */
    DELETE_PTR( theServer);
}

/*
 * client_spawn is responsible for creating a Client class
 * and launching the client. It is provided as a means for
 * the C thread subsystem to launch the client C++ object.
 */
void client_spawn( thread_Settings *thread ) {
    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( CLIENT_DEBUG | THREAD_DEBUG | IPERF_DBG_TRACE | IPERF_DBG_STATE, ( "Spawning a client.\n" ) );
    /* IPERF_MODIFIED End */
    Client *theClient = NULL;

    //start up the client
    theClient = new Client( thread );

    /* IPERF_MODIFIED Start */
    FAIL( theClient == NULL, ( "No memory for Client class.\n" ), thread );
    IPERF_DEBUGF( MEMALLOC_DEBUG | IPERF_DBG_TRACE, IPERF_MEMALLOC_MSG( theClient, sizeof( Client ) ) );
    /* IPERF_MODIFIED End */
    // set traffic thread to realtime if needed
    set_scheduler(thread);

    // Let the server know about our settings
    theClient->InitiateServer();

    // Run the test
    theClient->Run();

    /* IPERF_MODIFIED Start */
    IPERF_DEBUGF( MEMALLOC_DEBUG | IPERF_DBG_TRACE, IPERF_MEMFREE_MSG( theClient ) );
    /* IPERF_MODIFIED End */

    DELETE_PTR( theClient );
}

/*
 * client_init handles multiple threaded connects. It creates
 * a listener object if either the dual test or tradeoff were
 * specified. It also creates settings structures for all the
 * threads and arranges them so they can be managed and started
 * via the one settings structure that was passed in.
 */
void client_init( thread_Settings *clients ) {
    thread_Settings *itr = NULL;
    thread_Settings *next = NULL;

    // Set the first thread to report Settings
    setReport( clients );
    itr = clients;

    // See if we need to start a listener as well
    Settings_GenerateListenerSettings( clients, &next );

    // Create a multiple report header to handle reporting the
    // sum of multiple client threads
    Mutex_Lock( &groupCond );
    groupID--;
    clients->multihdr = InitMulti( clients, groupID );
    Mutex_Unlock( &groupCond );

#ifdef HAVE_THREAD
    if ( next != NULL ) {
        // We have threads and we need to start a listener so
        // have it ran before the client is launched
        itr->runNow = next;
        itr = next;
    }
#endif
    // For each of the needed threads create a copy of the
    // provided settings, unsetting the report flag and add
    // to the list of threads to start
    for (int i = 1; i < clients->mThreads; i++) {
        Settings_Copy( clients, &next );
	if (isIncrDstIP(clients))
	    next->incrdstip = i;
        unsetReport( next );
        itr->runNow = next;
        itr = next;
    }
#ifndef HAVE_THREAD
    if ( next != NULL ) {
        // We don't have threads and we need to start a listener so
        // have it ran after the client is finished
        itr->runNext = next;
    }
#endif
}

