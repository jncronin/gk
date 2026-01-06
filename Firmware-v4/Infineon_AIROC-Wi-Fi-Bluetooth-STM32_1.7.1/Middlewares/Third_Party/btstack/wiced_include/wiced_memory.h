/*
 * Copyright 2019-2023, Cypress Semiconductor Corporation or
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
 *  \addtogroup wiced_mem Memory Management
 *
 *  @{
 * Helper APIs to create heaps and pools and allocate/free buffers from those pools or heaps.
 * When a heap or a pool is created, this utility allocates required chunk of memory from the system and manages it for the creator.
 */

#pragma once

#include "wiced_data_types.h"
#include "wiced_result.h"


/* WICED does not care about the structure of the contents of a WICED buffer */
/** WICED BT Buffer */
typedef void wiced_bt_buffer_t;

/* Application does not know or care about structure used to manage buffer pools or heaps */
/** WICED BT Pool */
typedef struct t_wiced_bt_pool wiced_bt_pool_t;
/** WICED BT Heap */
typedef struct t_wiced_bt_heap wiced_bt_heap_t;

/** If an application wants to get a buffer from the default heap, he can use this. */
#define WICED_DEFAULT_HEAP      ((wiced_bt_heap_t *)NULL)

/** wiced bt buffer pool statistics */
typedef struct wiced_bt_pool_statistics_s
{
    uint16_t    pool_size;                  /**< pool buffer size */
    uint16_t    pool_count;                 /**< total number of buffers created in the pool */
    uint16_t    current_allocated_count;    /**< number of buffers currently allocated */
    uint16_t    max_allocated_count;        /**< maximum number of buffers ever allocated  */
}wiced_bt_pool_statistics_t;


/** wiced bt heap statistics */
typedef struct wiced_bt_heap_statistics_s
{
    uint16_t    heap_size;                   /**< heap size */

    uint16_t    max_single_allocation;       /**< max individual size allocated from the heap till now */
    uint16_t    max_heap_size_used;          /**< high watermark/peak heap size used. if this size is > 80% then we are running close to edge */
    uint16_t    allocation_failure_count;    /**< number of times allocation failed */

    uint16_t    current_num_allocations;     /**< number of fragments currently allocated */
    uint16_t    current_size_allocated;      /**< total size of current allocations */

    uint16_t    current_largest_free_size;   /**< largest free fragment size, which can be allocated  */
    uint16_t    current_num_free_fragments;  /**< num of free fragments */
    uint16_t    current_free_size;           /**< total free size of all fragments */
} wiced_bt_heap_statistics_t;


/** This queue is a general purpose buffer queue, for application use.*/
typedef struct
{
    wiced_bt_buffer_t   *p_first;       /**< Pointer to first buffer */
    wiced_bt_buffer_t   *p_last;        /**< Pointer to Last buffer */
    uint32_t             count;         /**< Number of buffer in queue */
    wiced_bt_lock_t      lock;          /**< Lock provided by appliction */
} wiced_bt_buffer_q_t;

/******************************************************
 *               Function Declarations
 ******************************************************/
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the number of free bytes of RAM left
 *
 * @return          the number of free bytes of RAM left
 */
uint32_t wiced_memory_get_free_bytes( void );

/**
* Initializes dynamic memory area. Application reserves an area for dynamic
* variable memory allocations with this call. Application can now allocate
* variable sized buffers as against fixed sized buffer allocations from the
* pools initialized with calls to wiced_bt_create_pool and allocated using
* wiced_bt_get_buffer_from_pool.
*
* @param[in]       name : Friendly name of the heap
* @param[in]       p_area : Pointer to area to use for the heap. If NULL, WICED will allocate the area.
* @param[in]       size : Size the area passed in. If no area passed in, this is the size of the heap desired.
* @param[in]       p_lock : Pointers to lock functions to use during heap manipulation. If NULL, then
*                  it is assumed that the application handles disabling of preemption.
* @param[in]       b_make_default: Flag as to whether the heap will be the default heap.
*
* @return          wiced_bt_heap_t * - pointer to heap, or NULL if the heap creation failed.
*/
wiced_bt_heap_t *wiced_bt_create_heap (const char * name, void *p_area, int size, wiced_bt_lock_t *p_lock, wiced_bool_t b_make_default);

/**
* Deletes the heap created with #wiced_bt_create_heap. The heap area is freed back to the platform in
* case the wiced_bt_create_heap was called with p_area set to NULL.
*
* @param[in]       p_heap : Heap created with #wiced_bt_create_heap
*
* @return          void
*/
void wiced_bt_delete_heap(wiced_bt_heap_t* p_heap);

/**
 * Creates a buffer pool for application usage.
 *
 * @param[in]       name : Friendly name of the heap
 * @param[in]       buffer_size: Size of the buffers in the pool
 * @param[in]       buffer_cnt : Number of buffers in the pool
 * @param[in]       p_lock : Pointers to lock functions to use during heap manipulation. If NULL, then
 *                  it is assumed that the application handles disabling of preemption.
 *
 * @return         pointer to the created pool on success, or NULL on failure
 */
wiced_bt_pool_t* wiced_bt_create_pool(const char* name, uint32_t buffer_size, uint32_t buffer_cnt, wiced_bt_lock_t* p_lock);

/**
 * Deletes a buffer pool created using #wiced_bt_create_pool
 *
 * @param[in]       p_pool : pointer of type #wiced_bt_pool_t returned through a call to #wiced_bt_create_pool
 *
 * @return         void
 */
void wiced_bt_delete_pool(wiced_bt_pool_t* p_pool);


/** Get buffer from requested pool */
/**
 * Allocates a buffer from the requested pool.
 *
 * @param[in]       p_pool  : pointer to pool from which to get the buffer
 *
 * @return         the pointer to the buffer or NULL on failure
 */
wiced_bt_buffer_t* wiced_bt_get_buffer_from_pool (wiced_bt_pool_t* p_pool);

/**
 * Allocates a buffer from the requested heap.
 *
 * @param[in]       p_heap  : pointer to heap from which to get the buffer
 * @param[in]       size  : size to be allocated
 *
 * @return         the pointer to the buffer or NULL on failure
 */
wiced_bt_buffer_t* wiced_bt_get_buffer_from_heap (wiced_bt_heap_t* p_heap, uint32_t size);

/**
 * Allocates a buffer from the <b> DEFAULT heap </b>.
 *
 * @param[in]       size  : size to be allocated
 *
 * @return         the pointer to the buffer or NULL on failure
 */

#define wiced_bt_get_buffer(size) wiced_bt_get_buffer_from_heap(WICED_DEFAULT_HEAP, (size))


/**
 * To get the number of buffers available in the pool
 *
 * @param[in]       p_pool           : pool pointer
 *
 * @return         the number of buffers available in the pool
 */
uint32_t wiced_bt_get_pool_free_count (wiced_bt_pool_t* p_pool);

/**
* To get the size of the largest buffer available in the heap
*
* @param[in]       p_heap           : heap pointer
*
* @return         the size of the largest buffer available in the heap
*/
uint32_t wiced_bt_get_largest_heap_buffer (wiced_bt_heap_t* p_heap);

/**
* To get the size of the largest buffer available in the stack heap
*
* @return  the size of the largest buffer available in the stack heap
*/
uint32_t wiced_bt_get_largest_stack_heap_buffer(void);

/**
 * Frees a buffer back to the pool or heap it came from
 *
 * @param[in]       p_buf : pointer to the start of the (pool/heap) buffer to be freed
 *
 * @return         None
 */
void wiced_bt_free_buffer (wiced_bt_buffer_t* p_buf);

/**
 * Gets the buffer size
 *
 * @param[in]       p_buf           : pointer to the start of the buffer
 *
 * @return        the buffer size
 */
uint32_t wiced_bt_get_buffer_size (wiced_bt_buffer_t *p_buf);

/**
 * Called by an application to initialize a WICED buffer queue.
 * Pointers to lock and unlock functions may be NULL if application
 * has handled preemption outside of the queue management code.
 *
 * @return          void
 */
void wiced_bt_init_q (wiced_bt_buffer_q_t* p_q, wiced_bt_lock_t * p_lock);

/**
 * Enqueue a buffer at the tail of the queue
 *
 * @param[in]       p_q   : pointer to a queue.
 * @param[in]       p_buf : address of the buffer to enqueue
 *
 * @return          void
 */
void wiced_bt_enqueue (wiced_bt_buffer_q_t *p_q, wiced_bt_buffer_t *p_buf);

/**
 * Enqueue a buffer at the head of the queue
 *
 * @param[in]       p_q   : pointer to a queue.
 * @param[in]       p_buf : address of the buffer to enqueue
 *
 * @return          void
 */
void wiced_bt_enqueue_head (wiced_bt_buffer_q_t *p_q, wiced_bt_buffer_t *p_buf);

/**
 * Dequeues a buffer from the head of a queue
 *
 * @param[in]       p_q  : pointer to a queue.
 *
 * @return          NULL if queue is empty, else buffer
 */
wiced_bt_buffer_t *wiced_bt_dequeue (wiced_bt_buffer_q_t *p_q);

/**
 * Dequeue a buffer from the middle of the queue
 *
 * @param[in]       p_q   : pointer to a queue.
 * @param[in]       p_buf : address of the buffer to dequeue
 *
 * @return          NULL if queue is empty, else buffer
 */
wiced_bt_buffer_t *wiced_bt_remove_from_queue (wiced_bt_buffer_q_t *p_q, wiced_bt_buffer_t *p_buf);

/**
 * Return a pointer to the first buffer in a queue
 *
 * @param[in]       p_q : pointer to a queue.
 *
 * @return          NULL if queue is empty, else buffer address
 */
wiced_bt_buffer_t *wiced_bt_getfirst (wiced_bt_buffer_q_t *p_q);

/**
 * Return a pointer to the last buffer in a queue
 *
 * @param[in]       p_q  : pointer to a queue.
 *
 * @return          NULL if queue is empty, else buffer address
 */
wiced_bt_buffer_t *wiced_bt_getlast (wiced_bt_buffer_q_t *p_q);

/**
 * Return a pointer to the next buffer in a queue
 *
 * @param[in]       p_buf  : pointer to the buffer to find the next one from.
 *
 * @return          NULL if no more buffers in the queue, else next buffer address
 */
wiced_bt_buffer_t *wiced_bt_getnext (wiced_bt_buffer_t *p_buf);

/**
 * Check the status of a queue.
 *
 * @param[in]       p_q  : pointer to a queue.
 *
 * @return          TRUE if queue is empty, else FALSE
 */
uint32_t wiced_bt_queue_is_empty (wiced_bt_buffer_q_t *p_q);

/**
 * Get the number of items in the queue.
 *
 * @param[in]       p_q  : pointer to a queue.
 *
 * @return          number of items in the queue
 */
uint32_t wiced_bt_queue_get_count(wiced_bt_buffer_q_t* p_q);

/**
 * Allocate long term memory, typically used for control blocks
 * allocated through config, not expected to be freed during the
 * lifetime of the application
 *
 * @param[in]  size  : size of memory to be allocated
 * @param[in]  block_name: friendly name of memory block to be allocated.
 *
 * @return     pointer to the allocated memory
 */
wiced_bt_buffer_t *wiced_memory_alloc_long_term_mem_block(int size, const char *block_name);

/**
 * Free long term memory, used to free memory allocated with
 * \ref wiced_memory_alloc_long_term_mem_block, typically called during
 * application shutdown
 *
 * @param[in]  p_mem : pointer memory to be freed
 *
 * @return   none
 */
void wiced_memory_free_long_term_mem_block(wiced_bt_buffer_t *p_mem);


/**
 * Get heap stats
 *
 * @param[in]       p_heap  : heap pointer (output of #wiced_bt_create_heap)
 * @param[out]      p_stats : pointer to receive the heap statistics
 *
 * @return          TRUE in case of valid stats returned in p_stats
 */
wiced_bool_t wiced_bt_get_heap_statistics(void* p_heap, wiced_bt_heap_statistics_t* p_stats);

/**
 * Get heap stats of heap at index. Application calls the function in a loop incrementing the index
 * till the function returns WICED_FALSE
 *
 * @param[in]  index  : index of heap, starts from 0
 * @param[out] p_stats : pointer to receive the heap statistics
 *
 * @return TRUE in case of valid stats returned in p_stats
 */
wiced_bool_t wiced_bt_get_heap_statistics_with_index(int index, wiced_bt_heap_statistics_t *p_stats);

/**
 * Get pool stats
 *
 * @param[in]  p_pool  : pool pointer (output of #wiced_bt_create_pool)
 * @param[out] p_stats : pointer to receive the pool statistics
 *
 * @return  TRUE in case of valid stats returned in p_stats
 */
wiced_result_t wiced_bt_get_pool_statistics(wiced_bt_pool_t *p_pool, wiced_bt_pool_statistics_t *p_stats);

/**
 * Set the exception callback
 *
 * @param[in]       pf_handler : Exception callback function
 *
 * @return          void
 */
void wiced_set_exception_callback(pf_wiced_exception pf_handler);

#ifdef __cplusplus
}
#endif
/** @} */
