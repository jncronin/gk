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
#ifndef DISABLE_COMMAND_CONSOLE_BT

#include "command_console.h"
#include "bt_cfg.h"
#include "wiced_bt_stack.h"
#ifndef COMPONENT_CAT5
#include "cybt_platform_trace.h"
#endif
#include "wiced_memory.h"
#include "stdio.h"
#include "stdlib.h"
#include "wiced_bt_dev.h"
#include "cyabs_rtos.h"
#include "wiced_bt_l2c.h"
#include "wiced_data_types.h"

#define BT_LE_INFO(X)   printf X
#define BT_LE_DEBUG(X)  //printf X
#define BT_LE_ERROR(X)  printf X

/******************************************************
 *                      Macros
 ******************************************************/
/* BLE thread priority should be at par with iperf thread priority to ensure the CPU cycles are equally available b/w both BT and Wi-Fi for coex use cases */
#define BLE_WORKER_THREAD_PRIORITY      ((cy_thread_priority_t)(CY_RTOS_PRIORITY_HIGH - 1))
#define BLE_WORKER_THREAD_STACK_SIZE    (2 * 1024)
#define SEND_DATA_LE                    (1)
#define STOP_DATA_SEND                  (2)
#define APP_WORKER_THREAD_STACK_SIZE    (2048)
#define APP_QUEUE_MAX_ENTRIES           (5)
#define NEVER_TIMEOUT                   ((uint32_t) 0xFFFFFFFF)
#ifndef BLE_COC_MTU_SIZE
#define BLE_COC_MTU_SIZE                (100)
#endif
#define LE_COC_PSM                      (19)
#define MAX_SEMA_COUNT                  (1)
#define MAX_MUTEX_WAIT_TIME_MS          (0xFFFFFFFF)

/**
 * Macro to verify that BT is on
 * if not, return with result value;
 */
#define IS_BT_ON()   { if (!bt_state) { BT_LE_INFO( (" Error BT is OFF \r\n")); return WICED_ERROR;}}

#if(BLE_COC_MTU_SIZE > 512)
#error Maximum value of BLE_COC_MTU_SIZE that can be configured is 512
#endif

#if defined(__ICCARM__)
#define WICED_BLE_WEAK_FUNC            __WEAK
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#define WICED_BLE_WEAK_FUNC            __attribute__((weak))
#else
#define WICED_BLE_WEAK_FUNC           __attribute__((weak))
#endif

/******************************************************
 *               Function Declarations
 ******************************************************/
/* add function definiton */
/* Console commands */
int handle_bt_on                               (int argc, char *argv[], tlv_buffer_t** data);
int handle_bt_off                              (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_start_adv                       (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_stop_adv                        (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_start_scan                      (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_stop_scan                       (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_coc_init                        (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_coc_adv                         (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_coc_scan_connect                (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_coc_send_start                  (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_coc_disconnect                  (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_coc_send_stop                   (int argc, char *argv[], tlv_buffer_t** data);
int handle_ble_get_throughput                  (int argc, char *argv[], tlv_buffer_t** data);
int handle_bt_get_device_address               (int argc, char *argv[], tlv_buffer_t** data);

/* all the definitions here */
#define TEST_BT_COMMANDS \
    { (char*) "bt_on",                    handle_bt_on,                       0, NULL, NULL, (char*) "",         (char*) "Turn On  Bluetooth"}, \
    { (char*) "bt_off",                   handle_bt_off,                      0, NULL, NULL, (char*) "",         (char*) "Turn Off Bluetooth"}, \
    { (char*) "bt_get_device_address",    handle_bt_get_device_address,       0, NULL, NULL, (char*) "",         (char*) "Get Bluetooth Device Address" }, \
    { (char*) "ble_start_adv",            handle_ble_start_adv,               0, NULL, NULL, (char*) "",         (char*) "Start BLE Advertisement."}, \
    { (char*) "ble_stop_adv",             handle_ble_stop_adv,                0, NULL, NULL, (char*) "",         (char*) "Stop BLE Advertisement."}, \
    { (char*) "ble_start_scan",           handle_ble_start_scan,              0, NULL, NULL, (char*) "",         (char*) "Start BLE Scan"}, \
    { (char*) "ble_stop_scan",            handle_ble_stop_scan,               0, NULL, NULL, (char*) "",         (char*) "Stop BLE Scan"}, \
    { (char*) "ble_coc_init",             handle_ble_coc_init,                0, NULL, NULL, (char*) "",         (char*) "Initializes LE COC with PSM 19 and MTU 100" }, \
    { (char*) "ble_coc_adv",              handle_ble_coc_adv,                 0, NULL, NULL, (char*) "",         (char*) "Start LE COC advertisements" }, \
    { (char*) "ble_coc_scan_connect",     handle_ble_coc_scan_connect,        0, NULL, NULL, (char*) "",         (char*) "Scan and Connect to a LE COC server" }, \
    { (char*) "ble_coc_disconnect",       handle_ble_coc_disconnect,          0, NULL, NULL, (char*) "",         (char*) "Disconnect LE COC" }, \
    { (char*) "ble_coc_send_start",       handle_ble_coc_send_start,          0, NULL, NULL, (char*) "",         (char*) "Start Sending LE COC data" }, \
    { (char*) "ble_coc_send_stop",        handle_ble_coc_send_stop,           0, NULL, NULL, (char*) "",         (char*) "Stop Sending LE COC data" }, \
    { (char*) "ble_get_throughput",       handle_ble_get_throughput,          0, NULL, NULL, (char*) "",         (char*) "Get LE COC Throughput" }, \

/******************************************************
 *               Static Function Declarations
 ******************************************************/
static wiced_bt_dev_status_t bt_management_cback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data);
static void le_coc_set_advertisement_data(void);
static void le_coc_connect_ind_cback(wiced_bt_device_address_t bda, uint16_t local_cid, uint16_t psm, uint8_t id, uint16_t mtu_peer);
static void le_coc_connect_cfm_cback(uint16_t local_cid, uint16_t result, uint16_t mtu_peer);
static void le_coc_disconnect_ind_cback(uint16_t local_cid, uint16_t reason, wiced_bool_t ack);
static void le_coc_disconnect_cfm_cback(uint16_t local_cid, uint16_t result);
static void le_coc_data_cback(uint16_t local_cid, tDRB *p_drb);
static void le_coc_tx_complete_cback(uint16_t local_cid, void *p_data);
static void le_coc_scan_result_cback(wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data);
static void l2cap_test_release_drb (tDRB *p_drb);
static void send_data_thread(cy_thread_arg_t thread_input);
static void scan_result_cback(wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data);

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    uint16_t local_cid;
    wiced_bt_device_address_t peer_bda;
    uint8_t congested;
    uint16_t peer_mtu;
} le_coc_cb_t;

wiced_bt_l2cap_le_appl_information_t l2c_appl_info =
{
    &le_coc_connect_ind_cback,
    &le_coc_connect_cfm_cback,
    &le_coc_disconnect_ind_cback,
    &le_coc_disconnect_cfm_cback,
    &le_coc_data_cback,
    &le_coc_tx_complete_cback,
    &l2cap_test_release_drb
};

/******************************************************
 *               Variable Definitions
 ******************************************************/
bool bt_state         = WICED_FALSE;
static bool           bt_on_init_done = false;
cy_thread_t           ble_thread;
static volatile bool  send_data;
static uint32_t       data_le_tx_counter;
static uint32_t       data_le_rx_counter;
static bool           le_coc_data_rx_flag;
static cy_time_t      start_time;
static cy_time_t      end_time;
cy_semaphore_t        send_data_semaphore;
uint16_t              psm;
uint16_t              our_mtu;
uint8_t               lec_coc_data[ BLE_COC_MTU_SIZE ];
le_coc_cb_t           le_coc_cb;
static cy_mutex_t     bt_mutex;

const cy_command_console_cmd_t bt_coex_command_table[] =
{
    TEST_BT_COMMANDS
    CMD_TABLE_END
};
int handle_bt_on(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    if (bt_state == WICED_TRUE)
    {
        BT_LE_DEBUG(("BT is already ON \n"));
        return result;
    }
#ifndef COMPONENT_CAT5
    cybt_platform_config_init(&bt_platform_cfg_settings);
#endif
    wiced_bt_stack_init(bt_management_cback, &wiced_bt_command_console_cfg_settings);
    // create application heap for BT
    wiced_bt_create_heap ("app", NULL, 0x1000, NULL, WICED_TRUE);\

    BT_LE_INFO(("Executed BT ON , please wait to get Bluetooth enabled event \n"));
    return result;
}

int handle_bt_off(int argc, char *argv[], tlv_buffer_t** data)
{
#ifdef COMPONENT_CAT5
    BT_LE_INFO(("BT OFF is not supported on CAT5 devices \n"));
    return 0;
#endif
    int result = 0;
    BT_LE_DEBUG(("bt_off\n"));

    wiced_bt_stack_deinit( );
    bt_state = WICED_FALSE;

    if(bt_on_init_done)
    {
        cy_rtos_terminate_thread(&ble_thread);
        cy_rtos_deinit_semaphore(&send_data_semaphore);
        cy_rtos_deinit_mutex(&bt_mutex);
        bt_on_init_done = false;
    }

    return result;
}

int handle_ble_start_adv(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;

    IS_BT_ON();
    le_coc_set_advertisement_data( );

    BT_LE_INFO(("Starting advertisement \n"));
    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
    BT_LE_INFO(("Start advertisement,  result =  0x%X\n", (unsigned int)result));

    return result;
}

int handle_ble_stop_adv(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    IS_BT_ON();

    BT_LE_INFO(("Stopping advertisement \n"));
    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_OFF, 0, NULL);
    BT_LE_INFO(("Stop advertisement, result = 0x%X\n", (unsigned int)result));

    return result;
}

int handle_ble_start_scan(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    IS_BT_ON();

    BT_LE_INFO(("Starting scan operation \n"));
    result = wiced_bt_ble_scan( BTM_BLE_SCAN_TYPE_HIGH_DUTY, WICED_TRUE, scan_result_cback);
    BT_LE_INFO(("Start Scan, result = 0x%X\n", (unsigned int)result));

    return result;
}

int handle_ble_stop_scan(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    IS_BT_ON();
    result = wiced_bt_ble_scan(BTM_BLE_SCAN_TYPE_NONE, WICED_TRUE, NULL);
    BT_LE_DEBUG(("Stop Scan result = 0x%X\n", (unsigned int)result) );
    return result;
}


int handle_ble_get_throughput(int argc, char *argv[], tlv_buffer_t** data)
{
    float data_rate;
    float elapsed_time;
    BT_LE_INFO(("get_throughput \n"));

    if(data_le_rx_counter != 0)
    {
        BT_LE_INFO(("Throughput details for DATA RX ..\n"));
        cy_rtos_get_time(&end_time);
        elapsed_time = (float)(end_time - start_time)/1000;
        data_rate = (((float)data_le_rx_counter * 8)/elapsed_time);
        BT_LE_INFO(("start Time = %u ...\n", (unsigned int)start_time));
        BT_LE_INFO(("END Time = %u ...\n", (unsigned int)end_time));
        BT_LE_INFO(("elapsed time in seconds = %f \n", (double)elapsed_time));
        BT_LE_INFO(("total le bytes recieved  = %u \n", (unsigned int)data_le_rx_counter));
        BT_LE_INFO(("RX throughput =  %f bps\n", (double)data_rate));
        le_coc_data_rx_flag = false;
        data_le_rx_counter = 0;
        start_time = 0;
        end_time = 0;
    }

    if(data_le_tx_counter != 0)
    {
        BT_LE_INFO(("Throughput details for DATA TX ..\n"));
        elapsed_time = (float)(end_time - start_time)/1000;
        data_rate = (((float)data_le_tx_counter * 8)/elapsed_time);
        BT_LE_INFO(("start Time = %u ...\n", (unsigned int)start_time));
        BT_LE_INFO(("END Time = %u ...\n", (unsigned int)end_time));
        BT_LE_INFO(("elapsed time in seconds = %f \n", (double)elapsed_time));
        BT_LE_INFO(("total le bytes transferred  = %u \n", (unsigned int)data_le_tx_counter));
        BT_LE_INFO(("TX throughput =  %f bps\n", (double)data_rate));
        data_le_tx_counter = 0;
        start_time = 0;
        end_time = 0;
    }
    return 0;
}
void start_data_thread()
{
    if (cy_rtos_init_mutex(&bt_mutex) != CY_RSLT_SUCCESS)
    {
        BT_LE_INFO(("Unable to init the mutex \n"));
        return;
    }

    if(cy_rtos_init_semaphore(&send_data_semaphore, MAX_SEMA_COUNT, 0) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("unable to create semaphore \n"));
        cy_rtos_deinit_mutex(&bt_mutex);
        return;
    }

    /* start BLE TX/RX data Thread */
    if (cy_rtos_create_thread(&ble_thread, send_data_thread, "BLE Data Thread", NULL, BLE_WORKER_THREAD_STACK_SIZE, BLE_WORKER_THREAD_PRIORITY, 0) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("unable to create the thread \n"));
        cy_rtos_deinit_semaphore(&send_data_semaphore);
        cy_rtos_deinit_mutex(&bt_mutex);
        return;
    }
    bt_on_init_done = true;
}

wiced_bt_dev_status_t bt_management_cback( wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data )
{
    wiced_bt_device_address_t bda;
    wiced_bt_ble_advert_mode_t *p_mode;

    BT_LE_DEBUG(("Bluetooth Management Event: 0x%x\n", event));

    wiced_bt_dev_status_t status = WICED_BT_SUCCESS;
    switch ( event )
    {
        case BTM_ENABLED_EVT:
            /* Bluetooth controller and host stack enabled */
            BT_LE_INFO(("Bluetooth enabled (%s)\n", ((p_event_data->enabled.status == WICED_BT_SUCCESS) ? "success":"failure" )));

            if ( p_event_data->enabled.status == WICED_BT_SUCCESS )
            {
                bt_state = WICED_TRUE;
                wiced_bt_dev_read_local_addr( bda );
                BT_LE_INFO(("Local Bluetooth Address: [%02X:%02X:%02X:%02X:%02X:%02X]\n", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]));

            }

            start_data_thread();

            break;

        case BTM_DISABLED_EVT:
            BT_LE_INFO(( "BT turned off \n"));
            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
            /* Request for stored link keys for remote device (if any) */
            /* (sample app does not store link keys to NVRAM) */
            status = WICED_BT_UNKNOWN_ADDR;
            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
            /* Request store newly generated pairing link keys to NVRAM */
            /* (sample app does not store link keys to NVRAM) */
            break;

        case BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT:
            /* Request to restore local identity keys from NVRAM (requested during Bluetooth start up) */
            /* (sample app does not store keys to NVRAM. New local identity keys will be generated).   */
            status = WICED_BT_NO_RESOURCES;
            break;

        case BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT:
            /* Request to store newly generated local identity keys to NVRAM */
            /* (sample app does not store keys to NVRAM) */
            break;

        case BTM_USER_CONFIRMATION_REQUEST_EVT:
            /* User confirmation request for pairing (sample app always accepts) */
            wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data->user_confirmation_request.bd_addr);
            break;

        case BTM_PAIRING_COMPLETE_EVT:
            /* Pairing complete */
            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            /* adv state change callback */
            BT_LE_DEBUG(("---->>> New ADV state: %d\n", p_event_data->ble_advert_state_changed));
            p_mode = &p_event_data->ble_advert_state_changed;
            BT_LE_DEBUG(( "Advertisement State Change: %d\n", *p_mode));
            if (*p_mode == BTM_BLE_ADVERT_OFF)
            {
                BT_LE_INFO(( "ADV stopped\n"));
            }
            break;

        default:
            BT_LE_DEBUG(( "Unhandled Bluetooth Management Event: 0x%x\n", event));
            break;
    }

    return (status);
}

/* L2CAP Data RX callback */
static void le_coc_data_cback(uint16_t local_cid, tDRB *p_drb)
{
    BT_LE_INFO(("[%s] \n", __func__));
    uint16_t    rcvd_len = p_drb->drb_data_len;

    if (le_coc_data_rx_flag == FALSE)
    {
        le_coc_data_rx_flag = TRUE;
        start_time = 0;
        cy_rtos_get_time(&start_time);
    }
    data_le_rx_counter += rcvd_len;
    BT_LE_INFO(("[%s] received %u bytes\n", __func__, (unsigned int)data_le_rx_counter));
    return;
}

/* L2CAP connection management callback */
static void le_coc_connect_ind_cback(wiced_bt_device_address_t bda, uint16_t local_cid, uint16_t psm, uint8_t id, uint16_t mtu_peer )
{
    tDRB    *p_rx_drb = (tDRB *)wiced_bt_get_buffer (our_mtu + DRB_OVERHEAD_SIZE);
    uint8_t *p_data = le_coc_cb.peer_bda;

    BT_LE_INFO(( "[%s] from  [%02X:%02X:%02X:%02X:%02X:%02X]\n",__func__, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5] ) );
    BT_LE_INFO(("[%s] CID %d PSM 0x%x MTU %d \n", __func__, local_cid, psm, mtu_peer) );

    /* Store peer info for reference*/
    le_coc_cb.local_cid = local_cid;
    BDADDR_TO_STREAM( p_data, bda );
    le_coc_cb.peer_mtu = mtu_peer;

    /* Accept the connection */
    wiced_bt_l2cap_le_connect_rsp( bda, id, local_cid, L2CAP_CONN_OK, our_mtu, p_rx_drb);

    /* Stop advertising */
    wiced_bt_start_advertisements( BTM_BLE_ADVERT_OFF, 0, NULL );

    le_coc_data_rx_flag = FALSE;

}

static void le_coc_connect_cfm_cback(uint16_t local_cid, uint16_t result, uint16_t mtu_peer)
{
    BT_LE_INFO(("[%s] MTU %d , result = %d , local_cid = %d \n", __func__, mtu_peer, result, local_cid));

    if(result == 0)
    {
        /* Store peer info for reference*/
        le_coc_cb.local_cid = local_cid;
        le_coc_cb.peer_mtu = mtu_peer;
    }

    le_coc_data_rx_flag = FALSE;

}

static void le_coc_disconnect_ind_cback(uint16_t local_cid, uint16_t reason, wiced_bool_t ack)
{
    BT_LE_INFO( ("[%s] CID %d \n", __func__, local_cid) );

    cy_rtos_get_time(&end_time);

    /* Send disconnect response if needed */
    if ( ack )
    {
        wiced_bt_l2cap_le_disconnect_rsp( local_cid );
    }

    if ( le_coc_cb.local_cid == local_cid )
    {
          le_coc_cb.local_cid = 0xFFFF;
        memset( le_coc_cb.peer_bda, 0, BD_ADDR_LEN );
    }
}

static void le_coc_disconnect_cfm_cback(uint16_t local_cid, uint16_t result)
{
    BT_LE_DEBUG( ("[%s] , result = %d CID %d \n", __func__, result, local_cid) );
    if (le_coc_cb.local_cid == local_cid)
    {
        le_coc_cb.local_cid = 0xFFFF;
        memset( le_coc_cb.peer_bda, 0, BD_ADDR_LEN );
    }
}


static void le_coc_tx_complete_cback(uint16_t local_cid, void *p_data)
{
    if(cy_rtos_get_mutex(&bt_mutex, MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("Unable to acquire mutex \n"));
    }

    data_le_tx_counter += our_mtu;
    if(send_data)
    {
         cy_rtos_set_semaphore(&send_data_semaphore, false);
    }
    else
    {
        BT_LE_INFO(("data is stopped \n"));
    }
    if (cy_rtos_set_mutex(&bt_mutex) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("Unable to release mutex \n"));
    }
}

static void l2cap_test_release_drb (tDRB *p_drb)
{
    BT_LE_DEBUG(("l2cap_test_release_drb: 0x%08x", p_drb));
}


int handle_ble_coc_disconnect(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    BT_LE_INFO(("[%s] \n" , __func__));
    le_coc_data_rx_flag = FALSE;
    if ( le_coc_cb.local_cid )
    {
        result = wiced_bt_l2cap_le_disconnect_req( le_coc_cb.local_cid );
    }
    return result;
}
static void le_coc_set_advertisement_data(void)
{
    wiced_bt_ble_advert_elem_t adv_data[ 3 ];
    uint8_t adv_flags = BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED;
    uint8_t num_elem = 0;

    adv_data[ num_elem ].advert_type = BTM_BLE_ADVERT_TYPE_FLAG;
    adv_data[ num_elem ].len = 1;
    adv_data[ num_elem ].p_data = &adv_flags;
    num_elem++;

    adv_data[ num_elem ].advert_type = BTM_BLE_ADVERT_TYPE_NAME_SHORT;
    adv_data[ num_elem ].len = strlen( (const char *) wiced_bt_command_console_cfg_settings.device_name);
    adv_data[ num_elem ].p_data = wiced_bt_command_console_cfg_settings.device_name;
    num_elem++;

    if((wiced_bt_ble_set_raw_advertisement_data( num_elem, adv_data)) != WICED_BT_SUCCESS)
    {
        BT_LE_ERROR(("[%s] Unable to set ADV data... \n", __func__));
    }
    else
    {
        BT_LE_DEBUG(("[%s] advertisement data is set... \n", __func__));
    }
}

int handle_ble_coc_init(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    int i = 0;
    IS_BT_ON();

    le_coc_data_rx_flag = false;
    data_le_rx_counter = 0;
    data_le_tx_counter = 0;
    our_mtu = BLE_COC_MTU_SIZE;
    /* Clear app control block */
    memset( &le_coc_cb, 0, sizeof(le_coc_cb_t));
    le_coc_cb.local_cid = 0xFFFF;

    /* Register LE l2cap callbacks */
    result = wiced_bt_l2cap_le_register(LE_COC_PSM, &l2c_appl_info);

    for(i = 0; i < BLE_COC_MTU_SIZE; i++)
    {
        lec_coc_data[i] = 0x41;
    }

    BT_LE_INFO(("LE COC initalized local device MTU = %d and PSM = %d \n", our_mtu, LE_COC_PSM));

    return result;
}

int handle_ble_coc_adv(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;
    IS_BT_ON();
    le_coc_set_advertisement_data();
    wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL );
    return result;
}

int handle_ble_coc_scan_connect(int argc, char *argv[], tlv_buffer_t** data)
{
    int result = 0;

    IS_BT_ON();
    result = wiced_bt_ble_scan( BTM_BLE_SCAN_TYPE_HIGH_DUTY, 0, le_coc_scan_result_cback );

    return result;
}

/* Initiate connection */
void le_coc_connect( wiced_bt_device_address_t bd_addr, wiced_bt_ble_address_type_t bd_addr_type )
{
    uint8_t req_security = 0;
    uint8_t req_encr_key_size = 0;
    uint8_t *p_data = le_coc_cb.peer_bda;
    tDRB    *p_rx_drb;
    p_rx_drb = (tDRB *)wiced_bt_get_buffer (our_mtu + DRB_OVERHEAD_SIZE);

    memset(p_rx_drb, 0, our_mtu + DRB_OVERHEAD_SIZE);
    /* Initiate the connection L2CAP connection */
    wiced_bt_l2cap_le_connect_req( LE_COC_PSM, (uint8_t*) bd_addr, bd_addr_type, BLE_CONN_MODE_HIGH_DUTY, our_mtu,
                                   req_security, req_encr_key_size, p_rx_drb);
    BDADDR_TO_STREAM( p_data, bd_addr );
}

int handle_ble_coc_send_start(int argc, char *argv[], tlv_buffer_t** data)
{
    uint8_t ret_val = 0;
    start_time = 0;
    data_le_tx_counter = 0;
    IS_BT_ON();
    cy_rtos_get_time(&start_time);
    BT_LE_INFO(("current_time = %u \n", (unsigned int)start_time));
    if(cy_rtos_get_mutex(&bt_mutex, MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("Unable to acquire mutex \n"));
        return -1;
    }

    send_data = true;

    if (cy_rtos_set_mutex(&bt_mutex) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("Unable to release mutex \n"));
        return -1;
    }
    cy_rtos_set_semaphore(&send_data_semaphore, false);
    return ret_val;
}


int handle_ble_coc_send_stop(int argc, char *argv[], tlv_buffer_t** data)
{
    uint8_t ret_val = 0;
    IS_BT_ON();
    BT_LE_INFO(("stop_data_send ...\n"));
    if(cy_rtos_get_mutex(&bt_mutex, MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("Unable to acquire mutex \n"));
    }

    send_data = false;

    if (cy_rtos_set_mutex(&bt_mutex) != CY_RSLT_SUCCESS)
    {
        BT_LE_ERROR(("Unable to release mutex \n"));
    }
    cy_rtos_get_time(&end_time);
    return ret_val;
}
/*
 * Process advertisement packet received
 */
static void le_coc_scan_result_cback( wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data )
{
    uint8_t length;
    uint8_t * p_data;

    BT_LE_DEBUG(("le_coc_scan_result_cback \n"));
    if(p_scan_result)
    {
        p_data = wiced_bt_ble_check_advertising_data(p_adv_data, BTM_BLE_ADVERT_TYPE_NAME_SHORT, &length);
        if (memcmp(p_data, wiced_bt_command_console_cfg_settings.device_name, strlen( (const char *) wiced_bt_command_console_cfg_settings.device_name ) ) == 0 )
        {
            BT_LE_INFO( ("Found LE COC Server \n") );
            /* Initiate the connection L2CAP connection */
            wiced_bt_ble_scan( BTM_BLE_SCAN_TYPE_NONE, 0, NULL );
            le_coc_connect( p_scan_result->remote_bd_addr, p_scan_result->ble_addr_type );
        }
        else
        {
            return;
        }
    }
}

static void send_data_thread(cy_thread_arg_t thread_input)
{
    do
    {
        cy_rtos_get_semaphore(&send_data_semaphore, NEVER_TIMEOUT, false);

        if(cy_rtos_get_mutex(&bt_mutex, MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
        {
            BT_LE_ERROR(("Unable to acquire mutex \n"));
        }

        if (send_data)
        {
            wiced_bt_l2cap_le_data_write( le_coc_cb.local_cid, lec_coc_data, our_mtu);
        }
        else
        {
            BT_LE_INFO((" DATA SEND STOPPED \n"));
            cy_rtos_get_time(&end_time);
            BT_LE_INFO(("start Time = %u ...\n", (unsigned int)start_time));
            BT_LE_INFO(("END Time = %u ...\n", (unsigned int)end_time));
            BT_LE_INFO(("elapsed time = %u \n", (unsigned int)(end_time - start_time)));
            BT_LE_INFO(("total le bytes transmitted = %u \n", (unsigned int)data_le_tx_counter));
        }

        if (cy_rtos_set_mutex(&bt_mutex) != CY_RSLT_SUCCESS)
        {
            BT_LE_ERROR(("Unable to release mutex \n"));
        }
    } while(1);
}

int handle_bt_get_device_address(int argc, char *argv[], tlv_buffer_t** data)
{
    wiced_bt_device_address_t bda;
    IS_BT_ON();
    wiced_bt_dev_read_local_addr(bda);
    BT_LE_INFO(("Local Bluetooth Address: [%02X:%02X:%02X:%02X:%02X:%02X]\n", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]));
    return 0;
}

/*
 * This function handles the scan results
 */
void scan_result_cback(wiced_bt_ble_scan_results_t *p_scan_result, uint8_t *p_adv_data)
{
    (void)(p_adv_data);
    if (p_scan_result)
    {
        BT_LE_INFO(("Found Device: %02X:%02X:%02X:%02X:%02X:%02X \t RSSI: %d\n", p_scan_result->remote_bd_addr[0], p_scan_result->remote_bd_addr[1], p_scan_result->remote_bd_addr[2],
                p_scan_result->remote_bd_addr[3], p_scan_result->remote_bd_addr[4], p_scan_result->remote_bd_addr[5], p_scan_result->rssi));
    }
    else
    {
        BT_LE_INFO(( "Scan completed.\n" ));
    }
}

WICED_BLE_WEAK_FUNC void bt_utility_init(void)
{
    cy_command_console_add_table (bt_coex_command_table);
}
#endif
