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
 * Condition.h
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * An abstract class for waiting on a condition variable. If
 * threads are not available, this does nothing.
 * ------------------------------------------------------------------- */

#ifndef CONDITION_H
#define CONDITION_H

#include "headers.h"
/* IPERF_MODIFIED Start */
/* As build system looks for all the header files with the name and adding include paths. It includes mutex.h present in some other component
 * Hence files are renamed by appending iperf.
 */
#include "iperf_mutex.h"
#include "iperf_util.h"

#if   defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
typedef struct Condition {
    pthread_cond_t mCondition;
    pthread_mutex_t mMutex;
} Condition;
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
typedef struct Condition {
    HANDLE mCondition;
    HANDLE mMutex;
} Condition;
/* IPERF_MODIFIED Start */
#elif (defined(HAVE_FREERTOS_THREAD) || defined(HAVE_THREADX_THREAD)) && !defined(NO_THREADS)
typedef struct Lock {
    host_semaphore_type_t sm;
} Lock;

void Lock_Initialize(Lock *m);
void Lock_Destroy(Lock *m);
void Lock_Acquire(Lock *m);
void Lock_Release(Lock *m);

typedef struct Condition {
    host_semaphore_type_t s;
    host_semaphore_type_t x;
    Lock *m;
    int waiters;
    host_semaphore_type_t h;
} Condition;

void Condition_Initialize(Condition *c);
void Condition_Destroy(Condition *c);
void Condition_Wait(Condition *c);
void Condition_Signal(Condition *c);
void Condition_Broadcast(Condition *c);
void Condition_Lock(Condition *c);
void Condition_Unlock(Condition *c);
/* IPERF_MODIFIED End */
#else
typedef struct Condition {
    int mCondition;
    int mMutex;
} Condition;
#endif

#define Condition_Lock( Cond ) Mutex_Lock( &Cond.mMutex )

#define Condition_Unlock( Cond ) Mutex_Unlock( &Cond.mMutex )

    // initialize condition
/* IPERF_MODIFIED Start */
#if defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Initialize( Cond ) do {             \
        Mutex_Initialize( &(Cond)->mMutex );              \
        pthread_cond_init( &(Cond)->mCondition, NULL );   \
    } while ( 0 )
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    // set all conditions to be broadcast
    // unfortunately in Win32 you have to know at creation
    // whether the signal is broadcast or not.
    #define Condition_Initialize( Cond ) do {                         \
        Mutex_Initialize( &(Cond)->mMutex );                          \
        (Cond)->mCondition = CreateEvent( NULL, 1, 0, NULL );  \
    } while ( 0 )
#else
    #define Condition_Initialize( Cond )
#endif

    // destroy condition
/* IPERF_MODIFIED Start */
#if   defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Destroy( Cond ) do {            \
        pthread_cond_destroy( &(Cond)->mCondition );  \
        Mutex_Destroy( &(Cond)->mMutex );             \
    } while ( 0 )
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Destroy( Cond ) do {            \
        CloseHandle( (Cond)->mCondition );            \
        Mutex_Destroy( &(Cond)->mMutex );             \
    } while ( 0 )
#else
    #define Condition_Destroy( Cond )
#endif

    // sleep this thread, waiting for condition signal
/* IPERF_MODIFIED Start */
#if   defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Wait( Cond ) pthread_cond_wait( &(Cond)->mCondition, &(Cond)->mMutex )
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    // atomically release mutex and wait on condition,
    // then re-acquire the mutex
    #define Condition_Wait( Cond ) do {                                         \
        SignalObjectAndWait( (Cond)->mMutex, (Cond)->mCondition, INFINITE, 0 ); \
        Mutex_Lock( &(Cond)->mMutex );                          \
    } while ( 0 )
#else
    #define Condition_Wait( Cond )
    /* IPERF_MODIFIED Start */
    #define Condition_Wait_Event(Cond)
    /* IPERF_MODIFIED End */
#endif

    // sleep this thread, waiting for condition signal,
    // but bound sleep time by the relative time inSeconds.
/* IPERF_MODIFIED Start */
#if   defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_TimedWait( Cond, inSeconds ) do {                         \
        struct timespec absTimeout;                                             \
        absTimeout.tv_sec  = time( NULL ) + inSeconds;                          \
        absTimeout.tv_nsec = 0;                                                 \
       pthread_cond_timedwait( &(Cond)->mCondition, &(Cond)->mMutex, &absTimeout ); \
    } while ( 0 )
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    // atomically release mutex and wait on condition,
    // then re-acquire the mutex
    #define Condition_TimedWait( Cond, inSeconds ) do {                         \
        SignalObjectAndWait( (Cond)->mMutex, (Cond)->mCondition, inSeconds*1000, false ); \
        Mutex_Lock( &(Cond)->mMutex );                          \
    } while ( 0 )
#else
    #define Condition_TimedWait( Cond, inSeconds )
#endif

    // send a condition signal to wake one thread waiting on condition
    // in Win32, this actually wakes up all threads, same as Broadcast
    // use PulseEvent to auto-reset the signal after waking all threads
/* IPERF_MODIFIED Start */
#if   defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Signal( Cond ) pthread_cond_signal( &(Cond)->mCondition )
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Signal( Cond ) PulseEvent( (Cond)->mCondition )
#else
    #define Condition_Signal( Cond )
#endif

    // send a condition signal to wake all threads waiting on condition
/* IPERF_MODIFIED Start */
#if   defined( HAVE_POSIX_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Broadcast( Cond ) pthread_cond_broadcast( &(Cond)->mCondition )
/* IPERF_MODIFIED Start */
#elif defined( HAVE_WIN32_THREAD ) && !defined(NO_THREADS)
/* IPERF_MODIFIED End */
    #define Condition_Broadcast( Cond ) PulseEvent( (Cond)->mCondition )
#else
    #define Condition_Broadcast( Cond )
#endif

#endif // CONDITION_H
