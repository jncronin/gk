/*******************************************************************************
* \file cybt_platform_main.c
*
* \brief
* This file provides functions for WICED BT stack initialization.
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

#include "wiced_bt_dev.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_stack_platform.h"
#include "wiced_bt_stack.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_version.h"

#include "cybt_platform_task.h"
#include "cybt_platform_trace.h"
#include "cybt_platform_config.h"
#include "cybt_platform_interface.h"

wiced_result_t cybt_core_management_cback( wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data);
/******************************************************************************
 *                                Constants
 ******************************************************************************/
#define HCI_VSC_WRITE_SLEEP_MODE             (0xFC27)
#define HCI_VSC_WRITE_SLEEP_MODE_LENGTH      (12)

#define BT_SLEEP_MODE_UART                   (1)
#define BT_SLEEP_THRESHOLD_HOST              (1)
#define BT_SLEEP_THRESHOLD_HOST_CONTROLLER   (1)
#define BT_SLEEP_ALLOW_HOST_SLEEP_DURING_SCO (1)
#define BT_SLEEP_COMBINE_SLEEP_MODE_AND_LPM  (1)
#define BT_SLEEP_ENABLE_UART_TXD_TRISTATE    (0)
#define BT_SLEEP_PULSED_HOST_WAKE            (0)
#define BT_SLEEP_SLEEP_GUARD_TIME            (0)
#define BT_SLEEP_WAKEUP_GUARD_TIME           (0)
#define BT_SLEEP_TXD_CONFIG                  (1)
#define BT_SLEEP_BT_WAKE_IDLE_TIME           (50)


/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/
typedef struct
{
    wiced_bt_management_cback_t   *p_app_management_callback;
    const cybt_platform_config_t  *p_bt_platform_cfg;
    bool                          is_sleep_mode_enabled;
} cybt_platform_main_cb_t;

/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
cybt_platform_main_cb_t cybt_main_cb = {0};


/******************************************************************************
 *                          Function Declarations
 ******************************************************************************/
extern void host_stack_platform_interface_init(void);


/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
void bt_sleep_status_cback (wiced_bt_dev_vendor_specific_command_complete_params_t *p_command_complete_params)
{
    MAIN_TRACE_DEBUG("bt_sleep_status_cback(): status = 0x%x",
                     p_command_complete_params->p_param_buf[0]
                    );

    if(HCI_SUCCESS == p_command_complete_params->p_param_buf[0])
    {
        cybt_main_cb.is_sleep_mode_enabled = true;
    }
    else
    {
        cybt_main_cb.is_sleep_mode_enabled = false;
    }
}

bool bt_enable_sleep_mode(void)
{
    wiced_result_t result;
    uint8_t        sleep_vsc[HCI_VSC_WRITE_SLEEP_MODE_LENGTH];

    MAIN_TRACE_DEBUG("bt_enable_sleep_mode()");

    sleep_vsc[0] = BT_SLEEP_MODE_UART;
    sleep_vsc[1] = BT_SLEEP_THRESHOLD_HOST;
    sleep_vsc[2] = BT_SLEEP_THRESHOLD_HOST_CONTROLLER;
    sleep_vsc[3] = cybt_main_cb.p_bt_platform_cfg->controller_config.sleep_mode.device_wake_polarity;
    sleep_vsc[4] = cybt_main_cb.p_bt_platform_cfg->controller_config.sleep_mode.host_wake_polarity;
    sleep_vsc[5] = BT_SLEEP_ALLOW_HOST_SLEEP_DURING_SCO;
    sleep_vsc[6] = BT_SLEEP_COMBINE_SLEEP_MODE_AND_LPM;
    sleep_vsc[7] = BT_SLEEP_ENABLE_UART_TXD_TRISTATE;
    sleep_vsc[8] = 0;
    sleep_vsc[9] = 0;
    sleep_vsc[10] = 0;
    sleep_vsc[11] = BT_SLEEP_PULSED_HOST_WAKE;

    result = wiced_bt_dev_vendor_specific_command(HCI_VSC_WRITE_SLEEP_MODE,
                                                  HCI_VSC_WRITE_SLEEP_MODE_LENGTH,
                                                  sleep_vsc,
                                                  bt_sleep_status_cback
                                                 );
    if(WICED_BT_PENDING != result)
    {
        MAIN_TRACE_DEBUG("bt_enable_sleep_mode(): Fail to send vsc (0x%x)", result);
        return false;
    }

    return true;
}

/*
 * On stack initalization complete this call back gets called
*/
void wiced_post_stack_init_cback( void )
{
    wiced_bt_management_evt_data_t event_data;
    cybt_controller_sleep_config_t *p_sleep_config =
        &(((cybt_platform_config_t *)cybt_main_cb.p_bt_platform_cfg)->controller_config.sleep_mode);

    MAIN_TRACE_DEBUG("wiced_post_stack_init_cback");

    memset(&event_data, 0, sizeof(wiced_bt_management_evt_t));
    event_data.enabled.status = WICED_BT_SUCCESS;
    cybt_core_management_cback( BTM_ENABLED_EVT, &event_data);

    if((CYBT_SLEEP_MODE_ENABLED == p_sleep_config->sleep_mode_enabled)
       && (NC != p_sleep_config->device_wakeup_pin)
       && (NC != p_sleep_config->host_wakeup_pin)
      )
    {
        bool status = bt_enable_sleep_mode();

        if(false == status)
        {
            MAIN_TRACE_ERROR("wiced_post_stack_init_cback(): Fail to init sleep mode");
        }
    }
    else
    {
        MAIN_TRACE_ERROR("wiced_post_stack_init_cback(): BT sleep mode is NOT enabled");
    }
}

/*
* This call back gets called for each HCI event.
*/
wiced_bool_t wiced_stack_event_handler_cback (uint8_t *p_event)
{
    return WICED_FALSE;
}

#if (defined(BTSTACK_VER) && (BTSTACK_VER >= 0x03070000))
static wiced_result_t init_layers(void)
{
    /* handle in porting layer */
    return wiced_bt_smp_module_init();
}

#if (defined(__GNUC__) || defined(__ARMCC_VERSION))
extern __attribute__((weak)) wiced_result_t app_initialize_btstack_modules(void)
{
     return init_layers();
}
#endif

#if defined(__ICCARM__)
extern wiced_result_t app_initialize_btstack_modules(void);
#pragma weak app_initialize_btstack_modules=init_layers
#endif

#endif // BTSTACK_VER

wiced_result_t cybt_core_management_cback( wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data )
{
    wiced_result_t result = WICED_BT_SUCCESS;
    int send_to_app = 1;
#if (defined(BTSTACK_VER) && (BTSTACK_VER >= 0x03070000))
    switch(event)
    {
        case BTM_ENABLED_EVT:
#if (!defined(DISABLE_DEFAULT_BTSTACK_INIT) || (DISABLE_DEFAULT_BTSTACK_INIT == 1))
            app_initialize_btstack_modules(); // initialize all layers
#endif
            wiced_bt_init_resolution(); // To be removed, only required for non-privacy controllers
        break;
    }
#endif // BTSTACK_VER

    if(send_to_app && cybt_main_cb.p_app_management_callback)
    {
        result = cybt_main_cb.p_app_management_callback(event, p_event_data);
    }

    return result;
}

void cybt_core_stack_init(void)
{
    /* Start the stack */
    wiced_bt_stack_init_internal(cybt_core_management_cback,
                                 wiced_post_stack_init_cback,
                                 wiced_stack_event_handler_cback
                                );
}

wiced_result_t wiced_bt_stack_init(wiced_bt_management_cback_t *p_bt_management_cback,
                                           const wiced_bt_cfg_settings_t *p_bt_cfg_settings
                                          )
{
    MAIN_TRACE_DEBUG("wiced_bt_stack_init()");

    cybt_main_cb.is_sleep_mode_enabled = false;

    cybt_platform_init();

    cybt_main_cb.p_app_management_callback   = p_bt_management_cback;

    host_stack_platform_interface_init();

    /* Configure the stack */
    wiced_bt_set_stack_config(p_bt_cfg_settings);

    cybt_platform_task_init((void *)p_bt_cfg_settings);

    return WICED_BT_SUCCESS;
}

wiced_result_t wiced_bt_stack_deinit( void )
{
    extern cy_thread_t cybt_task[BT_TASK_NUM];
#if( configUSE_TICKLESS_IDLE != 0 )
    extern cy_thread_t sleep_timer_task;
    extern void cybt_platform_terminate_sleep_thread(void);
#endif
    cy_thread_t cur_thread_handle;

    if(CY_RSLT_SUCCESS != cy_rtos_get_thread_handle(&cur_thread_handle))
    {
        MAIN_TRACE_ERROR("wiced_bt_stack_deinit(): Fail to deinit stack.");
        return WICED_BT_ERROR;
    }

    if((cur_thread_handle == cybt_task[BT_TASK_ID_HCI_RX]) || (cur_thread_handle == cybt_task[BT_TASK_ID_HCI_TX])
    #if( configUSE_TICKLESS_IDLE != 0 )
        || (cur_thread_handle == sleep_timer_task)
    #endif
        )
    {
        /** This API is used to terminate all BT-own tasks and release all related resources.
         *  According to the current design, the OS task cannot be deleted by itself.
            In other words, the application must call wiced_stack_deinit( ) in the task context other than BT-own ones.
         */
        MAIN_TRACE_ERROR("Please call this API- wiced_bt_stack_deinit() in application thread.");
        return WICED_BT_ERROR;
    }
    else
    {
        cybt_platform_task_deinit();

        cybt_platform_deinit();

        cybt_platform_terminate_hci_rx_thread();

        cybt_platform_terminate_hci_tx_thread();

    #if( configUSE_TICKLESS_IDLE != 0 )
        cybt_platform_terminate_sleep_thread();
    #endif
    }

    return WICED_BT_SUCCESS;
}

void cybt_platform_config_init(const cybt_platform_config_t *p_bt_platform_cfg)
{
    MAIN_TRACE_DEBUG("cybt_platform_config_init()");

    cybt_main_cb.p_bt_platform_cfg = p_bt_platform_cfg;
}

bool cybt_platform_get_sleep_mode_status(void)
{
    return cybt_main_cb.is_sleep_mode_enabled;
}

const cybt_platform_config_t* cybt_platform_get_config(void)
{
    return cybt_main_cb.p_bt_platform_cfg;
}

