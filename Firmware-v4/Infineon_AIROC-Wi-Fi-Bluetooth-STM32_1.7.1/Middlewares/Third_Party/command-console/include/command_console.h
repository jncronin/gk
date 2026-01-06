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

/**
* @file command_console.h
* @brief The Command Console library provides a framework to add command console support to your application (or) product use cases.
* Support for Wi-Fi iPerf and Bluetooth BLE commands is bundled with this library.
*
*/

#pragma once

#include "cy_result_mw.h"
#include "command_utility.h"
#include "cyabs_rtos_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_TABLE_END      { NULL, NULL, 0, NULL, NULL, NULL, NULL }
#define MAX_LINE_LENGTH    (128)
#define MAX_HISTORY_LENGTH (20)

/* To enable polling; uncomment the below line. */
//#define ENABLE_UART_POLLING

#if !defined(COMPONENT_CAT5)
#define ENABLE_UART_POLLING
#endif

/**
 * \defgroup group_cmd_console_macros Macros
 * \defgroup group_cmd_console_enums Enumerated Types
 * \defgroup group_cmd_console_typedefs Typedefs
 * \defgroup group_cmd_console_structs Structures
 * \defgroup group_cmd_console_functions Functions
 */

/**
 * @defgroup generic_cmd_console_defines Command Console library results/error codes
 * @ingroup group_cmd_console_macros
 * Cypress middleware APIs return results of type cy_rslt_t and consist of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result Format
 *
   \verbatim
              Module base                      Type    Library-specific error code
      +--------------------------------------+------+------------------------------+
      |CY_RSLT_MODULE_COMMAND_CONSOLE_BASE   | 0x2  |           Error Code         |
      +--------------------------------------+------+------------------------------+
                    14-bits                   2-bits            16-bits

   See the macro section of this document for library-specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h located in <core_lib/include>.
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of the CY_RSLT_MODULE_MIDDLEWARE_BASE.
 *              The details of the offset and the middleware base are defined in cy_result_mw.h, which is part of [GitHub connectivity-utilities] (https://github.com/infineon/connectivity-utilities).
 *              For example, the Command Console library uses CY_RSLT_MODULE_COMMAND_CONSOLE_BASE as the module base.
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING, or CY_RSLT_TYPE_INFO. AWS library error codes are of type CY_RSLT_TYPE_ERROR.
 *
 * Library-specific error code: These error codes are library-specific and are defined in the macro section.
 *
 * Helper macros used for creating the library-specific result are provided as part of cy_result.h.
 * \{
 */

/** Generic command console base error code. */
#define CY_RSLT_COMMAND_CONSOLE_ERR_BASE                    CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_COMMAND_CONSOLE_BASE, 0)

#define CY_RSLT_COMMAND_CONSOLE_FAILURE                     ((cy_rslt_t)(CY_RSLT_COMMAND_CONSOLE_ERR_BASE + 1)) /**< Generic failure. */

/** \} generic_cmd_console_defines */

/**
 * \addtogroup group_cmd_console_enums
 * \{
 */

/******************************************************
 *                   Enumerations
 ******************************************************/

/**
 * Internal command console error codes.
 */
typedef enum
{
    ERR_CMD_OK           =  0,     /**< Error code - success. */
    ERR_UNKNOWN          = -1,     /**< Error code - unknown error. */
    ERR_UNKNOWN_CMD      = -2,     /**< Error code - unknown command. */
    ERR_INSUFFICENT_ARGS = -3,     /**< Error code - Insufficient arguments passed. */
    ERR_TOO_MANY_ARGS    = -4,     /**< Error code - Too many arguments passed. */
    ERR_ADDRESS          = -5,     /**< Error code - Bad address. */
    ERR_NO_CMD           = -6,     /**< Error code - No command entered. */
    ERR_TOO_LARGE_ARG    = -7,     /**< Error code - Argument too large. */
    ERR_OUT_OF_HEAP      = -8,     /**< Error code - Out of heap memory. */
    ERR_BAD_ARG          = -9,     /**< Error code - Bad argument passed */
/* !!!when adding values here, also update command_console.c:console_default_error_strings. */
    ERR_LAST_ERROR       = -10     /**< Error code - Do not use. */
} cy_command_console_err_t;

/** \} group_cmd_console_enums */

/**
 * \addtogroup group_cmd_console_structs
 * \{
 */

/**
 * Command console Type-Length-Value buffer.
 */
typedef struct
{
	uint8_t   type;      /**< Type of value: 0-ASCII, 1-Binary. */
	uint8_t   length;    /**< Length of value in bytes. */
	uint8_t   value[];   /**< Buffer containing the value. */
} tlv_buffer_t;

/** \} group_cmd_console_structs */

/**
 * \addtogroup group_cmd_console_typedefs
 * \{
 */
/**
 * Callback to be invoked when a command is executed.
 */
typedef int (*command_function_t)(int argc, char *argv[], tlv_buffer_t** data);

/**
 * Pointer to a function for command-specific help. Can be NULL.
 */
typedef cy_command_console_err_t (*help_example_function_t)( char* command_name, uint32_t eg_select );

/** \} group_cmd_console_typedefs */

/**
 * \addtogroup group_cmd_console_structs
 * \{
 */

/**
 * Structure to hold the information for each console command.
 */
typedef struct
{
    const char* name;                       /**< Command name matched at the command line. */
    command_function_t command;             /**< Function that runs the command. */
    int arg_count;                          /**< Minimum number of arguments. */
    const char* delimit;                    /**< Custom string of characters that may delimit the arguments for this command - NULL value will use the default for the console. */

    /*
     * These three elements are used only by the help, not the console dispatching code.
     * The default help function will not produce a help entry if both format and brief elements
     * are set to NULL (good for adding synonym or short form commands).
     */
    help_example_function_t help_example;   /**< Command-specific help function. Generally set to NULL. */
    const char *format;                     /**< String describing the argument format used by the generic help generator function. */
    const char *brief;                      /**< Brief description of the command used by the generic help generator function. */
} cy_command_console_cmd_t;

/**
 * Structure to configure the command console framework.
 */
typedef struct
{
    /* Base parameters */
    void* serial;                          /**< Pointer to the serial object. */
    uint32_t line_len;                     /**< Size of buffer to store command. */
    char* buffer;                          /**< Pointer pointing to the buffer provided by the application to store the command given from the console. */
    uint32_t history_len;                  /**< Size of the buffer to store the command history. */
    char* history_buffer_ptr;			   /**< Pointer pointing to the buffer provided by the application to store the command history. */
    const char* delimiter_string;          /**< Delimiter string which separates out the command line arguments. */
    cy_thread_priority_t thread_priority;  /**< Command console UART thread priority. NOTE: For iperf throughput measurements, it's recommended to set the priority to "CY_RTOS_PRIORITY_HIGH -1" */
    uint8_t params_num;                    /**< Maximum parameters allowed. */
} cy_command_console_cfg_t;

/** \} group_cmd_console_structs */

/**
 * \addtogroup group_cmd_console_functions
 * \{
 */
/**
 * Initializes the Command console framework and spawns a thread that listens on the console for commands. To be called by the application once.
 *
 * @param[in]  cfg      : Pointer to the command console configuration.
 *
 * @return CY_RSLT_SUCCESS if the initialization was successful; returns command-console-specific error codes. See above.
 */
cy_rslt_t cy_command_console_init ( cy_command_console_cfg_t *cfg);

/**
 * De-initializes the command console framework and terminates the thread.
 *
 * @return CY_RSLT_SUCCESS if the initialization was successful; returns command-console-specific error codes. See above.
 */
cy_rslt_t cy_command_console_deinit ( void );

/**
 * Invoked to register a table of cy_command_console_cmd_t entries.
 *
 * @param[in]  commands    : Pointer to the commands.
 *
 * @return CY_RSLT_SUCCESS if the initialization was successful; returns command-console-specific error codes. See above.
 */
cy_rslt_t cy_command_console_add_table ( const cy_command_console_cmd_t *commands );

/**
 * De-registers the cy_command_console_cmd_t table.
 *
 * @param[in]  commands    : Pointer to the commands.
 *
 * @return CY_RSLT_SUCCESS if the initialization was successful; returns command-console-specific error codes. See above.
 */
cy_rslt_t cy_command_console_delete_table ( const cy_command_console_cmd_t *commands );

/** \} group_cmd_console_functions */

#ifdef __cplusplus
}
#endif
