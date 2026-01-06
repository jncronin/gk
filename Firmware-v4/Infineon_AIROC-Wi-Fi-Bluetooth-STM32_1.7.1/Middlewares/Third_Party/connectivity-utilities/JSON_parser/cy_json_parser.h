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
 * @file
 * The JSON parser utility library provides helper functions to parse JSON objects
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cy_result_mw.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @addtogroup json_utils 
 * 
 * The JSON format is often used for serializing and transmitting structured data over a network connection. 
 * It is used primarily to transmit data between a server and web application, serving as an alternative to XML. 
 * JSON is JavaScript Object Notation. The JSON parser utility library provides helper functions to parse JSON objects and 
 * calls the function registered by the caller
 * 
 */
/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
/** Results returned by JSON library */
#define CY_RSLT_MODULE_JSON_ERR_CODE_START          (0) /** JSON parser result code base */
#define CY_RSLT_JSON_ERROR_BASE                     CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_JSON_BASE, CY_RSLT_MODULE_JSON_ERR_CODE_START)

#define CY_RSLT_JSON_GENERIC_ERROR                  ((cy_rslt_t)(CY_RSLT_JSON_ERROR_BASE + 1)) /** JSON parser generic error result */

#define OBJECT_START_TOKEN        '{'
#define OBJECT_END_TOKEN          '}'

#define ARRAY_START_TOKEN         '['
#define ARRAY_END_TOKEN           ']'

#define STRING_TOKEN              '"'

#define START_OF_VALUE            ':'

#define COMMA_SEPARATOR           ','

#define ESCAPE_TOKEN              '\\'

#define TRUE_TOKEN                't'

#define FALSE_TOKEN               'f'

#define NULL_TOKEN                'n'

/******************************************************
 *                   Enumerations
 ******************************************************/
/******************************************************************************/
/** \addtogroup group_json_enums 
 * Documentation of all the enums provided by JSON parser utility library.
 *//** \{ */
/******************************************************************************/

/** JSON data types */
typedef enum
{
    JSON_STRING_TYPE,  /**< JSON string datatype */
    JSON_NUMBER_TYPE,  /**< JSON integer datatype */
    JSON_VALUE_TYPE,   /**< JSON value datatype. 
                        **< This type is deprecated and it will be removed in the future release.
                        */
    JSON_FLOAT_TYPE,   /**< JSON float datatype */
    JSON_ARRAY_TYPE,   /**< JSON array datatype */
    JSON_OBJECT_TYPE,  /**< JSON object */
    JSON_BOOLEAN_TYPE, /**< JSON boolean datatype */
    JSON_NULL_TYPE,    /**< JSON null object */
    UNKNOWN_JSON_TYPE  /**< JSON unknown type */
} cy_JSON_type_t;

/** \} */

/******************************************************
 *                 Type Definitions
 ******************************************************/
/******************************************************************************/
/** \addtogroup group_json_structures 
 * Documentation of the data structures defined and used by JSON parser utility library.
 *//** \{ */
/******************************************************************************/

/******************************************************
 *                    Structures
 ******************************************************/
/** JSON parser object */
typedef struct cy_JSON_object {

    char*               object_string;        /**< JSON object as a string */
    uint8_t             object_string_length; /**< Length of the JSON string */
    cy_JSON_type_t      value_type;           /**< JSON data type of value parsed */
    char*               value;                /**< JSON string value parsed */
    uint16_t            value_length;         /**< length of string value parsed */
    uint32_t            intval;               /**< JSON integer value parsed */
    float               floatval;             /**< JSON float value parsed */
    bool                boolval;              /**< JSON boolean value parsed */
    struct cy_JSON_object* parent_object;     /**< Pointer to parent JSON object */
} cy_JSON_object_t;

/** Callback function used for registering with JSON parse
 *
 * @param[in] json_object : JSON object which contains the key=value pair parsed by the JSON parser
 * @param[in] arg         : User argument passed in when registering the callback
 *
 * @return cy_rslt_t
 */
typedef cy_rslt_t (*cy_JSON_callback_t)( cy_JSON_object_t* json_object, void *arg );

/** \} */

#define JSON_IS_NOT_ESC_CHAR( ch ) ( ( ch != '\b' )  &&  \
                                     ( ch != '\f' ) &&  \
                                     ( ch != '\n' ) &&  \
                                     ( ch != '\r' ) &&  \
                                     ( ch != '\t' ) &&  \
                                     ( ch != '\"' ) &&  \
                                     ( ch != '\\' ) )

/******************************************************
 *                 Global Variables
 ******************************************************/

/*****************************************************************************/
/**
 *
 *  @addtogroup group_json_func
 *
 * The JSON parser utility library provides helper functions to parse JSON objects
 *
 *
 *  @{
 */
/*****************************************************************************/


/** Register callback to be invoked by JSON parser while parsing the JSON data
 *
 * @param[in] json_callback : Pointer to the callback function to be invoked while parsing the JSON data
 * @param[in] arg           : User argument passed in when registering the callback
 *
 * @return cy_rslt_t
 */
cy_rslt_t cy_JSON_parser_register_callback( cy_JSON_callback_t json_callback, void *arg );

/** Returns the current callback function registered with the JSON parser
 *
 * @return @ref cy_JSON_callback_t
 */
cy_JSON_callback_t cy_JSON_parser_get_callback( void );

/** Parse the JSON data.
 *
 *  This function will parse the JSON input string through a single parse, calling a callback whenever it encounters milestones
 *  an object, passing in object name, JSON value type, and a value (if value is string or number )
 *
 * @param[in] json_input   : Pointer to the JSON data
 * @param[in] input_length : Length of the JSON data pointed by `json_input1`
 *
 * @return cy_rslt_t
 *
 * @note: Currently escape values are not supported.
 */
cy_rslt_t cy_JSON_parser( const char* json_input, uint32_t input_length );

/** @} */

#ifdef __cplusplus
} /*extern "C" */
#endif
