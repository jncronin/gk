#include "cybt_platform_config.h"

#ifdef ENABLE_DEBUG_UART

#include "cyhal_uart.h"
#include "cyabs_rtos.h"
#include "cybsp_types.h"
#include "cybt_platform_task.h"
#include "cybt_debug_uart.h"
#include "cybt_platform_interface.h"
#include "wiced_memory.h"

#define HCI_WICED_PKT               0x19
#define BT_TASK_NAME_DEBUG_UART_TX       "CYBT_DEBUG_UART_TX_Task"
#define BT_TASK_NAME_DEBUG_UART_RX       "CYBT_DEBUG_UART_RX_Task"
#define DEBUG_UART_TX_TASK_STACK_SIZE    (0x1800)
#define DEBUG_UART_RX_TASK_STACK_SIZE    (0x1800)
#define DEBUG_UART_TX_TASK_QUEUE_COUNT   (20)
#define DEBUG_UART_TX_QUEUE_ITEM_SIZE    (sizeof(void *))
#define DEBUG_UART_TX_TASK_QUEUE         cybt_debug_uart_tx_queue
#define DEBUG_UART_TX_TASK_PRIORITY     (CY_RTOS_PRIORITY_ABOVENORMAL)
#define DEBUG_UART_RX_TASK_PRIORITY     (CY_RTOS_PRIORITY_ABOVENORMAL)

#define WICED_HDR_SZ 5
#define MAX_TRACE_DATA_LEN          1000
#define MAX_RX_DATA_LEN             1000

#define DEBUG_UART_MEMORY_SIZE      (MAX_TRACE_DATA_LEN * 6)

#define HCI_CONTROL_GROUP_DEVICE                              0x00
#define HCI_CONTROL_EVENT_WICED_TRACE                       ( ( HCI_CONTROL_GROUP_DEVICE << 8 ) | 0x02 )    /* WICED trace packet */
#define HCI_CONTROL_EVENT_HCI_TRACE                         ( ( HCI_CONTROL_GROUP_DEVICE << 8 ) | 0x03 )    /* Bluetooth protocol trace */
#define INVALID_TYPE 0xFF

wiced_bt_heap_t *debug_task_heap = NULL;

enum
{
    HEADER_PHASE = 0,
    DATA_PHASE
};

typedef struct
{
    bool            inited;
    cyhal_uart_t    hal_obj;
    cy_semaphore_t  tx_complete;
    cy_semaphore_t  rx_complete;
    cy_semaphore_t  tx_ready;
    cy_mutex_t      tx_atomic;
    bool            rx_done;
    cybt_debug_uart_data_handler_t rx_cb;
} debug_uart_cb_t;


typedef struct
{
    int opcode;
    uint16_t length;
    uint16_t type;
    uint8_t *data;
}trace_data_t;

debug_uart_cb_t cy_trans_uart;
cy_queue_t  cybt_debug_uart_tx_queue;
cy_thread_t cybt_debug_uart_tx_task;
cy_thread_t cybt_debug_uart_rx_task;


/*
 * Global variable declarations
 */
static uint8_t  wiced_rx_cmd[MAX_RX_DATA_LEN+WICED_HDR_SZ]; //RX command pool.
volatile uint32_t phase=HEADER_PHASE,data_counter=0;

cybt_result_t cybt_trans_write (uint8_t type, uint16_t opcode, uint16_t data_size, uint8_t *p_data);
static cybt_result_t cybt_handle_received_tx_data(uint16_t type, uint16_t  op,uint16_t length, uint8_t* p_data);

uint32_t cybt_get_read_request_len(void)
{
    if (phase == DATA_PHASE)
        return data_counter;

    return WICED_HDR_SZ;
}

static void *cybt_platform_debug_task_mempool_alloc(uint32_t req_size)
{
    void *p_mem_block;

    if(NULL == debug_task_heap)
    {
        return NULL;
    }

    cybt_platform_disable_irq();

    p_mem_block = (void *) wiced_bt_get_buffer_from_heap(debug_task_heap, req_size);

    cybt_platform_enable_irq();

    return p_mem_block;
}

static void cybt_platform_debug_task_mempool_free(void *p_mem_block)
{
    if(NULL == debug_task_heap)
    {
        return;
    }

    cybt_platform_disable_irq();

    wiced_bt_free_buffer((wiced_bt_buffer_t *) p_mem_block);

    cybt_platform_enable_irq();
}

static cybt_result_t cybt_handle_received_tx_data(uint16_t type, uint16_t  opcode,uint16_t length, uint8_t* p_data)
{
    trace_data_t *data = NULL;
    cy_rslt_t result = CYBT_ERR_GENERIC;
    size_t   count = 0;
    if (!debug_task_heap || (length > MAX_TRACE_DATA_LEN) )
        return CYBT_ERR_GENERIC;
    result = cy_rtos_get_semaphore(&cy_trans_uart.tx_ready, CY_RTOS_NEVER_TIMEOUT, false);

    if(CY_RSLT_SUCCESS != result)
    {
        return CYBT_ERR_GENERIC;
    }

    result = cy_rtos_count_queue(&DEBUG_UART_TX_TASK_QUEUE, &count);
    if ( (result != CY_RSLT_SUCCESS) || (count == DEBUG_UART_TX_TASK_QUEUE_COUNT))
    {
        cy_rtos_set_semaphore(&cy_trans_uart.tx_ready, false);
        return CYBT_ERR_QUEUE_FULL;
    }

    data = cybt_platform_debug_task_mempool_alloc(sizeof(trace_data_t) + length + 3);
    if (data == NULL)
    {
        cy_rtos_set_semaphore(&cy_trans_uart.tx_ready, false);
        return CYBT_ERR_OUT_OF_MEMORY;
    }

    data->opcode = opcode;
    data->length = length;
    data->type   = type;
    data->data = (uint8_t *)(data + 1);
    memcpy(data->data, p_data, length);

    result = cy_rtos_put_queue(&DEBUG_UART_TX_TASK_QUEUE, (void *) &data, 0, false);

    if(CY_RSLT_SUCCESS != result)
    {
        cybt_platform_debug_task_mempool_free((void *)data);
    }
    cy_rtos_set_semaphore(&cy_trans_uart.tx_ready, false);
    return CYBT_SUCCESS;
}

static void cybt_debug_rx_task(void *arg)
{
    cy_rslt_t result;
    volatile uint32_t numAvailable;
    volatile size_t expectedlength = 0;
    volatile uint32_t data_index = 0;
    while(1)
    {
        result = cy_rtos_get_semaphore(&cy_trans_uart.rx_complete, CY_RTOS_NEVER_TIMEOUT, false);

        if (result != CY_RSLT_SUCCESS)
        {
            continue;
        }
        numAvailable = 0;
        expectedlength = cybt_get_read_request_len();
        if (!cy_trans_uart.rx_done)
        {

            numAvailable = cyhal_uart_readable(&cy_trans_uart.hal_obj);
            if (numAvailable >= expectedlength)
            {
                cyhal_uart_read(&cy_trans_uart.hal_obj, wiced_rx_cmd + data_index, (size_t *)&expectedlength);
                numAvailable -= expectedlength;
            }
            else
            {
                cyhal_uart_enable_event(&cy_trans_uart.hal_obj, CYHAL_UART_IRQ_RX_DONE, CYHAL_ISR_PRIORITY_DEFAULT, true);
                cyhal_uart_read_async(&cy_trans_uart.hal_obj, wiced_rx_cmd + data_index, expectedlength);
                continue;
            }
        }


        switch(phase)
        {
        case HEADER_PHASE:
            if(wiced_rx_cmd[0] != HCI_WICED_PKT)
            {
                data_index=0x0;
                break;
            }
            data_counter = ( wiced_rx_cmd[3] | (uint32_t)(wiced_rx_cmd[4])<<8);
            data_index += WICED_HDR_SZ;
            phase = DATA_PHASE;
            break;
        case DATA_PHASE:
            data_counter -= expectedlength;
            data_index += expectedlength;
            break;
        }
        if(data_counter==0 && (cy_trans_uart.rx_cb != NULL))
        {
            phase = HEADER_PHASE;
            cy_trans_uart.rx_cb(wiced_rx_cmd+1, data_index-1);
            data_index = 0;
        }
        cy_trans_uart.rx_done = false;
        if (numAvailable)
        {
            // re-enter the loop if data is available
            cy_rtos_set_semaphore(&cy_trans_uart.rx_complete, true);
            continue;
        }
        cyhal_uart_enable_event(&cy_trans_uart.hal_obj, CYHAL_UART_IRQ_RX_NOT_EMPTY, CYHAL_ISR_PRIORITY_DEFAULT, true);
    }
}

static void cybt_debug_tx_task(void *arg)
{
    cy_rslt_t result;
    trace_data_t *data = NULL;
    while(1)
    {
        data = NULL;
        result = cy_rtos_get_queue(&DEBUG_UART_TX_TASK_QUEUE,
                                   (void *)&data,
                                   CY_RTOS_NEVER_TIMEOUT,
                                   false
                                  );

        if(CY_RSLT_SUCCESS != result || NULL == data)
        {
            continue;
        }

        cybt_trans_write(data->type,data->opcode, (uint32_t)data->length, data->data);
        cybt_platform_debug_task_mempool_free(data);
    }
}

cybt_result_t cybt_init_debug_trans_task(void)
{
    cy_rslt_t result;
    void *p_heap_mem = NULL;

    result = cy_rtos_init_queue(&DEBUG_UART_TX_TASK_QUEUE,
                       DEBUG_UART_TX_TASK_QUEUE_COUNT,
                       DEBUG_UART_TX_QUEUE_ITEM_SIZE
                       );
    if (result != CY_RSLT_SUCCESS)
        return CYBT_ERR_INIT_QUEUE_FAILED;

    result = cy_rtos_create_thread(&cybt_debug_uart_tx_task,
                          cybt_debug_tx_task,
                          BT_TASK_NAME_DEBUG_UART_TX,
                          NULL,
                          DEBUG_UART_TX_TASK_STACK_SIZE,
                          DEBUG_UART_TX_TASK_PRIORITY,
                          (cy_thread_arg_t) NULL
                         );
    if (result != CY_RSLT_SUCCESS)
        return CYBT_ERR_CREATE_TASK_FAILED;

    result = cy_rtos_create_thread(&cybt_debug_uart_rx_task,
                              cybt_debug_rx_task,
                              BT_TASK_NAME_DEBUG_UART_RX,
                              NULL,
                              DEBUG_UART_RX_TASK_STACK_SIZE,
                              DEBUG_UART_RX_TASK_PRIORITY,
                              (cy_thread_arg_t) NULL
                             );

    if (result != CY_RSLT_SUCCESS)
        return CYBT_ERR_CREATE_TASK_FAILED;

    p_heap_mem = (wiced_bt_heap_t *)cybt_platform_malloc(DEBUG_UART_MEMORY_SIZE);
    if (p_heap_mem == NULL)
        return CYBT_ERR_OUT_OF_MEMORY;

    debug_task_heap = wiced_bt_create_heap("CYBT_DEBUG_TASK_POOL",
                                        p_heap_mem,
                                        DEBUG_UART_MEMORY_SIZE,
                                        NULL,
                                        FALSE
                                       );
    return CYBT_SUCCESS;
}

static void cybt_uart_tx_irq(void)
{
    cy_rtos_set_semaphore(&cy_trans_uart.tx_complete, true);
}

static void cybt_uart_irq_handler_(void *handler_arg, cyhal_uart_event_t event)
{
    switch(event)
    {
        case CYHAL_UART_IRQ_RX_NOT_EMPTY:
            cyhal_uart_enable_event(&cy_trans_uart.hal_obj, CYHAL_UART_IRQ_RX_NOT_EMPTY, CYHAL_ISR_PRIORITY_DEFAULT, false);
            cy_rtos_set_semaphore(&cy_trans_uart.rx_complete, true);
            break;
        case CYHAL_UART_IRQ_RX_DONE:
            cyhal_uart_enable_event(&cy_trans_uart.hal_obj, CYHAL_UART_IRQ_RX_DONE, CYHAL_ISR_PRIORITY_DEFAULT, false);
            cy_rtos_set_semaphore(&cy_trans_uart.rx_complete, true);
            cy_trans_uart.rx_done = true;
            break;
        case CYHAL_UART_IRQ_TX_DONE:
            cybt_uart_tx_irq();
            break;
        default:
            break;
    }
}

cybt_result_t cybt_debug_uart_init(cybt_debug_uart_config_t *config, cybt_debug_uart_data_handler_t p_data_handler)
{
    const cyhal_uart_cfg_t uart_config =
    {
        .data_bits = 8,
        .stop_bits = 1,
        .parity = CYHAL_UART_PARITY_NONE,
        .rx_buffer = NULL,
        .rx_buffer_size = 0,
    };
    uint16_t enable_irq_event = (CYHAL_UART_IRQ_TX_DONE
                                           | CYHAL_UART_IRQ_RX_NOT_EMPTY
                                          );
    if (!config)
    {
        return CYBT_ERR_BADARG;
    }

    memset(&cy_trans_uart, 0, sizeof(debug_uart_cb_t));
#if (CYHAL_API_VERSION >= 2)
    cy_rslt_t result = cyhal_uart_init(&cy_trans_uart.hal_obj,
                                       config->uart_tx_pin,
                                       config->uart_rx_pin,
                                       config->uart_cts_pin,
                                       config->uart_rts_pin,
                                       NULL,
                                       &uart_config
                                      );
#else
    cy_rslt_t result = cyhal_uart_init(&cy_trans_uart.hal_obj, config->uart_tx_pin, config->uart_rx_pin, NULL, &uart_config);
#endif

    if (result == CY_RSLT_SUCCESS)
    {
        result = cyhal_uart_set_baud(&cy_trans_uart.hal_obj, config->baud_rate, NULL);
        if (result == CY_RSLT_SUCCESS)
        {
            if (config->flow_control)
            {
            #if (CYHAL_API_VERSION >= 2)
                result = cyhal_uart_enable_flow_control(&cy_trans_uart.hal_obj, true, true);
            #else
                result = cyhal_uart_set_flow_control(&cy_trans_uart.hal_obj, config->uart_cts_pin, config->uart_rts_pin);
            #endif
            }
            if (result == CY_RSLT_SUCCESS)
            {
                cy_rtos_init_semaphore(&cy_trans_uart.tx_complete,
                                       1,
                                       0
                                      );
                cy_rtos_init_semaphore(&cy_trans_uart.rx_complete,
                                       1,
                                       0
                                      );

                cy_rtos_init_semaphore(&cy_trans_uart.tx_ready,
                                                       1,
                                                       1
                                                      );

                cy_rtos_init_mutex(&cy_trans_uart.tx_atomic);

                cyhal_uart_register_callback(&cy_trans_uart.hal_obj,
                                             cybt_uart_irq_handler_,
                                             NULL
                                            );

                cyhal_uart_enable_event(&cy_trans_uart.hal_obj,
                                        (cyhal_uart_event_t)enable_irq_event,
                                        CYHAL_ISR_PRIORITY_DEFAULT,
                                        true
                                       );
                cy_trans_uart.inited = true;
                cy_trans_uart.rx_done = false;
                cy_trans_uart.rx_cb = p_data_handler;
                cybt_init_debug_trans_task();
                return CYBT_SUCCESS;
            }
        }
    }
    return CYBT_ERR_HCI_INIT_FAILED;
}

cybt_result_t cybt_debug_uart_send_trace(uint16_t length, uint8_t* p_data)
{
    return cybt_handle_received_tx_data(INVALID_TYPE, HCI_CONTROL_EVENT_WICED_TRACE, length, p_data);
}

cybt_result_t cybt_debug_uart_send_data (uint16_t opcode, uint16_t data_size, uint8_t *p_data)
{
    return cybt_trans_write(INVALID_TYPE,(uint16_t)opcode, data_size, p_data);
}

cybt_result_t cybt_debug_uart_send_hci_trace (uint8_t type, uint16_t data_size, uint8_t *p_data)
{
    return cybt_handle_received_tx_data((uint16_t)type, HCI_CONTROL_EVENT_HCI_TRACE, data_size, p_data);
}

cybt_result_t cybt_trans_write (uint8_t type, uint16_t op, uint16_t data_size, uint8_t *p_data)
{
    cybt_result_t result = CYBT_ERR_GENERIC;
    cy_rslt_t status = CY_RSLT_SUCCESS;

    uint8_t data[1000];
    size_t index = 0;
    uint8_t opcode = (uint8_t)(op&0xff);
    uint8_t group_code = (uint8_t)((op >> 8)&0xff);

    if (cy_trans_uart.inited == false)
        return CYBT_ERR_GENERIC;

    status = cy_rtos_get_mutex(&cy_trans_uart.tx_atomic, CY_RTOS_NEVER_TIMEOUT);

    if(CY_RSLT_SUCCESS != status)
    {
        return result;
    }

    data[index++] = HCI_WICED_PKT;

    if ( (type != 0xFF) || ((group_code == 0x00) && (opcode == 0x03)) )
    {
        uint16_t new_size = (data_size+1);
        data[index++] = 0x03;
        data[index++] = 0x00;
        data[index++] = (uint8_t)(new_size&0xff);
        data[index++] = (uint8_t)((new_size >> 8)&0xff);
        data[index++] = type;
    }
    else
    {
        data[index++] = opcode;
        data[index++] = group_code;
        data[index++] = (uint8_t)(data_size&0xff);
        data[index++] = (uint8_t)((data_size >> 8)&0xff);
    }
    memcpy(&data[index], p_data, data_size);
    index += data_size;

    status = cyhal_uart_write_async(&cy_trans_uart.hal_obj,
            (void *) data,
            (size_t) index
            );
    if(CY_RSLT_SUCCESS == status)
    {
        cy_rtos_get_semaphore(&cy_trans_uart.tx_complete, CY_RTOS_NEVER_TIMEOUT, false);
        result = CYBT_SUCCESS;
    }

    cy_rtos_set_mutex(&cy_trans_uart.tx_atomic);
    return result;
}

int _write(int fd, const char* ptr, int len)
{
    if ( cybt_debug_uart_send_trace(len,(uint8_t* )ptr) == CYBT_SUCCESS)
    {
        return len;
    }
    return 0;
}

#endif // ENABLE_DEBUG_UART
