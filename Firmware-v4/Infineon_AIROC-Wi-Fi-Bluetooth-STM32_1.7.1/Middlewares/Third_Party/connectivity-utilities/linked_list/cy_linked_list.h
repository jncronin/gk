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
 * This is a generic linked list library with helper functions to add, insert, delete and find nodes in a list.
 * 
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup linkedlist_utils 
 * This is a generic linked list library with helper functions to add, insert, delete and find nodes in a list.
 */

#include <stdbool.h>
#include <stdint.h>
#include "cy_result_mw.h"
/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
/** Results returned by linked list library */
#define CY_RSLT_MODULE_LINKED_LIST_ERR_CODE_START          (0) /** Linked list result code base */
#define CY_RSLT_LINKED_LIST_ERROR_BASE                     CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_LINKED_LIST_BASE, CY_RSLT_MODULE_LINKED_LIST_ERR_CODE_START)

#define CY_RSLT_LINKED_LIST_BADARG                         ((cy_rslt_t)(CY_RSLT_LINKED_LIST_ERROR_BASE + 1)) /** Linked list error bad argument */
#define CY_RSLT_LINKED_LIST_NOT_FOUND                      ((cy_rslt_t)(CY_RSLT_LINKED_LIST_ERROR_BASE + 2)) /** Linked list error not found */

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/
/******************************************************************************/
/** \addtogroup group_linkedlist_structures 
 * Documentation of data structures and typedefs provided by the linked list utility.
 *//** \{ */
/******************************************************************************/

/** Linked list node structure */
typedef struct linked_list_node cy_linked_list_node_t;

/******************************************************
 *                    Structures
 ******************************************************/
/** Linked list node object */
struct linked_list_node
{
    void*               data;    /**< Data object of the node */
    cy_linked_list_node_t* next; /**< Pointer to the next node in the list */
    cy_linked_list_node_t* prev; /**< Pointer to the previous node in the list */
};

/** Linked list object */
typedef struct
{
    uint32_t            count;    /**< Number of nodes in the list */
    cy_linked_list_node_t* front; /**< Pointer to the front of the list */
    cy_linked_list_node_t* rear;  /**< Pointer to the rear of the list */
} cy_linked_list_t;

/** Callback to compare the data in the node for find routine */
typedef bool (*cy_linked_list_compare_callback_t)( cy_linked_list_node_t* node_to_compare, void* user_data );

/** \} */
/******************************************************
 *                 Global Variables
 ******************************************************/

/*****************************************************************************/
/**
 *
 *  @addtogroup group_linkedlist_func
 *
 * This is a generic linked list library with helper functions to add, insert, delete and find nodes in a list.
 *
 *  @{
 */
/******************************************************
 *               Function Declarations
 ******************************************************/

 /** Initialize list
 *
 * @param [in]  list   - Pointer to a list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_init( cy_linked_list_t* list );

 /** De-initialize list
 *
 * @param [in]  list   - Pointer to a list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_deinit( cy_linked_list_t* list );

 /** Get the count for number of nodes present in list
 *
 * @param [in]  list   - Pointer to an initialized list
 * @param [out] count  - number of nodes present in list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_get_count( cy_linked_list_t* list, uint32_t* count );

 /** Set the data for linked list node
 *
 * @param [in]  node   - linked list node for which data need to be set
 * @param [in]  data   - data value for node
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_set_node_data( cy_linked_list_node_t* node, const void* data );

 /** Get the front node in list
 *
 * @param [in]  list        - Pointer to an initialized list
 * @param [out] front_node  - front node in the list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_get_front_node( cy_linked_list_t* list, cy_linked_list_node_t** front_node );

 /** Get the rear node in list
 *
 * @param [in]  list       - Pointer to an initialized list
 * @param [out] rear_node  - rear node in the list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_get_rear_node( cy_linked_list_t* list, cy_linked_list_node_t** rear_node );

 /** Find particular node in the list
 *
 * @param [in]  list       - Pointer to an initialized list
 * @param [in]  callback   - callback to compare node data in the list
 * @param [in]  user_data  - user data
 * @param [out] node_found - node_found in the list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 *                     - CY_RSLT_LINKED_LIST_NOT_FOUND
 */
cy_rslt_t cy_linked_list_find_node( cy_linked_list_t* list, cy_linked_list_compare_callback_t callback, void* user_data, cy_linked_list_node_t** node_found );

 /** Insert node at the front of the list
 *
 * @param [in]  list    - Pointer to an initialized list
 * @param [in]  node    - node to be added at the front of the list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_insert_node_at_front( cy_linked_list_t* list, cy_linked_list_node_t* node );

 /** Insert node at the rear of the list
 *
 * @param [in]  list    - Pointer to an initialized list
 * @param [in]  node    - node to be added at the rear of the list
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 */
cy_rslt_t cy_linked_list_insert_node_at_rear( cy_linked_list_t* list, cy_linked_list_node_t* node );

 /** Insert node before reference node
 *
 * @param [in]  list              - Pointer to an initialized list
 * @param [in]  reference_node    - Pointer to a node before which you want to add new node
 * @param [in]  node_to_insert    - Pointer to a node to be inserted
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 *                     - CY_RSLT_LINKED_LIST_NOT_FOUND
 */
cy_rslt_t cy_linked_list_insert_node_before( cy_linked_list_t* list, cy_linked_list_node_t* reference_node, cy_linked_list_node_t* node_to_insert );

 /** Insert node after reference node
 *
 * @param [in]  list              - Pointer to an initialized list
 * @param [in]  reference_node    - Pointer to a node after which you want to add new node
 * @param [in]  node_to_insert    - Pointer to a node to be inserted
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 *                     - CY_RSLT_LINKED_LIST_NOT_FOUND
 */
cy_rslt_t cy_linked_list_insert_node_after( cy_linked_list_t* list, cy_linked_list_node_t* reference_node, cy_linked_list_node_t* node_to_insert );

 /** Remove the node from the list
 *
 * @param [in]  list              - Pointer to an initialized list
 * @param [in]  node              - Pointer to a linked list node to be deleted
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 *                     - CY_RSLT_LINKED_LIST_NOT_FOUND
 */
cy_rslt_t cy_linked_list_remove_node( cy_linked_list_t* list, cy_linked_list_node_t* node );

 /** Remove node from front of the list
 *
 * @param [in]  list              - Pointer to an initialized list
 * @param [out] removed_node      - removed node
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 *                     - CY_RSLT_LINKED_LIST_NOT_FOUND
 */
cy_rslt_t cy_linked_list_remove_node_from_front( cy_linked_list_t* list, cy_linked_list_node_t** removed_node );

 /** Remove node from rear of the list
 *
 * @param [in]  list              - Pointer to an initialized list
 * @param [out] removed_node      - removed node
 *
 *  @return on success - CY_RSLT_SUCCESS
 *          on failure - CY_RSLT_LINKED_LIST_BADARG
 *                     - CY_RSLT_LINKED_LIST_NOT_FOUND
 */
cy_rslt_t cy_linked_list_remove_node_from_rear( cy_linked_list_t* list, cy_linked_list_node_t** removed_node );

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif
