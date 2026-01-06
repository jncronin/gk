/*******************************************************************************
* \file cybt_platform_task.c
*
* \brief
* Provides functions for OS task communication.
*
********************************************************************************
* \copyright
* Copyright 2018-2021 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cyabs_rtos.h"

#include "cybt_platform_config.h"
#include "cybt_platform_task.h"
#include "cybt_platform_interface.h"
#include "cybt_platform_util.h"

#include "cybt_platform_trace.h"

#include "wiced_memory.h"

/*****************************************************************************
 *                                Constants
 *****************************************************************************/
#define CYBT_RX_MEM_MIN_SIZE         (1040)
#define CYBT_TX_CMD_MEM_MIN_SIZE     (264)
#define CYBT_TX_HEAP_MIN_SIZE        (1040)

#define CYBT_TASK_MINIMUM_POOL_SIZE  (CYBT_RX_MEM_MIN_SIZE \
                                      + CYBT_TX_CMD_MEM_MIN_SIZE \
                                      + CYBT_TX_HEAP_MIN_SIZE \
                                     )

/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/
typedef struct
{
    uint8_t         *p_rx_mem;
    uint32_t        rx_mem_size;
    uint8_t         *p_tx_cmd_mem;
    uint32_t        tx_cmd_mem_size;
    wiced_bt_heap_t *p_tx_data_heap;
    uint32_t        tx_data_heap_size;
} cybt_task_mem_cb_t;

/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
cy_thread_t cybt_task[BT_TASK_NUM] = {0};
cy_queue_t  cybt_task_queue[BT_TASK_NUM] = {0};

cybt_task_mem_cb_t  task_mem_cb = {0};


/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
cybt_result_t cybt_platform_task_init(void *p_arg)
{
    extern void cybt_hci_tx_task(cy_thread_arg_t arg);
    extern void cybt_hci_rx_task(cy_thread_arg_t arg);

    cy_rslt_t cy_result;
    cybt_result_t task_result;
    UNUSED_VARIABLE(p_arg);
    const cybt_platform_config_t *p_bt_platform_cfg = cybt_platform_get_config();

    uint32_t total_mem_pool_size =
        (p_bt_platform_cfg->task_mem_pool_size > CYBT_TASK_MINIMUM_POOL_SIZE)?
        p_bt_platform_cfg->task_mem_pool_size: CYBT_TASK_MINIMUM_POOL_SIZE;

    task_result = cybt_platform_task_mempool_init(total_mem_pool_size);
    if(CYBT_SUCCESS != task_result)
    {
        MAIN_TRACE_ERROR("task_init(): Create mempool failed (0x%x)",
                         task_result
                        );
        return CYBT_ERR_INIT_MEMPOOL_FAILED;
    }

    cy_result = cy_rtos_init_queue(&HCI_TX_TASK_QUEUE,
                                   HCI_TX_TASK_QUEUE_COUNT,
                                   HCI_TX_TASK_QUEUE_ITEM_SIZE
                                  );
    if(CY_RSLT_SUCCESS != cy_result)
    {
        MAIN_TRACE_ERROR("task_init(): Init hci_tx task queue failed (0x%x)",
                         cy_result
                        );
        return CYBT_ERR_INIT_QUEUE_FAILED;
    }

    cy_result = cy_rtos_init_queue(&HCI_RX_TASK_QUEUE,
                                   HCI_RX_TASK_QUEUE_COUNT,
                                   HCI_RX_TASK_QUEUE_ITEM_SIZE
                                  );
    if(CY_RSLT_SUCCESS != cy_result)
    {
        MAIN_TRACE_ERROR("task_init(): Init hci_rx task queue failed (0x%x)",
                         cy_result
                        );
        return CYBT_ERR_INIT_QUEUE_FAILED;
    }

    cy_result = cy_rtos_create_thread(&cybt_task[BT_TASK_ID_HCI_TX],
                                      cybt_hci_tx_task,
                                      BT_TASK_NAME_HCI_TX,
                                      NULL,
                                      HCI_TX_TASK_STACK_SIZE,
                                      HCI_TX_TASK_PRIORITY,
                                      (cy_thread_arg_t) NULL
                                     );
    if(CY_RSLT_SUCCESS != cy_result)
    {
        MAIN_TRACE_ERROR("task_init(): Create hci_tx task failed (0x%x)",
                         cy_result
                        );
        return CYBT_ERR_CREATE_TASK_FAILED;
    }

    cy_result = cy_rtos_create_thread(&cybt_task[BT_TASK_ID_HCI_RX],
                                      cybt_hci_rx_task,
                                      BT_TASK_NAME_HCI_RX,
                                      NULL,
                                      HCI_RX_TASK_STACK_SIZE,
                                      HCI_RX_TASK_PRIORITY,
                                      (cy_thread_arg_t) NULL
                                     );
    if(CY_RSLT_SUCCESS != cy_result)
    {
        MAIN_TRACE_ERROR("task_init(): Create hci_rx task failed (0x%x)",
                         cy_result
                        );
        return CYBT_ERR_CREATE_TASK_FAILED;
    }

    return CYBT_SUCCESS;
}

cybt_result_t cybt_platform_task_deinit(void)
{
    cybt_result_t result;

    MAIN_TRACE_DEBUG("cybt_platform_task_deinit()");

    result = cybt_send_msg_to_hci_rx_task(BT_IND_TASK_SHUTDOWN, false);
    if(CYBT_SUCCESS != result)
    {
        MAIN_TRACE_ERROR("task_deinit(): Failed to shutdown HCI_RX task");
    }

    result = cybt_send_msg_to_hci_tx_task((BT_MSG_HDR *)BT_IND_TASK_SHUTDOWN, false);
    if(CYBT_SUCCESS != result)
    {
        MAIN_TRACE_ERROR("task_deinit(): Failed to shutdown HCI_TX task");
    }

    cybt_platform_task_mempool_deinit();

    return CYBT_SUCCESS;
}

cybt_result_t cybt_platform_task_mempool_init(uint32_t total_size)
{
    task_mem_cb.rx_mem_size = CYBT_RX_MEM_MIN_SIZE;
    task_mem_cb.tx_cmd_mem_size = CYBT_TX_CMD_MEM_MIN_SIZE;
    task_mem_cb.tx_data_heap_size = total_size - CYBT_RX_MEM_MIN_SIZE - CYBT_TX_CMD_MEM_MIN_SIZE;

    task_mem_cb.p_tx_cmd_mem = (uint8_t *) cybt_platform_malloc(task_mem_cb.tx_cmd_mem_size);
    task_mem_cb.p_rx_mem = (uint8_t *) cybt_platform_malloc(task_mem_cb.rx_mem_size);
    task_mem_cb.p_tx_data_heap = (wiced_bt_heap_t *)cybt_platform_malloc(task_mem_cb.tx_data_heap_size);

    if((NULL == task_mem_cb.p_tx_cmd_mem)
       || (NULL == task_mem_cb.p_rx_mem)
       || (NULL == task_mem_cb.p_tx_data_heap)
      )
    {
        MEM_TRACE_ERROR("task_mempool_init(): init failed, tx_cmd = 0x%x, tx_data = 0x%x, rx_mem = 0x%x",
                        task_mem_cb.p_tx_cmd_mem,
                        task_mem_cb.p_tx_data_heap,
                        task_mem_cb.p_rx_mem
                       );
        cybt_platform_task_mempool_deinit();

        return CYBT_ERR_OUT_OF_MEMORY;
    }

    task_mem_cb.p_tx_data_heap = wiced_bt_create_heap("CYBT_TASK_TX_POOL",
                                                      task_mem_cb.p_tx_data_heap,
                                                      task_mem_cb.tx_data_heap_size,
                                                      NULL,
                                                      WICED_FALSE
                                                     );

    return CYBT_SUCCESS;
}

void *cybt_platform_task_tx_mempool_alloc(uint32_t req_size)
{
    void *p_mem_block;

    if(NULL == task_mem_cb.p_tx_data_heap)
    {
        MEM_TRACE_ERROR("task_tx_memory_alloc(): Invalid Heap");
        return NULL;
    }

    cybt_platform_disable_irq();

    p_mem_block = (void *) wiced_bt_get_buffer_from_heap(task_mem_cb.p_tx_data_heap,
                                                         req_size
                                                        );

    cybt_platform_enable_irq();

    return p_mem_block;
}

void *cybt_platform_task_get_tx_cmd_mem(void)
{
    return (void *) task_mem_cb.p_tx_cmd_mem;
}

void *cybt_platform_task_get_rx_mem(void)
{
    return (void *) task_mem_cb.p_rx_mem;
}

void cybt_platform_task_mempool_free(void *p_mem_block)
{
    cybt_platform_disable_irq();

    wiced_bt_free_buffer((wiced_bt_buffer_t *) p_mem_block);

    cybt_platform_enable_irq();
}

void cybt_platform_task_mempool_deinit(void)
{
    MEM_TRACE_DEBUG("task_mempool_deinit()");

    if(task_mem_cb.p_rx_mem)
    {
        cybt_platform_free(task_mem_cb.p_rx_mem);
    }

    if(task_mem_cb.p_tx_data_heap)
    {
        wiced_bt_delete_heap(task_mem_cb.p_tx_data_heap);
        cybt_platform_free(task_mem_cb.p_tx_data_heap);
    }

    if(task_mem_cb.p_tx_cmd_mem)
    {
        cybt_platform_free(task_mem_cb.p_tx_cmd_mem);
    }

    memset(&task_mem_cb, 0, sizeof(cybt_task_mem_cb_t));
}

uint8_t cybt_platform_task_get_queue_utilization(uint8_t task_id)
{
    size_t     item_cnt_in_queue = 0;
    cy_rslt_t  result;
    uint16_t   queue_total_cnt = 0;

    switch(task_id)
    {
        case BT_TASK_ID_HCI_RX:
            queue_total_cnt = HCI_RX_TASK_QUEUE_COUNT;
            break;
        case BT_TASK_ID_HCI_TX:
            queue_total_cnt = HCI_TX_TASK_QUEUE_COUNT;
            break;
        default:
            MAIN_TRACE_ERROR("task_queue_utilization(): unknown task (%d)", task_id);
            return CYBT_INVALID_QUEUE_UTILIZATION;
    }

    result = cy_rtos_count_queue(&cybt_task_queue[task_id], &item_cnt_in_queue);
    if(CY_RSLT_SUCCESS != result)
    {
        MAIN_TRACE_ERROR("task_queue_utilization(): get count failed (0x%x)", result);
        return CYBT_INVALID_QUEUE_UTILIZATION;
    }

    return (item_cnt_in_queue * 100 / queue_total_cnt);
}

uint8_t cybt_platform_task_get_heap_utilization(wiced_bt_heap_t * p_heap,
                                                                    uint16_t        *p_largest_free_size
                                                                  )
{
    wiced_bool_t result;
    wiced_bt_heap_statistics_t heap_stat = {0};

    if(NULL == p_heap)
    {
        return CYBT_INVALID_HEAP_UTILIZATION;
    }

    result = wiced_bt_get_heap_statistics((void *) p_heap, &heap_stat);
    if(WICED_FALSE == result)
    {
        return CYBT_INVALID_HEAP_UTILIZATION;
    }

    if(NULL != p_largest_free_size)
    {
        *p_largest_free_size = heap_stat.current_largest_free_size;
    }

    return (heap_stat.current_size_allocated * 100 / heap_stat.heap_size);
}

uint8_t cybt_platform_task_get_tx_heap_utilization(uint16_t                      *p_largest_free_size)
{
    return cybt_platform_task_get_heap_utilization(task_mem_cb.p_tx_data_heap, p_largest_free_size);
}

void cybt_platform_terminate_hci_tx_thread(void)
{
    cy_rslt_t cy_result;

    cy_result = cy_rtos_join_thread(&cybt_task[BT_TASK_ID_HCI_TX]);
    if(CY_RSLT_SUCCESS != cy_result)
    {
        MAIN_TRACE_ERROR("terminate HCI_TX thread failed 0x%x\n", cy_result);
    }

}

void cybt_platform_terminate_hci_rx_thread(void)
{
    cy_rslt_t cy_result;

    cy_result = cy_rtos_join_thread(&cybt_task[BT_TASK_ID_HCI_RX]);
    if(CY_RSLT_SUCCESS != cy_result)
    {
        MAIN_TRACE_ERROR("terminate HCI_RX thread failed 0x%x\n", cy_result);
    }
}

