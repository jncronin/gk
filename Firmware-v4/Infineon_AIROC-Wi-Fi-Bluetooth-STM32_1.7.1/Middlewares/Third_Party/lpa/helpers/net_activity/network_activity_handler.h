/*******************************************************************************
* File Name: network_activity_handler.h
*
* Version: 1.0
*
* Description: This file contains the macros, enumerations and function
* prototypes used by network_activity_handler.
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

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "cy_lpa_wifi_ol.h"
#include "cy_lpa_wifi_tko_ol.h"
#include "cy_result_mw.h"

#define NW_INFO( x )      printf x

#define RX_EVENT_FLAG     ( 1UL << 0 )
#define TX_EVENT_FLAG     ( 1UL << 1 )

#define PKT_FILTER_NAME "Pkt_Filter"
#define ARP_NAME        "ARP"
#define TKO_NAME        "TKO"

/* This enumeration enlists the different states of the network stack */
enum network_stack_state_t
{
    ST_SUCCESS,
    ST_WAIT_TIMEOUT_EXPIRED,
    ST_WAIT_INACTIVITY_TIMEOUT_EXPIRED,
    ST_WAIT_ACTIVITY_TIMEOUT_EXPIRED,
    ST_BAD_ARGS,
    ST_BAD_STATE,
    ST_CONNECT_FAILED,
    ST_DISCONNECT_FAILED,
    ST_WIFI_NET_SUSPENDED_FAILED,
    ST_WIFI_NET_RESUMING_FAILED,
    ST_NET_ACTIVITY
};

/*******************************************************************************
* Function Name: cylpa_suspend_ns
********************************************************************************
*
* Summary:
* This function suspends the network stack by taking a lock on the TCP/IP stack.
*
* Return: int32_t: contains status of suspending the network stack.
*
*******************************************************************************/
int32_t cylpa_suspend_ns(void);

/*******************************************************************************
* Function Name: cylpa_resume_ns
********************************************************************************
*
* Summary:
* This function suspends the network stack by unlocking the TCP/IP stack.
*
* Return: int32_t: contains status of resuming the network stack.
*
*******************************************************************************/
int32_t cylpa_resume_ns(void);

/*******************************************************************************
* Function Name: cylpa_on_emac_activity
********************************************************************************
*
* Summary: This is the callback that is called on detecting emac activity. It
* sets the bits of a flag indicating TX event or RX event.
*
*
* Parameters:
* bool is_tx_activity: used to determine which bit of flag is to be set. If true,
* TX bit in the flag is set. Else, RX bit is set.
*
*******************************************************************************/

void cylpa_on_emac_activity(bool is_tx_activity);

/*******************************************************************************
* Function Name: cylpa_wait_net_inactivity
********************************************************************************
*
* Summary:
* In this function the network is monitored for inactivity in an interval of
* length inactive_interval_ms. If the network is inactive for a continuous
* duration specified by inactive_window_ms, the network is declared as inactive
* and the corresponding network state value is returned.
*
* Parameters:
* uint32_t inactive_interval_ms: The interval for which the network is monitored
* for inactivity.
* uint32_t inactive_window_ms: The continuous duration for which network has to
* be inactive in inactive_interval_ms.
*
* Return:
* int32_t: contains status of network inactivity.
*
*******************************************************************************/

int32_t cylpa_wait_net_inactivity(uint32_t inactive_interval_ms, uint32_t inactive_window_ms);


/** @addtogroup lpautilities LPA Utilities API
 *  The Documentation is provided for Network Utility functions
 *  used by the LPA application to demonstrate the Low power functionality.
 * \{
 *
 *
 * \section subsection_lpa_code_snippets Code Snippets
 * \subsection subsection_lpa_snippet_1 Code Snippet 1: Using wait_net_suspend for suspend/resume network stack
 * The following code snippet demonstrates an example for Suspending Network stack to allow MCU to go to Deep Sleep.
 * LPA API \b cylpa_on_emac_activity should be called to resume network stack in order to queue packets to LwIP.
 * \snippet wifi_low_power.c snippet_cylpa_wait_net_suspend
 *
 * \subsection subsection_lpa_snippet_2 Code Snippet 2: Read OLM configuration from device configurator.
 * The following code snippet demonstrates an example for reading OLM configuration from device configurator and makes a
 * local copy of the list.
 * \snippet wifi_low_power.c snippet2_cylpa_read_olm
 *
 * \subsection subsection_lpa_snippet_3 Code Snippet 3: Search for TCP KA offload in the device configurator list.
 * The following code snippet demonstrates an example for searching the "TKO" string in the device configurator list
 * and if not present it add manual TCP KA offload configuration.
 * \snippet wifi_low_power.c snippet3_cylpa_add_tko_olm
 *
 * \subsection subsection_lpa_snippet_4 Code Snippet 4: Use cylpa_restart_olm when the OLM configuration is changed by the user application.
 * The following code snippet demonstrates an example for Restarting OLM interface with user defined configuration.
 * \snippet wifi_low_power.c snippet4_cylpa_restart_olm
 *
 * \subsection subsection_lpa_snippet_5 Code Snippet 5: Use cy_tcp_create_socket_connection to offload TCP Keep-alive to WiFi Firmware
 * The following code snippet demonstrates an example for creating TCP connection(s) and offloads TCP keep-alive when host MCU enters sleep/deepsleep via
 * wait_net_suspend API call.
 * \snippet wifi_low_power.c snippet5_cy_tcp_create_socket_connection
 *
 * \subsection subsection_lpa_snippet_6 Code Snippet 6: Read OLM configuration from device configurator.
 *  The following code snippet demonstrates an example for reading OLM configuration from device configurator and makes a
 * local copy of the list.
 * \snippet wifi_low_power.c snippet6_cylpa_read_olm
 *
 */

/** Network Monitor Function
 *
 *  In this function the network is monitored for inactivity in an interval of
 *  length inactive_interval_ms. If the network is inactive for a continuous
 *  duration specified by inactive_window_ms, the TCP/IP network stack is
 *  suspended and stays suspended until either there is emac activity or a
 *  duration of wait_ms has expired. Network stack is resumed on
 *  either detecting emac activity or if wait_ms time has elapsed.
 *
 *  @param   *net_intf             : pointer to WLAN interface
 *  @param   wait_ms               : The interval for which the network is monitored
 *                                   for activity. Network stack is resumed ether at the end of
 *                                   wait_ms or on emac activity.
 *  @param   inactive_interval_ms  : The interval for which the network is monitored
 *                                   for inactivity.
 *  @param   inactive_window_ms    : The continuous duration for which network has to
 *                                   be inactive in inactive_interval_ms.
 *
 *  @return int32_t                : Returns status on one of these cases: Network error status if the network stack suspension failed or EMAC activity status as a result of network inactivity monitor.
 */
extern int32_t   wait_net_suspend(void *net_intf,   uint32_t wait_ms, uint32_t inactive_interval_ms, uint32_t inactive_window_ms);

/** Network Monitor Function
 *
 *  This function should be used for waking up Network Stack from suspended/lock state.
 *  TCPIP Core will locked in wait_net_suspend function for putting MCU to Deep Sleep.
 *  Any task which needs to queue to LwIP should be call this function to unlock TCPIP
 *  Core.
 *
 *  @param is_tx_activity  : set event bit for TX/RX activity
 *
 *
 *  @return void
 */

extern void cylpa_on_emac_activity(bool is_tx_activity);

/** Creates TCP Socket connection
 *
 *  This function can be used as reference to create tcp socket connection to
 *  provided remote ip address, remote port, local port and the tcp socket handle is returned in *socket_handle
 *
 *  @param net_intf                : Pointer to WLAN interface
 *  @param socket_handle           : Pointer to an Array of TCP Socket Handle which is populated by this API
 *  @param remote_ip               : Pointer to Remote TCP IP Address
 *  @param remote_port             : Pointer to Remote TCP IP Port number
 *  @param local_port              : Pointer to Local TCP IP Port number
 *  @param downloaded              : Pointer to TKO offload list
 *  @param socket_keepalive_enable : Host TCP Keepalive enable 1 or 0 disable.
 *  @return int                    : Returns CY_RSLT_SUCCESS if the TCP socket connection is successful, otherwise it returns a socket error status
 */
extern int cy_tcp_create_socket_connection(void *net_intf, void **socket_handle, const char *remote_ip, uint16_t remote_port, uint16_t local_port,
    cy_tko_ol_cfg_t *downloaded, int socket_keepalive_enable);

/** Restarts OLM
 *
 *  This function restarts OLM manager with configuration passed by the caller.
 *  @note: This API should only be invoked when device is not connected to AP,
 *  if it is connected to an AP then it has to do disconnect first and then
 *  invoke this API
 *
 *  @param  offload_list           : Pointer to the user defined configuration list \ref ol_desc_t
 *  @param  net_intf               : Pointer to WLAN interface
 *  @return int                    : Returns CY_RSLT_SUCCESS if the restart is successful, otherwise it returns error.
 */
extern int cylpa_restart_olm( ol_desc_t *offload_list, void *net_intf );

/** Finds OLM descriptor for a given name
 *
 *  This function finds the OLM descriptor for a given name
 *  passed by the caller in a OLM descriptor list.
 *
 *  @param  name                   : Pointer to the string name of OLM descriptor
 *  @param  offload_list           : OLM descriptor list
 *  @return ol_desc_t              : Returns OLM descriptor matching the name, otherwise it returns NULL
 */
extern ol_desc_t *cylpa_find_my_descriptor(const char *name, ol_desc_t *offload_list );

/** Get OLM instance
 *
 *  This function returns OLM instance
 *  @return  olm_t      : Return OLM structure pointer
 *
 */
extern void *cy_get_olm_instance( void );
/** \} */
#ifdef __cplusplus
} /* extern C */
#endif
