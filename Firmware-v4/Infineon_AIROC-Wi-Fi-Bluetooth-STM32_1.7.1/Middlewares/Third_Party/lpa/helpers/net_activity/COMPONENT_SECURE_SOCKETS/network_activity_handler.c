/*******************************************************************************
* File Name: network_activity_handler.c
*
* Version: 1.0
*
* Description: This file implements the functions needed to suspend
* network stack and wait till external event or timeout
*
********************************************************************************
* Copyright 2020 Cypress Semiconductor Corporation
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
********************************************************************************/

#include "network_activity_handler.h"
#include "cyabs_rtos.h"
#include "cy_network_mw_core.h"

#include "cy_OlmInterface.h"
#include "whd_int.h"
#include <cycfg_system.h>
#include "cyhal.h"

/* lwIP header files */
#include <lwip/tcpip.h>
#include <lwip/api.h>
#include <lwip/tcp.h>
#include <lwip/priv/tcp_priv.h>
#include "cy_nw_helper.h"

/* secure socket Header files */
#include "cy_secure_sockets.h"
#include "cy_tls.h"

/******************************************************
 *                      Macros
 ******************************************************/

#define IDLE_POWER_MODE_STRING_LEN (32)

#define PACKET_PAYLOAD 1500
/* TCP user data buffer to send to server */
static uint8_t cy_tcp_databuf[PACKET_PAYLOAD];
#define HARDCODED_STR   "Some random stuff"

extern whd_interface_t cy_olm_get_whd_interface ( void );

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/*******************************************************************************
* Function Name: cylpa_get_idle_power_mode
********************************************************************************
*
* Summary: This function is used to get the currently configured MCU low power
* mode, and copy it as a string into the first argument.
*
* Parameters:
* char *str: Pointer to character array where idle power mode string should be copied
* uint32_t length of character array passed in argument 1.
*
* Return:
* void.
*
*******************************************************************************/
static void cylpa_get_idle_power_mode(char *str, uint32_t length);

/*******************************************************************************
* Function Name: cylpa_print_whd_bus_stats
********************************************************************************
*
* Summary: This function is used to print the WHD bus stats, one of the useful
* the information includes no of WLAN Out-of-Band interrupts and In-band interrupts.
*
* Parameters:
* netif *wifi: pointer to WLAN interface
*
* Return:
* void.
*
*******************************************************************************/
static void cylpa_print_whd_bus_stats(struct netif *wifi);


/*******************************************************************************
* Function Name: cylpa_register_network_activity_callback
********************************************************************************
*
* Summary: This function is used to register a callback for the event of net
* activity.
*
* Return:
* cy_rslt_t: contains status of attaching callback.
*
*******************************************************************************/
static cy_rslt_t cylpa_register_network_activity_callback(void);

/*******************************************************************************
* Function Name: cylpa_network_state_handler
********************************************************************************
*
* Summary: This function prints state of the network stack on the terminal
  emulator.
*
*
* Parameters:
* cy_rslt_t state: contains the state of the network stack as defined in the
* enumeration, network_stack_state_t
*
*******************************************************************************/
static void cylpa_network_state_handler(cy_rslt_t state);

/*******************************************************************************
* Function Name: cylpa_network_act_handler_init
********************************************************************************
*
* Summary:
* In this function Callback is registered for Deep Sleep monitoring and
* can be used for lock/unlock deep sleep.Also this registers for network
* activity handling
*
* Parameters:
* void
*
* Return:
* void
*
*******************************************************************************/
static void cylpa_network_act_handler_init(void);

/******************************************************
 *               Variable Definitions
 ******************************************************/

/*  This variable is used as a check to prevent successive locking/unlocking
    of the TCP/IP stack. Locking the TCP/IP stack prevents the MCU from servicing
    the network timers. This enables the MCU to stay in deep sleep longer.
*/

static bool cylpa_s_ns_suspended;

/*  This event variable is used to alert the device if there is any network
    activity.
*/
cy_event_t cy_lp_wait_net_event;

/*  This is mutex to sychronize multiple threads */
cy_mutex_t cy_lp_mutex;

/* This variable is used to track total time spent in deep sleep */
uint32_t cy_dsleep_nw_suspend_time;

/******************************************************
 *               Function Definitions
 ******************************************************/
void cylpa_network_act_handler_init(void)
{
    /* Register for network activity callback */
    cylpa_register_network_activity_callback();
}

static void cylpa_get_idle_power_mode(char *str, uint32_t length)
{
    switch (CY_CFG_PWR_SYS_IDLE_MODE)
    {
        case CY_CFG_PWR_MODE_LP:
            strncpy(str, "LP", length);
            break;
        case CY_CFG_PWR_MODE_ULP:
            strncpy(str, "ULP", length);
            break;
        case CY_CFG_PWR_MODE_ACTIVE:
            strncpy(str, "Active", length);
            break;
        case CY_CFG_PWR_MODE_SLEEP:
            strncpy(str, "Sleep", length);
            break;
        case CY_CFG_PWR_MODE_DEEPSLEEP:
            strncpy(str, "DeepSleep", length);
            break;
        default:
            strncpy(str, "Active", length);
            break;
    }
}

static void cylpa_print_whd_bus_stats(struct netif *wifi)
{
    whd_interface_t ifp = NULL;
    if (wifi->flags & NETIF_FLAG_UP)
    {
        ifp = cy_olm_get_whd_interface();
        if (ifp == NULL)
        {
           return;
        }

        NW_INFO(("\n=====================================================\n"));
        (void)whd_print_stats(ifp->whd_driver, WHD_FALSE);
        NW_INFO(("=====================================================\n"));
    }
    else
    {
        NW_INFO(("Wi-Fi interface is not powered on, bus_stats are unavailable\n"));
    }
}

void cylpa_on_emac_activity(bool is_tx_activity)
{
    if (cy_lp_wait_net_event)
    {
        cy_rtos_setbits_event(&cy_lp_wait_net_event,
			(uint32_t)(is_tx_activity ? TX_EVENT_FLAG : RX_EVENT_FLAG), true);
    }
}

static cy_rslt_t cylpa_register_network_activity_callback(void)
{
    cy_rslt_t result = cy_rtos_init_event(&cy_lp_wait_net_event);

    if (CY_RSLT_SUCCESS != result)
    {
        NW_INFO(("Failed to initialize Wait for Network Activity event.\n"));
        return result;
    }

    result  = cy_rtos_init_mutex(&cy_lp_mutex);

    if ( CY_RSLT_SUCCESS != result )
    {
    	NW_INFO(("Failed to initialize Mutex \n"));
    	return result;
    }
    else
    {
        /* Set EMAC activity callback for this module. */
        cy_network_activity_register_cb(cylpa_on_emac_activity);
        result = CY_RSLT_SUCCESS;
    }

    return result;
}

int32_t cylpa_suspend_ns(void)
{
    int32_t state;

    if (true == cylpa_s_ns_suspended)
    {
        state = ST_BAD_STATE;
    }
    else
    {
        LOCK_TCPIP_CORE();
        cylpa_s_ns_suspended = true;
        state = ST_SUCCESS;
    }

    return state;
}

int32_t cylpa_resume_ns(void)
{
    int32_t state;

    if (false == cylpa_s_ns_suspended)
    {
        state = ST_BAD_STATE;
    }
    else
    {
        UNLOCK_TCPIP_CORE();
        cylpa_s_ns_suspended = false;
        state = ST_SUCCESS;
    }

    return state;
}

int32_t cylpa_wait_net_inactivity(uint32_t inactive_interval_ms, uint32_t inactive_window_ms)
{
    cy_time_t lp_start_time;
    cy_time_t lp_end_time;
    uint32_t state = ST_SUCCESS;

    if (inactive_interval_ms > inactive_window_ms)
    {
        /* Clear event flags to start with an initial state of no activity. */
        cy_rtos_clearbits_event(&cy_lp_wait_net_event, (uint32_t)(TX_EVENT_FLAG | RX_EVENT_FLAG), false);

         /* Start the wait timer. */
        cy_rtos_get_time( &lp_start_time);

         /* Wait until either our wait expires or the network is inactive for the period. */
        while(true)
        {
            uint32_t result;
            uint32_t elapsed_ms;
            uint32_t flags;

            /* Wait for the timeout value to expire.  This is the time we want
               to monitor for inactivity.
            */
            flags = (TX_EVENT_FLAG | RX_EVENT_FLAG);
            result = cy_rtos_waitbits_event(&cy_lp_wait_net_event, &flags, true, false, inactive_window_ms);
            if (CY_RTOS_TIMEOUT == result)
            {
                /* Inactivity wait condition met. */
                break;
            }

            cy_rtos_get_time(&lp_end_time);
            elapsed_ms = lp_end_time - lp_start_time;

            if (elapsed_ms >= inactive_interval_ms)
            {
                /* The network did not become inactive and we burned through our wait time. */
                state = ST_WAIT_INACTIVITY_TIMEOUT_EXPIRED;
                break;
            }
        }
    }
    else
    {
        state = ST_BAD_ARGS;
    }

    return state;
}

int32_t wait_net_suspend(void *net_intf, uint32_t wait_ms, uint32_t network_inactive_interval_ms,
        uint32_t network_inactive_window_ms)
{
    int32_t state;
    uint32_t result, flags;
    char idle_power_mode[IDLE_POWER_MODE_STRING_LEN];
    static bool emac_activity_callback_registered = false;
    struct netif *wifi = (struct netif *)cy_network_get_nw_interface( CY_NETWORK_WIFI_STA_INTERFACE, CY_NETWORK_WIFI_STA_INTERFACE );
    cy_time_t lp_start_time;
    cy_time_t lp_end_time;

    if (false == emac_activity_callback_registered)
    {
        cylpa_network_act_handler_init();
        emac_activity_callback_registered = true;
    }

    cyhal_syspm_lock_deepsleep();
    state = cylpa_wait_net_inactivity(network_inactive_interval_ms, network_inactive_window_ms);

    if (ST_SUCCESS == state)
    {
        /* Suspend network stack.
         * State data (e.g. caches) may be adjusted here so that the stack resumes properly.
         */
        state = cylpa_suspend_ns();
        if (ST_SUCCESS == state)
        {
            cy_rtos_clearbits_event(&cy_lp_wait_net_event, (uint32_t)(TX_EVENT_FLAG | RX_EVENT_FLAG), false);

            cylpa_get_idle_power_mode(idle_power_mode, sizeof(idle_power_mode));
            cylpa_olm_dispatch_pm_notification(cy_get_olm_instance(), OL_PM_ST_GOING_TO_SLEEP);
            NW_INFO(("\nNetwork Stack Suspended, MCU will enter %s power mode\n", idle_power_mode));
            cyhal_syspm_unlock_deepsleep();
            flags = (RX_EVENT_FLAG | TX_EVENT_FLAG);

            cy_rtos_get_time( &lp_start_time);

            /* Wait till there is emac activity. */
            result = cy_rtos_waitbits_event(&cy_lp_wait_net_event, &flags, true, false, wait_ms);
            if (CY_RTOS_TIMEOUT == result)
            {
                state = ST_WAIT_TIMEOUT_EXPIRED;
            }
            else
            {
                state = ST_NET_ACTIVITY;
            }

            cy_rtos_get_time( &lp_end_time);

            if(lp_end_time > lp_start_time)
            {
                lp_end_time = (lp_end_time-lp_start_time);
            }
            else
            {
                lp_end_time = (lp_start_time-lp_end_time);
            }

            cy_rtos_get_mutex(&cy_lp_mutex, wait_ms);

            cyhal_syspm_lock_deepsleep();
            /* Resume the network stack.
             * State data (e.g. caches) may be adjusted here so that the stack resumes properly.
            */
            NW_INFO(("Resuming Network Stack, Network stack was suspended for %ums\n",(unsigned int) lp_end_time));
            cy_dsleep_nw_suspend_time += lp_end_time;
            cylpa_print_whd_bus_stats(wifi);

            /* Update state to timeout expired even if event is set after timeout is expired as
             * xEventGroupWaitBits function returns the current bit for timeout scenario with
             * OOB disabled
             */
            if (lp_end_time > wait_ms) {
                state = ST_WAIT_TIMEOUT_EXPIRED;
            }
            cylpa_network_state_handler(state);
            cylpa_resume_ns();
            /* Call OLM API to reset back the configurations */
            cylpa_olm_dispatch_pm_notification(cy_get_olm_instance(), OL_PM_ST_AWAKE);
            cy_rtos_set_mutex(&cy_lp_mutex);
        }
    }

    cyhal_syspm_unlock_deepsleep();
    return state;
}

static void cylpa_network_state_handler(cy_rslt_t state)
{
    switch(state)
    {
        case ST_WAIT_TIMEOUT_EXPIRED:
            NW_INFO( ( "Host wait timeout expired, network did not become active.\r\n" ) );
            break;
        case ST_WAIT_INACTIVITY_TIMEOUT_EXPIRED:
            NW_INFO( ( "Host wait timeout expired, network did not become inactive.\r\n" ) );
            break;
        case ST_BAD_ARGS:
            NW_INFO( ( "Bad arguments passed for network suspension.\r\n" ) );
            break;
        case ST_BAD_STATE:
            NW_INFO( ( "Network stack state is corrupted.\r\n" ) );
            break;
        case ST_NET_ACTIVITY:
            NW_INFO( ( "Network is active. Resuming network stack\r\n" ) );
            break;
        default:
            break;
    }
}


/* This function can be used for doing TCP Keepalive verification
 *
 */
int cy_tcp_create_socket_connection( void *net_intf, void **global_socket_ptr, const char *remote_ip, uint16_t remote_port, uint16_t local_port,
                                          cy_tko_ol_cfg_t *downloaded, int socket_keepalive_enable)
{
    cy_rslt_t response = CY_RSLT_SUCCESS;
    cy_socket_sockaddr_t remote_addr = {0};
    cy_nw_ip_address_t nw_ip_addr;
    struct netif *net = NULL;
    int32_t seconds, retry_count;
#if LWIP_TCP
    uint32_t sock_handle;
    cy_socket_sockaddr_t sockaddr;
#endif

    if (net_intf == NULL )
    {
        OL_LOG_TKO(LOG_OLA_LVL_DEBUG, "Network is not up!\n");
        return CY_RSLT_TCPIP_ERROR;
    }

    int len = strlen(HARDCODED_STR);
    uint32_t bytes_sent = 0;

    net = (struct netif *)net_intf;

    cy_nw_str_to_ipv4((const char *)remote_ip, &nw_ip_addr);
    remote_addr.ip_address.version = CY_SOCKET_IP_VER_V4;
    remote_addr.ip_address.ip.v4 = nw_ip_addr.ip.v4;
    remote_addr.port = remote_port;
    sockaddr.ip_address.version = CY_SOCKET_IP_VER_V4;
    sockaddr.ip_address.ip.v4 = net->ip_addr.u_addr.ip4.addr;
    sockaddr.port = local_port;

    OL_LOG_TKO(LOG_OLA_LVL_DEBUG, "TCP remote IP Address %s  remote port:%d\n", remote_ip, remote_port );

    /* Create TCP Socket */
    response = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_STREAM, CY_SOCKET_IPPROTO_TCP, (cy_socket_t *)&sock_handle);
    if (response != CY_RSLT_SUCCESS)
    {
        OL_LOG_TKO(LOG_OLA_LVL_ERR, "TCP socket create failed ret:%ld\n", response);
        return response;
    }

    /* Bind socket to local port */
    response = cy_socket_bind((void * )sock_handle, &sockaddr, (uint32_t)sizeof(cy_socket_sockaddr_t));
    if ( CY_RSLT_SUCCESS != response)
    {
        cy_socket_delete((void *)sock_handle);
        OL_LOG_TKO(LOG_OLA_LVL_ERR, "TCP socket bind failed response: %ld\n", response);
        return response;
    }

    /* Establish TCP Connection */
    response = cy_socket_connect((void *)sock_handle, &remote_addr, (uint32_t)sizeof(cy_socket_sockaddr_t));

    if ( response != CY_RSLT_SUCCESS )
    {
        OL_LOG_TKO(LOG_OLA_LVL_ERR, "TCP connect failed to IP %s local port:%d remote port:%d err=%ld\n",
                   remote_ip, local_port, remote_port, response);
        cy_socket_delete((void *)sock_handle);
        return response;
    }

    OL_LOG_TKO(LOG_OLA_LVL_DEBUG, "Created TCP connection to IP %s, local port %d, remote port %d\n",
               remote_ip, local_port, remote_port);

#if LWIP_TCP
    if (socket_keepalive_enable)
    {
        response = cy_socket_setsockopt((void *)sock_handle, CY_SOCKET_SOL_SOCKET,
                                         CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE, &socket_keepalive_enable,
                                         sizeof(socket_keepalive_enable));

        if ( response != CY_RSLT_SUCCESS)
        {
            OL_LOG_TKO(LOG_OLA_LVL_ERR, "Host TCP keepalive enable failed %ld\n", response );
        }

        seconds = downloaded->retry_interval * 1000;
    	response = cy_socket_setsockopt((void *)sock_handle, CY_SOCKET_SOL_TCP,
                                        CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL,
                                        &seconds, sizeof(seconds));

        if ( response != CY_RSLT_SUCCESS )
        {
            OL_LOG_TKO(LOG_OLA_LVL_ERR, "TCP keepalive interval time configuration failed :%ld\n", response );
        }

        retry_count = downloaded->retry_count;
        response = cy_socket_setsockopt((void *)sock_handle, CY_SOCKET_SOL_TCP,
                                        CY_SOCKET_SO_TCP_KEEPALIVE_COUNT,
                                        &retry_count, sizeof(retry_count));

        if ( response != CY_RSLT_SUCCESS )
        {
            OL_LOG_TKO(LOG_OLA_LVL_ERR, "TCP keepalive retry count configuration failed :%ld\n", response );
        }

        seconds = downloaded->interval * 1000;
        response = cy_socket_setsockopt((void *)sock_handle, CY_SOCKET_SOL_TCP,
                                        CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME,
                                        &seconds, sizeof(seconds ));

        if ( response != CY_RSLT_SUCCESS )
        {
            OL_LOG_TKO(LOG_OLA_LVL_ERR, "TCP keepalive Idle time configuration failed :%ld\n", response );
        }
        OL_LOG_TKO(LOG_OLA_LVL_DEBUG, "setsockopt LWIP keepalive: Interval %d, Retry Interval %d, keepalive value %d\n",
                   downloaded->interval, downloaded->retry_interval, downloaded->interval);
    }
#endif

    memcpy(cy_tcp_databuf, HARDCODED_STR, len);
    response = cy_socket_send((void *)sock_handle, cy_tcp_databuf, len, NETCONN_NOFLAG, &bytes_sent);
    if (response != CY_RSLT_SUCCESS)
    {
        OL_LOG_TKO(LOG_OLA_LVL_ERR, "Could only send %u bytes on socket of request length err: %ld\n", (unsigned int)bytes_sent, response );
    }

    *global_socket_ptr = (void *)sock_handle;
    cylpa_tko_ol_update_config(remote_ip, remote_port, local_port, downloaded);
    return response;
}

/**
 * Restart olm with a new configuration
 */
int cylpa_restart_olm( ol_desc_t *offload_list , void *net_intf )
{
	olm_t *olm_ptr = (olm_t *)cy_get_olm_instance();
	const ol_desc_t *oflds_list = (const ol_desc_t *)offload_list;
	cylpa_olm_deinit(olm_ptr);
	cylpa_olm_init(olm_ptr, oflds_list);
	return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
/*
 * Find the descriptor for the given filter.
 */
ol_desc_t *cylpa_find_my_descriptor(const char *name, ol_desc_t *offload_list )
{
    ol_desc_t *oflds_list = offload_list;

    if (oflds_list == NULL)
    {
        return NULL;
    }

    /* Loop through the offload list to find by name */
    while (oflds_list && oflds_list->name && strncmp(oflds_list->name, name, strlen(name))) {
        oflds_list++;
    }
    if (!oflds_list || !oflds_list->name) {
        return NULL;
    }
    return oflds_list;
}
