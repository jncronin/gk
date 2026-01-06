/***************************************************************************//**
* \file cybt_platform_freertos.c
*
* \brief
* Implementation for BT porting interface on FreeRTOS
*
********************************************************************************
* \copyright
* Copyright 2018-2019 Cypress Semiconductor Corporation
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
#include <string.h>
#include "cybt_platform_task.h"
#include "cybt_platform_interface.h"
#include "cybt_platform_trace.h"
#include "PSoC6IPC.h"
#include "wiced_bt_cfg.h"

/******************************************************************************
 *                                Constants
 ******************************************************************************/
#define HCI_HEADER_LEN 0x04
#define L2C_HEADER_LEN 0x04
#define BT_TASK_EVENT_BUFFERS_CNT 10

#define MAX_WRITE_RETRIES       (10)
#define RECORD_IPC_STATS

/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/
typedef struct 
{
    bool            inited;
    cy_mutex_t      tx_atomic;
    cy_mutex_t      rx_atomic;
} hci_interface_t;

typedef struct
{
    uint16_t pkt_type;
    uint8_t *data;
    uint16_t pkt_len;
}BT_HCI_MSG;

typedef struct
{
    uint16_t event_id;
    BT_HCI_MSG msg;
}bt_task_event_t;

typedef struct
{
    uint8_t  clientID;
    uint8_t  pktType;
    uint16_t intrRelMask;
    bool     controllerStarted;
    uint32_t data;
    uint16_t dataLen;
} cy_stc_ble_ipc_msg_t;

#ifdef RECORD_IPC_STATS
typedef struct {
    uint32_t write_fail_count; /* Number of write requests that failed. This does not count number of retries performed for same request.  */
    uint32_t write_max_retry_count; /* Maximum number of failed retries permormed for write request. 0 <= MAX_WRITE_RETRIES */
}cybt_ipc_stats_t;

#endif

/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
static hci_interface_t   hci_cb = {.inited = false};
static bt_task_event_t timer_msg = { .event_id = BT_EVT_TIMEOUT };
static bt_task_event_t hci_boot_msg = { .event_id = BT_EVT_TASK_BOOT_COMPLETES };
static bt_task_event_t shutdown_msg = { .event_id = BT_EVT_TASK_SHUTDOWN };
static cy_event_t hci_task_event;
#define HCI_TASK_EVENT 0x1

ipc_host_msg_buffer ipc_host_buffer[IPC_NUM_BUFFERS];
uint16_t ipc_avail_pos = 0, ipc_allotted = 0;
static bt_task_event_t event_buffers[BT_TASK_EVENT_BUFFERS_CNT];
static uint8_t available_pos = 0, allotted_buffs = 0;
static bool is_shutdown_progress = false;

static volatile cy_stc_ble_ipc_msg_t controllerMsg =
{
    /* clientID          */  3u,
    /* pktType           */  0x01u,
    /* intrRelMask       */  0x00u,
    /* controllerStarted */  false,
    /* data              */  0x00u,
    /* dataLen           */  0x00u
};

static IPC_HOST_MSG msg_comp;

#ifdef RECORD_IPC_STATS
static cybt_ipc_stats_t ipc_stats;
#endif

/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
extern cybt_result_t cybt_platform_wake_gpios_on_hci_open(void);
extern void cybt_platform_wake_gpios_on_hci_close(void);
extern void cybt_platform_assert_bt_wake(void);
extern void cybt_platform_deassert_bt_wake(void);
extern cybt_result_t cybt_platform_msg_to_bt_task(const uint16_t msg, bool is_from_isr);

/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
 
uint8_t *cybt_platform_hci_get_buffer(hci_packet_type_t pti, uint32_t size)
{
    uint8_t *p_txbuffer = NULL;
	IPC_HOST_MSG *host_msg;

    if (ipc_allotted == IPC_NUM_BUFFERS)
    {
        HCIDRV_TRACE_ERROR("no ipc buffers available");
        return NULL;
    }

    host_msg = &ipc_host_buffer[ipc_avail_pos].host_msg;
    p_txbuffer = ipc_host_buffer[ipc_avail_pos].ipc_tx_buffer;
    memset(host_msg, 0, sizeof(IPC_HOST_MSG));
    memset(p_txbuffer, 0, IPC_TX_BUFFER_SIZE);
    ipc_avail_pos = (ipc_avail_pos + 1) % IPC_NUM_BUFFERS;
    ipc_allotted++;

    return (p_txbuffer);
}

cybt_result_t cybt_platform_msg_to_bt_task(const uint16_t msg, bool is_from_isr)
{
    cybt_result_t result;
    switch(msg)
    {
        case BT_EVT_TIMEOUT:
            result = cybt_send_msg_to_bt_task(&timer_msg, is_from_isr);
            break;
        case BT_EVT_TASK_BOOT_COMPLETES:
            result = cybt_send_msg_to_bt_task(&hci_boot_msg, is_from_isr);
            break;
        case BT_EVT_TASK_SHUTDOWN:
            result = cybt_send_msg_to_bt_task(&shutdown_msg, is_from_isr);
            break;
        default:
            result = CYBT_ERR_BADARG;
            break;
    }
    return (result);
}

void cybt_platform_ipc_pipe_release_cb(void)
{
    HCIDRV_TRACE_DEBUG("cybt_platform_ipc_pipe_release_cb");
}

void HciIpcFreePktBuffer(unsigned char type, uint32_t bufferPtr, uint16_t length)
{
    memset (&msg_comp, 0x00, sizeof(msg_comp));
    msg_comp.clientID = CY_BLE_CYPIPE_MSG_COMPLETE_ID;
    msg_comp.pktType = type;
    msg_comp.pktLen = length;
    msg_comp.pktDataPointer = bufferPtr;

    Cy_IPC_Pipe_SendMessage(CY_BLE_IPC_CONTROLLER_ADDR,CY_BLE_IPC_HOST_ADDR, (void *)&msg_comp,
                            NULL);
}

bt_task_event_t *get_rx_free_event_buffer()
{
    if (allotted_buffs == BT_TASK_EVENT_BUFFERS_CNT)
    {
        return NULL;
    }
    bt_task_event_t *evt = &event_buffers[available_pos];
    available_pos = (available_pos + 1)%BT_TASK_EVENT_BUFFERS_CNT;
    allotted_buffs++;
    return evt;
}

cybt_result_t cybt_platform_hci_read(void *event,hci_packet_type_t *pti,
                                     uint8_t           *p_data,
                                     uint32_t          *p_length
                                     )
{

    cybt_result_t res = CYBT_SUCCESS;
    bt_task_event_t *evt = (bt_task_event_t*) event;
    if (evt == NULL)
    {
        allotted_buffs--;
        return CYBT_ERR_BADARG;
    }

    *pti = (hci_packet_type_t)evt->msg.pkt_type;
    memcpy(p_data, evt->msg.data, evt->msg.pkt_len);
    *p_length = evt->msg.pkt_len;

    HciIpcFreePktBuffer(evt->msg.pkt_type, (uint32_t)evt->msg.data, evt->msg.pkt_len);
    allotted_buffs--;
    return res;
}

static void Cy_BLE_IPC_HostMsgRecvCallBack(uint32_t *msgPtr)
{
    IPC_CTRL_MSG *msg = NULL;

    //HCIDRV_TRACE_DEBUG("HostMsgRecvCallBack");
    if(msgPtr == NULL)
    {
        /* Call the BLE Stack IPC handler */
        return;
    }
    msg = (IPC_CTRL_MSG *)msgPtr;
    bt_task_event_t *task_event = get_rx_free_event_buffer();

    if (task_event == NULL)
    {
        //TODO: Post msg to controller to free memory
        return;
    }

	if (is_shutdown_progress)
	{
		task_event->event_id = BT_EVT_TASK_RESET_COMPLETE;
		is_shutdown_progress = false;
		HciIpcFreePktBuffer(msg->pktType, msg->pktDataPtr, msg->pktDataLen);
		allotted_buffs--;
	}
	else
	{
	    task_event->event_id = BT_EVT_CORE_HCI;
	    task_event->msg.data = (uint8_t *)msg->pktDataPtr;
	    task_event->msg.pkt_len = msg->pktDataLen;
	    task_event->msg.pkt_type = msg->pktType;
	}

    cybt_send_msg_to_bt_task(task_event, true);
}
static void Cy_BLE_IPC_HostMsgFlushRecvCallBack(uint32_t *msgPtr)
{
    ipc_allotted--;
	cy_rtos_setbits_event(&hci_task_event, HCI_TASK_EVENT, true);
    /*Free HOST memory if any allocated and send to controller*/
}

cybt_result_t cybt_platform_hci_open(void *p_arg)
{
    uint32_t ipcStatus;
    uint32_t rTimeout = 2000u;
    wiced_bt_cfg_settings_t * p_bt_cfg_settings_Bless = (wiced_bt_cfg_settings_t*)p_arg ;
    if (hci_cb.inited == false)
    {

    	stackParam.maxConnCount = p_bt_cfg_settings_Bless->p_ble_cfg->ble_max_simultaneous_links;
    	stackParam.controllerTotalHeapSz = (stackParam.maxConnCount *1532) + 3476 ;
    	stackParam.totalHeapSz = stackParam.controllerTotalHeapSz;
    	if(stackParam.controllerMemoryHeapPtr == NULL)
    	{
    		stackParam.controllerMemoryHeapPtr = (uint8_t *)cybt_platform_malloc(stackParam.controllerTotalHeapSz);
    	}
    	controllerMsg.data = (uint32_t) &stackParam;

    	do
    	{
    		ipcStatus = Cy_IPC_Pipe_SendMessage(CY_IPC_EP_CYPIPE_CM0_ADDR, CY_IPC_EP_CYPIPE_CM4_ADDR,
    				(void *)&controllerMsg, NULL);
    		Cy_SysLib_DelayUs(1u);
    		rTimeout--;

    	}while((ipcStatus != CY_IPC_PIPE_SUCCESS) && (rTimeout != 0u));

    	rTimeout = 2000u;
    	do
    	{
    		Cy_SysLib_Delay(1u);
    		rTimeout--;
    	} while ((!controllerMsg.controllerStarted) && (rTimeout != 0u));


    	HCIDRV_TRACE_DEBUG("Controller started %d result %s",
    			controllerMsg.controllerStarted, (controllerMsg.data==0)?"SUCCESS":"FAIL");

    	Cy_IPC_Pipe_RegisterCallback	(CY_IPC_EP_CYPIPE_CM4_ADDR,
    			&Cy_BLE_IPC_HostMsgRecvCallBack,CY_BLE_CYPIPE_MSG_SEND_ID);
    	Cy_IPC_Pipe_RegisterCallback	(CY_IPC_EP_CYPIPE_CM4_ADDR,
    			&Cy_BLE_IPC_HostMsgFlushRecvCallBack,CY_BLE_CYPIPE_MSG_COMPLETE_ID);

    	hci_cb.inited = true;
    	cy_rtos_init_event(&hci_task_event);
    }

    HCIDRV_TRACE_DEBUG("hci_open(): Done");

    cybt_platform_msg_to_bt_task(BT_EVT_TASK_BOOT_COMPLETES, false);

    return  CYBT_SUCCESS;
}

/* cybt_platform_hci_write - before calling this api, always make sure to 
 * call cybt_platform_hci_get_buffer to get the IPC buffer. 
 * Only the obtained IPC buffer can be used with cybt_platform_hci_write.
 */
cybt_result_t cybt_platform_hci_write(hci_packet_type_t pti,
                                      uint8_t *p_data,
                                      uint32_t length
                                      )
{
    cy_en_ipc_pipe_status_t result = CY_IPC_PIPE_ERROR_NO_IPC;
    cybt_result_t status = CYBT_SUCCESS;
    IPC_HOST_MSG *host_msg;
    uint8_t* ipc_tx_buffer;
    uint32_t wait_for = HCI_TASK_EVENT;
    int retries = 0;
    host_msg = (IPC_HOST_MSG *)(p_data-sizeof(IPC_HOST_MSG));
    ipc_tx_buffer = p_data;

    HCIDRV_TRACE_DEBUG("hci_write for len = %d", length);

    host_msg->clientID = CY_BLE_CYPIPE_MSG_SEND_ID;
    host_msg->pktLen = length;
    host_msg->pktType = pti;

    if (pti == HCI_PACKET_TYPE_ACL)
    {
        memcpy(&(host_msg->hciHeader), p_data, HCI_HEADER_LEN);
        memcpy(&(host_msg->pktDataHeader), (p_data+HCI_HEADER_LEN), L2C_HEADER_LEN);
        host_msg->pktDataPointer = (uint32_t)&ipc_tx_buffer[HCI_HEADER_LEN+L2C_HEADER_LEN];
    }
    else
    {
        host_msg->pktDataPointer = (uint32_t)ipc_tx_buffer;
    }

    do
    {
        result = Cy_IPC_Pipe_SendMessage(CY_BLE_IPC_CONTROLLER_ADDR,CY_BLE_IPC_HOST_ADDR, (uint32_t *)host_msg,
        		&cybt_platform_ipc_pipe_release_cb);

        if (result)
        {
            /* adding a delay before retrying */
            {
                volatile int delay = 1000;
                while(delay--);
            }

            status = CYBT_ERR_HCI_WRITE_FAILED;

#ifdef RECORD_IPC_STATS
			if (!retries)
				++ipc_stats.write_fail_count;

			if (retries > ipc_stats.write_max_retry_count){
				ipc_stats.write_max_retry_count = retries;
            }
#endif
        }else{
            status = CYBT_SUCCESS;
            break;
        }
    }while(++retries < MAX_WRITE_RETRIES);

    //TODO: Instead of blocking wait, manage this by buffer queue or a new thread

    if (status == CYBT_SUCCESS)
        cy_rtos_waitbits_event(&hci_task_event, &wait_for, true, false, CY_RTOS_NEVER_TIMEOUT);

    return status;
}

uint16_t cybt_platfrom_hci_get_rx_fifo_count(void)
{
    return 1;
}

cybt_result_t cybt_platform_hci_close(void)
{
    cybt_result_t status = CYBT_SUCCESS;
	uint8_t hci_reset_cmd[3] = {0x03, 0x0c, 0x00};	
	uint8_t *ptr = NULL;

    if(false == hci_cb.inited)
    {
        HCIDRV_TRACE_ERROR("hci_close(): Not inited\n");
        return  CYBT_ERR_HCI_NOT_INITIALIZE;
    }

	
	ptr = cybt_platform_hci_get_buffer(HCI_PACKET_TYPE_COMMAND, sizeof(hci_reset_cmd));
	memcpy(ptr, hci_reset_cmd, sizeof(hci_reset_cmd));
	is_shutdown_progress = true;
	cybt_platform_hci_write(HCI_PACKET_TYPE_COMMAND,ptr, sizeof(hci_reset_cmd));

    return status;
}

uint16_t cybt_platform_get_event_id(void *event)
{
    bt_task_event_t *evt = (bt_task_event_t *)event;
    return (evt->event_id);
}

void cybt_platform_hci_wait_for_boot_fully_up(bool is_from_isr)
{
    UNUSED_VARIABLE(is_from_isr);
}

void cybt_platform_hci_post_stack_init(void)
{

}

cybt_result_t cybt_platform_hci_set_baudrate(uint32_t baudrate)
{
    /* This function is not applicable if HCI communication is using IPC instead UART
     * Only reason to keeping this function is to maintain compatibility
     */

    UNUSED_VARIABLE(baudrate);
    return  CYBT_SUCCESS;
}

bool cybt_platform_hci_process_if_coredump(uint8_t *p_data, uint32_t length)
{
    /* Todo: decode according to bless controller core dump format */
    UNUSED_VARIABLE(p_data);
    UNUSED_VARIABLE(length);
    return (false);
}

#ifdef RECORD_IPC_STATS
cybt_result_t cybt_platform_hci_get_ipc_stats(cybt_ipc_stats_t *p_stats)
{
	p_stats->write_fail_count = ipc_stats.write_fail_count;
	p_stats->write_max_retry_count = ipc_stats.write_max_retry_count;
	return  CYBT_SUCCESS;
}
#endif
