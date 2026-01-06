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

/** @file
 *
 */
#ifndef DISABLE_COMMAND_CONSOLE_IPERF
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "command_console.h"
#include "iperf_utility.h"
#include "iperf_sockets.h"
#include "cyabs_rtos.h"
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
extern int iperf( int argc, char *argv[], tlv_buffer_t** data );
int iperf_test(int argc, char *argv[], tlv_buffer_t** data);
/******************************************************
 *               Variables Definitions
 ******************************************************/

 static uint32_t wait_bits = 0x1;

#define IPERF_COMMANDS \
    { "iperf", iperf_test, 0, NULL, NULL, (char *)"", (char *)"Run iperf --help for usage."}, \

#define IPERF_THREAD_STACK_SIZE 4096

#if defined(__ICCARM__)
#define IPERF_WEAK_FUNC            __WEAK
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define IPERF_WEAK_FUNC            __attribute__((weak))
#else
#define IPERF_WEAK_FUNC           __attribute__((weak))
#endif

const cy_command_console_cmd_t iperf_command_table[] =
{
    IPERF_COMMANDS
    CMD_TABLE_END
};

#pragma pack(1)
struct cmd_result
{
    char status;
    char flag;
    int  count;
};

struct iperf_args
{
    int argc;
    char **argv;
};

typedef struct
{
    bool    available;
    struct  iperf_args args;
    cy_thread_t  *thread;
} iperf_thread_t;

#ifdef COMPONENT_CAT5
/* MAX_SIMULTANEOUS_COMMANDS is 1 for CAT5 devices. Hence a single thread is sufficient */
cy_thread_t iperf_thread;
#endif

static cy_event_t event;
iperf_thread_t iperf_util_threads[MAX_SIMULTANEOUS_COMMANDS];

/******************************************************
 *               Function Definitions
 ******************************************************/
IPERF_WEAK_FUNC void iperf_utility_init( void *networkInterface)
{
    int i;
    iperf_network_init( networkInterface );
    cy_command_console_add_table( iperf_command_table );

    for( i = 0; i < MAX_SIMULTANEOUS_COMMANDS; i++)
    {
        iperf_util_threads[i].available = true;
    }

    cy_rtos_init_event(&event);
}
int iperf_util_find_free_thread()
{
    int j = -1;

    for(int i = 0; i < MAX_SIMULTANEOUS_COMMANDS; i++)
    {
        if(iperf_util_threads[i].available == true)
        {
            if(iperf_util_threads[i].thread != NULL)
            {
                cy_rtos_join_thread(iperf_util_threads[i].thread);
#ifndef COMPONENT_CAT5
                delete iperf_util_threads[i].thread;
#endif
                iperf_util_threads[i].thread = NULL;
            }
            iperf_util_threads[i].available = false;
            return i;
        }
    }
    return j;
}

void iperf_util_thread(cy_thread_arg_t arg)
{
    int index;

    index = *((int *)arg);
    cy_rtos_setbits_event(&event, wait_bits, false);
    iperf ( iperf_util_threads[index].args.argc, iperf_util_threads[index].args.argv, NULL);
    iperf_util_threads[index].available = true;
    printf("IPERF[%d] EXITED\n", index);
    cy_rtos_exit_thread();
}

int iperf_test(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    int index;
    cy_thread_arg_t args;
    cy_rslt_t res;
    int i = 1;
    bool is_server = false;

    index = iperf_util_find_free_thread();
    if( index == -1 )
    {
        printf("Error: exceeded maximum number of simultaneous commands supported\n");
        return -1;
    }

    iperf_util_threads[index].args.argc = argc;
    iperf_util_threads[index].args.argv = argv;
#ifdef COMPONENT_CAT5
    iperf_util_threads[index].thread = &iperf_thread;
#else
    iperf_util_threads[index].thread = new cy_thread_t;
#endif

    args = (cy_thread_arg_t)(&index);
    while (i <= (argc - 1))
    {
        if (strcasecmp (argv[i], "-s") == 0)
        {
            is_server = true;
        }
        i = i+1 ;
    }

    if ( is_server ==  true)
    {
        res = cy_rtos_create_thread(iperf_util_threads[index].thread, iperf_util_thread, NULL, NULL, IPERF_THREAD_STACK_SIZE, (cy_thread_priority_t)(CY_RTOS_PRIORITY_HIGH), args);
    }
    else
    {
        res = cy_rtos_create_thread(iperf_util_threads[index].thread, iperf_util_thread, NULL, NULL, IPERF_THREAD_STACK_SIZE, (cy_thread_priority_t)(CY_RTOS_PRIORITY_HIGH - 1), args);
    }

    if(res != CY_RSLT_SUCCESS)
    {
        printf("Error creating thread \n");
        return -1;
    }

    cy_rtos_waitbits_event(&event, &wait_bits, true, true, CY_RTOS_NEVER_TIMEOUT);

    return result;
}

#ifdef __cplusplus
}
#endif
#endif