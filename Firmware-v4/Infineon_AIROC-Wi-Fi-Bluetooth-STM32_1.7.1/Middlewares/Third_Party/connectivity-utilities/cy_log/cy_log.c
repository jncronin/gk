/*
 * Copyright 2019-2024, Cypress Semiconductor Corporation (an Infineon company) or
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

/**
 * @file Cypress log routines
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "cy_result.h"
#include "cyabs_rtos.h"
#include "cy_log.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/** Size of the logging buffer */
#ifndef CY_LOGBUF_SIZE
#define CY_LOGBUF_SIZE (1024)
#endif

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    bool                init;
    cy_mutex_t          mutex;
    CY_LOG_LEVEL_T      loglevel[CYLF_MAX];
    cy_time_t           start_time;
    char                logbuf[CY_LOGBUF_SIZE];
    uint16_t            seq_num;
    log_output          platform_log;
    platform_get_time   platform_time;
    void                *worker_thread;
} cy_log_data_t;

/******************************************************
 *               Function Declarations
 ******************************************************/

/******************************************************
 *               Variables Definitions
 ******************************************************/

static cy_log_data_t cy_log;

/******************************************************
 *               Function Definitions
 ******************************************************/


/*
 * Default implementation to output the log messages. This function is called, if the user doesn't define a output logging function.
 */
static int cy_log_output(CY_LOG_FACILITY_T facility, CY_LOG_LEVEL_T level, char *logmsg)
{
    (void)facility;
    (void)level;
    printf("[F%d] : [L%d] : %s", facility, level, logmsg);

    return 1;
}

/*
 * Default implementation to get the time. This function is called, if the user doesn't provide a callback function to get time.
 */
static cy_rslt_t cy_log_get_time(cy_time_t* time)
{
    cy_rtos_get_time(time);
    return CY_RSLT_SUCCESS;
}


cy_rslt_t cy_log_init(CY_LOG_LEVEL_T level, log_output platform_output, platform_get_time platform_time)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int i;

    if (cy_log.init)
    {
        return CY_RSLT_SUCCESS;
    }

    memset(&cy_log, 0x00, sizeof(cy_log));
    cy_rtos_get_time(&cy_log.start_time);

    /*
     * Create our mutex.
     */

    result = cy_rtos_init_mutex(&cy_log.mutex);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }

    /*
     * Set the starting log level.
     */

    if ( level >= CY_LOG_MAX )
    {
        level = (CY_LOG_LEVEL_T)(CY_LOG_MAX - 1);
    }

    /* For all facilities */

    for (i = 0; i < CYLF_MAX; i++)
    {
        cy_log.loglevel[i] = level;
    }

    /*
     * Set the platform output and time routines.
     */

    if (platform_output != NULL)
    {
        cy_log.platform_log  = platform_output;
    }
    else
    {
        cy_log.platform_log = cy_log_output;
    }

    if (platform_time != NULL)
    {
        cy_log.platform_time = platform_time;
    }
    else
    {
        cy_log.platform_time = cy_log_get_time;
    }
    /*
     * All done.
     */

    cy_log.init = true;

    return result;
}


cy_rslt_t cy_log_shutdown(void)
{
    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    cy_log.init = false;

    cy_rtos_deinit_mutex(&cy_log.mutex);

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_log_set_platform_output(log_output platform_output_func)
{
    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    cy_log.platform_log = platform_output_func;

    return CY_RSLT_SUCCESS;
}


cy_rslt_t cy_log_set_platform_time(platform_get_time platform_time)
{
    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    cy_log.platform_time = platform_time;

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_log_set_facility_level(CY_LOG_FACILITY_T facility, CY_LOG_LEVEL_T level)
{
    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    if (facility >= CYLF_MAX)
    {
        facility = CYLF_DEF;
    }

    if (level >= CY_LOG_MAX)
    {
        level = (CY_LOG_LEVEL_T)(CY_LOG_MAX - 1);
    }
    cy_log.loglevel[facility] = level;

    return CY_RSLT_SUCCESS;
}


cy_rslt_t cy_log_set_all_levels(CY_LOG_LEVEL_T level)
{
    int i;

    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    if (level >= CY_LOG_MAX)
    {
        level = (CY_LOG_LEVEL_T)(CY_LOG_MAX - 1);
    }

    for (i = 0; i < CYLF_MAX; i++)
    {
        cy_log.loglevel[i] = level;
    }

    return CY_RSLT_SUCCESS;
}


CY_LOG_LEVEL_T cy_log_get_facility_level(CY_LOG_FACILITY_T facility)
{
    CY_LOG_LEVEL_T local_loglevel = CY_LOG_OFF;

    if (!cy_log.init)
    {
        return local_loglevel;
    }

    if (facility >= CYLF_MAX)
    {
        facility = CYLF_DEF;
    }

    local_loglevel = cy_log.loglevel[facility];

    return local_loglevel;
}


cy_rslt_t cy_log_msg(CY_LOG_FACILITY_T facility, CY_LOG_LEVEL_T level, const char *fmt, ...)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t time_from_start;
    cy_time_t cur_time;
    int hrs, mins, secs, ms;
    va_list args;
    int len;

    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    /* Is logging enabled for the requested level of the requested facility? */
    if (facility >= CYLF_MAX)
    {
        facility = CYLF_DEF;
    }
    if ((cy_log.platform_log == NULL) || (cy_log.loglevel[facility] == CY_LOG_OFF) || (level > cy_log.loglevel[facility]))
    {
        return CY_RSLT_SUCCESS;
    }

    /*
     * Create the time stamp.
     */

    if (cy_log.platform_time != NULL)
    {
        result = cy_log.platform_time(&time_from_start);
        if (result != CY_RSLT_SUCCESS)
        {
            return result;
        }
    }
    else
    {
        cy_rtos_get_time(&cur_time);
        time_from_start = cur_time - cy_log.start_time;
    }

    ms   = time_from_start % 1000;
    time_from_start /= 1000;

    secs = time_from_start % 60;
    time_from_start /= 60;

    mins = time_from_start % 60;
    hrs  = (time_from_start / 60) % 24;

    /*
     * We use a common buffer for composing the log messages so we need to protect against
     * multiple threads calling us simultaneously.
     */

    result = cy_rtos_get_mutex(&cy_log.mutex, CY_RTOS_NEVER_TIMEOUT);
    if (result != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    len = snprintf(cy_log.logbuf, CY_LOGBUF_SIZE, "%04d %02d:%02d:%02d.%03d ", cy_log.seq_num, hrs, mins, secs, ms);

    va_start(args, fmt);
    len = vsnprintf(&cy_log.logbuf[len], CY_LOGBUF_SIZE - len, fmt, args);
    if ((len == -1) || (len >= CY_LOGBUF_SIZE))
    {
        /* The vsnprintf() output was truncated. */
        cy_log.logbuf[CY_LOGBUF_SIZE - 1] = '\0';
    }
    va_end(args);

    cy_log.platform_log(facility, level, cy_log.logbuf);

    /* increment sequence number for next line*/
    cy_log.seq_num++;

    cy_rtos_set_mutex(&cy_log.mutex);

    return result;
}

cy_rslt_t cy_log_printf(const char *fmt, ...)
{
    cy_rslt_t result;
    va_list args;

    va_start(args, fmt);
    result = cy_log_vprintf(fmt, args);
    va_end(args);

    return result;
}

cy_rslt_t cy_log_vprintf(const char *fmt, va_list varg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int len;

    if (!cy_log.init)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    /*
     * We use a common buffer for composing the log messages so we need to protect against
     * multiple threads calling us simultaneously.
     */

    result = cy_rtos_get_mutex(&cy_log.mutex, CY_RTOS_NEVER_TIMEOUT);
    if (result != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_TYPE_ERROR;
    }

    len = vsnprintf(cy_log.logbuf, CY_LOGBUF_SIZE, fmt, varg);
    if ((len == -1) || (len >= CY_LOGBUF_SIZE))
    {
        /* The vsnprintf() output was truncated. */
        cy_log.logbuf[CY_LOGBUF_SIZE - 1] = '\0';
    }

    cy_log.platform_log(CYLF_DEF, CY_LOG_PRINTF, cy_log.logbuf);

    cy_rtos_set_mutex(&cy_log.mutex);

    return result;
}
#ifdef __cplusplus
}
#endif
