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

#include <string.h>
#include <stdint.h>
#include "lwipopts.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "lwip/ethip6.h"
#include "lwip/igmp.h"
#include "lwip/nd6.h"
#include "netif/ethernet.h"
#include "lwip/prot/autoip.h"
#include "lwip/prot/dhcp.h"
#include "lwip/dns.h"
#include "lwip/inet_chksum.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "cy_lwip_dhcp_server.h"
#include "cy_network_mw_core.h"
#include "cy_result.h"
#include "cy_lwip_log.h"

#if defined(CYBSP_WIFI_CAPABLE)
#include "cybsp_wifi.h"
#include "cy_network_buffer.h"
#include "whd.h"
#include "whd_wifi_api.h"
#include "whd_network_types.h"
#include "whd_buffer_api.h"
#include "cy_wifimwcore_eapol.h"
#ifdef COMPONENT_4390X
#include "whd_wlioctl.h"
#endif
#endif

#if defined(CYBSP_ETHERNET_CAPABLE)
#include "cy_internal.h"

#ifdef COMPONENT_CAT3
#include "xmc_gpio.h"
#include "xmc_eth_mac.h"
#include "xmc_eth_phy.h"
#endif

#ifdef COMPONENT_CAT1
#include "cyabs_rtos.h"
#include "cy_worker_thread.h"
#endif

#endif

#if (COMPONENT_CAT1)
#include "cy_crypto_core.h"
#include "cy_crypto_core_trng_config.h"
#endif
/* While using lwIP/sockets errno is required. Since IAR and ARMC6 doesn't define errno variable, the following definition is required for building it successfully. */
#if !( (defined(__GNUC__) && !defined(__ARMCC_VERSION)) )
int errno;
#endif

/******************************************************
 *                      Macros
 ******************************************************/
/**
 * Suppress unused variable warning
 */
#define UNUSED_VARIABLE(x) ( (void)(x) )

#define EAPOL_PACKET_TYPE                        (0x888E)

#define MULTICAST_IP_TO_MAC(ip)                  {   (uint8_t) 0x01,             \
                                                     (uint8_t) 0x00,             \
                                                     (uint8_t) 0x5e,             \
                                                     (uint8_t) ((ip)[1] & 0x7F), \
                                                     (uint8_t) (ip)[2],          \
                                                     (uint8_t) (ip)[3]           \
                                                 }

#define IPV6_MULTICAST_TO_MAC_PREFIX             (0x33)

#define ARP_WAIT_TIME_IN_MSEC                    (30000)
#define ARP_CACHE_CHECK_INTERVAL_IN_MSEC         (5)

#define DHCP_IP_ADDRESS_RESOLUTION_TIMEOUT_IN_MS (15000)
#ifndef AUTO_IP_ADDRESS_RESOLUTION_TIMEOUT_IN_MS
#define AUTO_IP_ADDRESS_RESOLUTION_TIMEOUT_IN_MS (60000 * 10)
#endif
#define DCHP_RENEWAL_DELAY_IN_MS                 (100)
#define DHCP_STOP_DELAY_IN_MS                    (400)
/**
 * Maximum number of interface instances supported
 */
#define CY_IFACE_MAX_HANDLE                      (4U)

#define MAX_AUTO_IP_RETRIES                      (5)

#ifdef COMPONENT_4390X
#define CY_PRNG_SEED_FEEDBACK_MAX_LOOPS          (1000)
#define CY_PRNG_CRC32_POLYNOMIAL                 (0xEDB88320)
#define CY_PRNG_ADD_CYCLECNT_ENTROPY_EACH_N_BYTE (1024)
#define CY_PRNG_WELL512_STATE_SIZE               (16)
#endif

/* Ping definitions*/
#define PING_DATA_SIZE                           (64)
#define PING_IF_NAME_LEN                         (6)
#define PING_RESPONSE_LEN                        (64)
#define PING_ID                                  (0xAFAF)  /** ICMP identifier for ping */

#define MAX_ETHERNET_PORT                        (2U)
#define ETH_INTERFACE_INDEX                      (2U)

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1
#define CM_RX_WORKER_THREAD_PRIORITY                  (CY_RTOS_PRIORITY_HIGH)
#define CM_RX_WORKER_THREAD_STACK_SIZE                (4 * 1024)
#define CM_RX_WORKER_THREAD_QUEUE_SIZE                (32)
#define CM_TX_WORKER_THREAD_PRIORITY                  (CY_RTOS_PRIORITY_HIGH)
#define CM_TX_WORKER_THREAD_STACK_SIZE                (4 * 1024)
#define CM_TX_WORKER_THREAD_QUEUE_SIZE                (32)
#endif
#endif

#if defined(COMPONENT_CAT1)
#define MAX_TRNG_BIT_SIZE        (32UL)
#endif

/******************************************************
 *               Variable Definitions
 ******************************************************/
static cy_network_activity_event_callback_t activity_callback = NULL;
static bool is_dhcp_client_required = false;
#if defined(CYBSP_WIFI_CAPABLE)
static cy_wifimwcore_eapol_packet_handler_t internal_eapol_packet_handler = NULL;
#endif
static cy_network_ip_change_callback_t ip_change_callback = NULL;

#if LWIP_IPV4
static cy_lwip_dhcp_server_t internal_dhcp_server;
#endif

/* Interface init status */
static bool ip_networking_inited[CY_IFACE_MAX_HANDLE];
#define SET_IP_NETWORK_INITED(interface_index, status)   (ip_networking_inited[(interface_index)&3] = status)

/* Interface UP status */
static bool ip_up[CY_IFACE_MAX_HANDLE];
#define SET_IP_UP(interface_index, status)               (ip_up[(interface_index)&3] = status)

struct  netif                                           *cy_lwip_ip_handle[CY_IFACE_MAX_HANDLE];
#define LWIP_IP_HANDLE(interface)                        (cy_lwip_ip_handle[(interface) & 3])

static struct netif       sta_ip_handle;
static struct netif       ap_ip_handle;
static struct netif       eth0_ip_handle;
static struct netif       eth1_ip_handle;

struct netif* cy_lwip_ip_handle[CY_IFACE_MAX_HANDLE] =
{
    [0] =  &sta_ip_handle,
    [1] =  &ap_ip_handle,
    [2] =  &eth0_ip_handle,
    [3] =  &eth1_ip_handle
};

struct icmp_packet
{
    struct   icmp_echo_hdr hdr;
    uint8_t  data[PING_DATA_SIZE];
};

cy_network_interface_context iface_context_database[CY_IFACE_MAX_HANDLE];
static uint8_t iface_count = 0;
static uint8_t connectivity_lib_init = 0;
static bool is_tcp_initialized       = false;

#ifdef COMPONENT_4390X
static uint32_t prng_well512_state[CY_PRNG_WELL512_STATE_SIZE];
static uint32_t prng_well512_index = 0;

static uint32_t prng_add_cyclecnt_entropy_bytes = CY_PRNG_ADD_CYCLECNT_ENTROPY_EACH_N_BYTE;

/** Mutex to protect the PRNG state array */
static cy_mutex_t cy_prng_mutex;
static cy_mutex_t *cy_prng_mutex_ptr;

#endif

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1
cy_worker_thread_info_t          cy_rx_worker_thread;
cy_queue_t                       rx_input_buffer_queue;
cy_worker_thread_info_t          cy_tx_worker_thread;
cy_semaphore_t                   tx_semaphore = NULL;
cy_mutex_t                       tx_mutex;
static cy_internal_buffer_pool_t *rx_pool_handle = NULL;
uint8_t *pRx_Q_buff_pool[CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE];
#endif
#endif

#if defined(COMPONENT_CAT1)
/** mutex to protect trng count */
static cy_mutex_t trng_mutex;
#endif

/******************************************************
 *               Static Function Declarations
 ******************************************************/
#if defined(CYBSP_ETHERNET_CAPABLE)
extern err_t ethernetif_init(struct netif* netif);
#endif

#if LWIP_IPV4
static void invalidate_all_arp_entries(struct netif *netif);
#endif
static void internal_ip_change_callback (struct netif *netif);
static bool is_interface_added(uint8_t interface_index);
static cy_rslt_t is_interface_valid(cy_network_interface_context *iface);
static bool is_network_up(uint8_t interface_index);

#if LWIP_IPV4
static void ping_prepare_echo(struct icmp_packet *iecho, uint16_t len, uint16_t *ping_seq_num);
static err_t ping_send(int socket_hnd, const cy_nw_ip_address_t* address, struct icmp_packet *iecho, uint16_t *sequence_number);
static err_t ping_recv(int socket_hnd, cy_nw_ip_address_t* address, uint16_t *ping_seq_num);
#endif

#ifdef COMPONENT_4390X
static uint32_t prng_well512_get_random ( void );
static void     prng_well512_add_entropy( const void* buffer, uint16_t buffer_length );
cy_rslt_t cy_prng_get_random( void* buffer, uint32_t buffer_length );
cy_rslt_t cy_prng_add_entropy( const void* buffer, uint32_t buffer_length );
#endif

#if defined(COMPONENT_CAT1)
static int cy_trng_release( void );
static int cy_trng_reserve( void );
#endif

/******************************************************
 *               Function Definitions
 ******************************************************/

#if defined(CYBSP_WIFI_CAPABLE)
/*
 * This function takes packets from the radio driver and passes them into the
 * lwIP stack. If the stack is not initialized, or if the lwIP stack does not
 * accept the packet, the packet is freed (dropped). If the packet is of type EAPOL
 * and if the Extensible Authentication Protocol over LAN (EAPOL) handler is registered, the packet will be redirected to the registered
 * handler and should be freed by the EAPOL handler.
 */
void cy_network_process_ethernet_data(whd_interface_t iface, whd_buffer_t buf)
{
    uint8_t *data = whd_buffer_get_current_piece_data_pointer(iface->whd_driver, buf);
    uint16_t ethertype;
    struct netif *net_interface = NULL;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_INFO, "%s(): START iface->role:[%d]\n", __FUNCTION__, iface->role );

    if(iface->role == WHD_STA_ROLE)
    {
        net_interface = &sta_ip_handle;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "STA net_interface:[%p] \n", net_interface);
    }
    else if(iface->role == WHD_AP_ROLE)
    {
        net_interface = &ap_ip_handle;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AP net_interface:[%p] \n", net_interface);
    }
    else
    {
        cy_buffer_release(buf, WHD_NETWORK_RX) ;
        return;
    }

    ethertype = (uint16_t)(data[12] << 8 | data[13]);
    if (ethertype == EAPOL_PACKET_TYPE)
    {
        if( internal_eapol_packet_handler != NULL )
        {
            internal_eapol_packet_handler(iface, buf);
        }
        else
        {
            cy_buffer_release(buf, WHD_NETWORK_RX) ;
        }
    }
    else
    {
        /* Call the registered activity handler with the argument as false
         * indicating that there is RX packet
         */
        if (activity_callback)
        {
            activity_callback(false);
        }

        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Send data up to LwIP \n");
        /* If the interface is not yet set up, drop the packet */
        if (net_interface->input == NULL || net_interface->input(buf, net_interface) != ERR_OK)
        {
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Drop packet before lwip \n");
            cy_buffer_release(buf, WHD_NETWORK_RX) ;
        }
    }
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
}

/* Create a duplicate pbuf of the input pbuf */
static struct pbuf *pbuf_dup(const struct pbuf *orig)
{
    struct pbuf *p = pbuf_alloc(PBUF_LINK, orig->tot_len, PBUF_RAM);
    if (p != NULL)
    {
        pbuf_copy(p, orig);
        p->flags = orig->flags;
    }
    return p;
}

/*
 * This function takes the packets from the lwIP stack and sends them down to the radio.
 * If the radio is not ready, return and error; otherwise, add a reference to
 * the packet for the radio driver and send the packet to the radio driver. The radio
 * driver puts the packet into a send queue and sends it based on another thread. This
 * other thread will release the packet reference once the packet is actually sent.
 */
static err_t wifioutput(struct netif *iface, struct pbuf *p)
{
    cy_network_interface_context *if_ctx;
    if_ctx = (cy_network_interface_context *)iface->state;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if (whd_wifi_is_ready_to_transceive((whd_interface_t)if_ctx->hw_interface) != WHD_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Wi-Fi is not ready, packet not sent\n");
        return ERR_INPROGRESS ;
    }

    struct pbuf *whd_buf = pbuf_dup(p);
    if (whd_buf == NULL)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to allocate buffer for outgoing packet\n");
        return ERR_MEM;
    }
    /* Call the registered activity handler with the argument as true
     * indicating there is TX packet
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    whd_network_send_ethernet_data((whd_interface_t)if_ctx->hw_interface, whd_buf) ;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );

    return ERR_OK ;
}


#if LWIP_IPV4 && LWIP_IGMP
/*
 * Respond to IGMP (group management) requests
 */
static err_t igmp_filter(struct netif *iface, const ip4_addr_t *group, enum netif_mac_filter_action action)
{
    cy_network_interface_context *if_ctx;
    if_ctx = (cy_network_interface_context *)iface->state;

    whd_mac_t mac = { MULTICAST_IP_TO_MAC((uint8_t*)group) };

    switch ( action )
    {
        case NETIF_ADD_MAC_FILTER:
            if ( whd_wifi_register_multicast_address( (whd_interface_t)if_ctx->hw_interface, &mac ) != CY_RSLT_SUCCESS )
            {
                return ERR_VAL;
            }
            break;

        case NETIF_DEL_MAC_FILTER:
            if ( whd_wifi_unregister_multicast_address( (whd_interface_t)if_ctx->hw_interface, &mac ) != CY_RSLT_SUCCESS )
            {
                return ERR_VAL;
            }
            break;

        default:
            return ERR_VAL;
    }

    return ERR_OK;
}
#endif

#if LWIP_IPV6 && LWIP_IPV6_MLD
/*
 * Function called by the lwIP stack to add or delete an entry in the IPv6 multicast filter table of the Ethernet MAC.
 */
static err_t mld_mac_filter(struct netif *iface, const ip6_addr_t *group, enum netif_mac_filter_action action)
{
    whd_mac_t macaddr;
    cy_rslt_t res;
    const uint8_t *ptr = (const uint8_t *)group->addr;
    cy_network_interface_context *if_ctx;
    if_ctx = (cy_network_interface_context *)iface->state;

    /* Convert the IPv6 multicast address to the MAC address.
     * The first two octets of the converted MAC address are fixed values (0x33 and 0x33).
     * The last four octets are the last four octets of the IPv6 multicast address.
     */
    macaddr.octet[0] = IPV6_MULTICAST_TO_MAC_PREFIX ;
    macaddr.octet[1] = IPV6_MULTICAST_TO_MAC_PREFIX ;
    macaddr.octet[2] = ptr[12] ;
    macaddr.octet[3] = ptr[13] ;
    macaddr.octet[4] = ptr[14] ;
    macaddr.octet[5] = ptr[15] ;

    switch ( action )
    {
        case NETIF_ADD_MAC_FILTER:
            res = whd_wifi_register_multicast_address( (whd_interface_t)if_ctx->hw_interface, &macaddr );
            if (  res != CY_RSLT_SUCCESS )
            {
                cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_register_multicast_address call failed, err = %lx\n", res);
                return ERR_VAL;
            }
            break;

        case NETIF_DEL_MAC_FILTER:
            res = whd_wifi_unregister_multicast_address( (whd_interface_t)if_ctx->hw_interface, &macaddr );
            if ( res != CY_RSLT_SUCCESS )
            {
                cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_unregister_multicast_address call failed, err = %lx\n", res);
                return ERR_VAL;
            }
            break;

        default:
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid MAC Filter Action: %d\n", action);
            return ERR_VAL;
    }

    return ERR_OK;
}
#endif

/*
 * This function is called when adding the Wi-Fi network interface to lwIP,
 * it actually performs the initialization for the netif interface.
 */
static err_t wifiinit(struct netif *iface)
{
    cy_rslt_t res;
    cy_network_interface_context *if_ctx;
    if_ctx = (cy_network_interface_context *)iface->state;

    whd_mac_t macaddr;
    whd_interface_t whd_iface = (whd_interface_t)if_ctx->hw_interface;
#ifdef COMPONENT_4390X
    uint8_t buffer[WLC_GET_RANDOM_BYTES];
    uint32_t *wlan_rand = NULL;
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    /*
     * Set the MAC address of the interface
     */
    res = whd_wifi_get_mac_address(whd_iface, &macaddr);

    if (res != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_wifi_get_mac_address call failed, err = %lx\n", res);
        return res ;
    }
    memcpy(&iface->hwaddr, &macaddr, sizeof(macaddr));
    iface->hwaddr_len = sizeof(macaddr);

#ifdef COMPONENT_4390X
    /*
     * CYW43907 kits do not have the TRNG module. Get a random number from the WLAN and feed
     * it as a seed to the PRNG function.
     * Before invoking whd_wifi_get_iovar_buffer, WHD interface should be initialized.
     * However, wcminit is called from cy_lwip_add_interface; the WHD interface is an
     * input to the cy_lwip_add_interface API. So it is safe to invoke
     * whd_wifi_get_iovar_buffer here.
     */
    if(WHD_SUCCESS != whd_wifi_get_iovar_buffer(whd_iface, IOVAR_STR_RAND, buffer, WLC_GET_RANDOM_BYTES))
    {
        return ERR_IF;
    }
    wlan_rand = (uint32_t *)buffer;

    /* Initialize the mutex to protect the PRNG WELL512 state */
    if (cy_prng_mutex_ptr == NULL)
    {
        cy_prng_mutex_ptr = &cy_prng_mutex;
        cy_rtos_init_mutex(cy_prng_mutex_ptr);
    }
    /* Feed the random number obtained from the WLAN to the WELL512
     * algorithm as the initial seed value.
     */
    cy_prng_add_entropy((const void *)wlan_rand, 4);
#endif
    /*
     * Set up the information associated with sending packets
     */
#if LWIP_IPV4
    iface->output = etharp_output;
#endif
    iface->linkoutput = wifioutput;
    iface->mtu = WHD_LINK_MTU;
    iface->flags |= (NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP) ;
#if LWIP_IPV6_MLD
    iface->flags |= NETIF_FLAG_MLD6;
#endif
    /*
     * Set the interface name for the interface
     */
    iface->name[0] = 'w' ;
    iface->name[1] = 'l' ;

#if LWIP_IPV4 && LWIP_IGMP
    netif_set_igmp_mac_filter(iface, igmp_filter) ;
#endif


#if LWIP_IPV6 == 1
    /*
     * Filter the output packets for IPv6 through the Ethernet output
     * function for IPv6
     */
    iface->output_ip6 = ethip6_output ;

    /*
     * Automatically generate a unicast IP address based on
     * neighbor discovery
     */
    iface->ip6_autoconfig_enabled = 1 ;

    /*
     * Create a link-local IPv6 address
     */
    netif_create_ip6_linklocal_address(iface, 1);

    /*
     * Tell the radio that you want to listen to solicited-node multicast
     * packets. These packets are part of the IPv6 neighbor discovery
     * process.
     */
    macaddr.octet[0] = 0x33 ;
    macaddr.octet[1] = 0x33 ;
    macaddr.octet[2] = 0xff ;
    whd_wifi_register_multicast_address((whd_interface_t)if_ctx->hw_interface, &macaddr) ;

    /*
     * Tell the radio that you want to listen to the multicast address
     * that targets all IPv6 devices. These packets are part of the IPv6
     * neighbor discovery process.
     */
    memset(&macaddr, 0, sizeof(macaddr)) ;
    macaddr.octet[0] = 0x33 ;
    macaddr.octet[1] = 0x33 ;
    macaddr.octet[5] = 0x01 ;
    whd_wifi_register_multicast_address((whd_interface_t)if_ctx->hw_interface, &macaddr) ;

#if LWIP_IPV6_MLD
    /*
     * Register the MLD MAC filter callback function that will be called by the lwIP stack to add or delete an
     * entry in the IPv6 multicast filter table of the Ethernet MAC
     */
    netif_set_mld_mac_filter(iface, mld_mac_filter);
#endif
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return 0 ;
}
#endif

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1
static cy_rslt_t cy_buffer_pool_create(uint32_t no_of_buffers, uint32_t sizeof_buffer, cy_internal_buffer_pool_t **handle)
{
    uint16_t header_size;
    uint8_t *data_buffer = NULL;
    uint8_t *data_buffer_aligned = NULL;

    int count = 1;
    list_node_t *current_node = NULL;

    /* Allocate memory for the pool handle */
    cy_internal_buffer_pool_t *pool_handle = calloc(1, sizeof(cy_internal_buffer_pool_t));
    if (NULL == pool_handle)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Failed to allocate memory for buffer pool manager handle\n");
        return CY_RSLT_NETWORK_ERROR_NOMEM;
    }
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Buf pool handle : [%0x]\n", pool_handle);

    header_size = sizeof(list_node_t);

    /* Allocate memory for the actual buffer. Allocate additional MEM_BYTE_ALIGNMENT bytes to accommodate if the pointer returned is not 32 byte aligned */
    data_buffer = malloc((no_of_buffers * sizeof_buffer) + (header_size * no_of_buffers) + MEM_BYTE_ALIGNMENT);
    if (NULL == data_buffer)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Failed to allocate memory for actual data\n");
        free(pool_handle);
        return CY_RSLT_NETWORK_ERROR_NOMEM;
    }
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Base address of allocated buffer : [%0x] sizeof(list_node_t):[%d]\n", data_buffer, sizeof(list_node_t));

    pool_handle->total_num_buf_created = no_of_buffers;
    pool_handle->sizeof_buffer = sizeof_buffer;
    pool_handle->data_buffer = data_buffer;
    pool_handle->head = NULL;

    /* Update the allocated data_buffer pointer to be 32 byte aligned */
    data_buffer_aligned = (uint8_t*)((size_t)data_buffer + ((size_t)MEM_BYTE_ALIGNMENT - ((size_t)data_buffer & 0x1F)));
    /* Iterate over and build up the chain of buffers */
    while (count <= no_of_buffers)
    {
        list_node_t *node = (list_node_t*) data_buffer_aligned;

        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Creating buffer no [%d]....\n", count);

        node->pool_handle = pool_handle;
        node->next = NULL;
        node->buffer_ptr = data_buffer_aligned + header_size;

        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Buffer node addr : [%0x], actual buffer addr : [%0x]\n", node, node->buffer_ptr);

        /* If the first node should be added to the list, change the head pointer */
        if (pool_handle->head == NULL)
        {
            pool_handle->head = node;
            current_node = node;
        }
        else
        {
            current_node->next = node;
            current_node = node;
        }

        data_buffer_aligned += sizeof_buffer + header_size;

        count++;
    }

    *handle = pool_handle;

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_buffer_pool_get(cy_buffer_t *buffer, cy_internal_buffer_pool_t *pool_handle)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    list_node_t *list_node = NULL;

    /* If there are no free available buffer in the list, then return error */
    if (pool_handle->head == NULL)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "No available buffers");
        return CY_RSLT_NETWORK_ERROR_NOMEM;
    }
    else
    {
        list_node = pool_handle->head;
        /* Make the free available node as head */
        pool_handle->head = list_node->next;

        *buffer = (void *)list_node->buffer_ptr;
    }
    return result;
}

static void cy_buffer_pool_delete(cy_internal_buffer_pool_t *pool_handle)
{
    free(pool_handle->data_buffer);
    free(pool_handle);
    pool_handle = NULL;
}
#endif
#endif

cy_rslt_t cy_network_init( void )
{
#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1
    cy_worker_thread_params_t params;
    cy_buffer_t get_buffer = NULL;
    uint8_t *pbuffer = NULL;
    cy_rx_buffer_info_t *rx_info;
    cy_rslt_t result;
#endif
#endif
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if( connectivity_lib_init )
    {
        connectivity_lib_init++;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n Connectivity library is already initialized.\n");
        return CY_RSLT_SUCCESS;
    }

    /** Initialize TCP ip stack, LWIP init is called through tcpip_init **/
    if(!is_tcp_initialized)
    {
        /*Network stack initialization*/
        tcpip_init(NULL, NULL);
        is_tcp_initialized = true;
    }

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n tcpip_init success.\n");

    /*Memory to store iface_context. Currently, 4 contexts*/
    for (int i = 0; i < CY_IFACE_MAX_HANDLE; i++)
    {
        memset(&(iface_context_database[i]), 0, sizeof(cy_network_interface_context) );
    }

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1

    /* Create the RX worker thread */
    memset(&params, 0, sizeof(params));
    params.name = "ETH_RX_Worker";
    params.priority = CM_RX_WORKER_THREAD_PRIORITY;
    params.stack = NULL;
    params.stack_size = CM_RX_WORKER_THREAD_STACK_SIZE;
    params.num_entries = CM_RX_WORKER_THREAD_QUEUE_SIZE;

    if(cy_worker_thread_create(&cy_rx_worker_thread, &params) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "worker thread creation failed \n");
        return CY_RSLT_NETWORK_ERROR_RTOS;
    }

    if(cy_rtos_init_queue(&rx_input_buffer_queue, NO_OF_BUFFERS, sizeof(uint32_t)) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Failed to initialize queue to store input free buffers\n");
        cy_worker_thread_delete(&cy_rx_worker_thread);
        return CY_RSLT_NETWORK_ERROR_RTOS;
    }

    if(cy_buffer_pool_create(NO_OF_BUFFERS, (sizeof(cy_rx_buffer_info_t) + CY_ETH_SIZE_MAX_FRAME), &rx_pool_handle)!= CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Failed to create buffer pool\n");
        cy_rtos_deinit_queue(&rx_input_buffer_queue);
        cy_worker_thread_delete(&cy_rx_worker_thread);
        return CY_RSLT_NETWORK_ERROR_NOMEM;
    }

    /* Initialize tx mutex */
    if( cy_rtos_init_mutex(&tx_mutex) != CY_RSLT_SUCCESS )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n cy_rtos_init_mutex failed\n" );
        cy_buffer_pool_delete(rx_pool_handle);
        cy_rtos_deinit_queue(&rx_input_buffer_queue);
        cy_worker_thread_delete(&cy_rx_worker_thread);
        return CY_RSLT_NETWORK_ERROR_RTOS;
    }

    /* Initialize tx semaphore */
    if( cy_rtos_init_semaphore( &tx_semaphore, 1, 0) != CY_RSLT_SUCCESS )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n cy_rtos_init_semaphore failed\n");
        cy_rtos_deinit_mutex(&tx_mutex);
        cy_buffer_pool_delete(rx_pool_handle);
        cy_rtos_deinit_queue(&rx_input_buffer_queue);
        cy_worker_thread_delete(&cy_rx_worker_thread);
        return CY_RSLT_NETWORK_ERROR_RTOS;
    }

    /* Create a worker thread for handling TX callback events */
    memset(&params, 0, sizeof(params));
    params.name = "ETH_TX_Worker";
    params.priority = CM_TX_WORKER_THREAD_PRIORITY;
    params.stack = NULL;
    params.stack_size = CM_TX_WORKER_THREAD_STACK_SIZE;
    params.num_entries = CM_TX_WORKER_THREAD_QUEUE_SIZE;

    /* Create a worker thread */
    if(cy_worker_thread_create(&cy_tx_worker_thread, &params) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "worker thread creation failed \n");
        cy_rtos_deinit_semaphore(&tx_semaphore);
        cy_rtos_deinit_mutex(&tx_mutex);
        cy_buffer_pool_delete(rx_pool_handle);
        cy_rtos_deinit_queue(&rx_input_buffer_queue);
        cy_worker_thread_delete(&cy_rx_worker_thread);
        return CY_RSLT_NETWORK_ERROR_RTOS;
    }

    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Get all the buffers from pool and push to rx queue\n");

    for(int i = 0; i < rx_pool_handle->total_num_buf_created; i++)
    {
        /* Get the buffer from pool */
        if(cy_buffer_pool_get(&get_buffer, rx_pool_handle)!= CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Failed to get the buffer from pool\n");
            result = CY_RSLT_NETWORK_ERROR_NOMEM;
            goto exit;
        }

        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Buffer from pool : [%p]\n", get_buffer);

        /* Push the free buffer to the queue */
        if(cy_rtos_put_queue(&rx_input_buffer_queue, &get_buffer, CY_RTOS_NEVER_TIMEOUT, false) != CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Failed to push the free buffer to queue\n");
            result = CY_RSLT_NETWORK_ERROR_RTOS;
            goto exit;
        }
    }

    for(int i = 0; i < CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE; i++)
    {
        if(cy_rtos_get_queue(&rx_input_buffer_queue, (void *)&pbuffer, 0, true) != CY_RSLT_SUCCESS)
        {
            /* Failed to get a free buffer from the queue. */
            result = CY_RSLT_NETWORK_ERROR_RTOS;
            goto exit;
        }

        rx_info = (cy_rx_buffer_info_t *)pbuffer;
        rx_info->rx_data_ptr = (uint8_t *)(pbuffer + sizeof(cy_rx_buffer_info_t));

        pRx_Q_buff_pool[i] = (uint8_t *)&rx_info->rx_data_ptr;
    }
#endif
#endif

#if defined(COMPONENT_CAT1)
    if( cy_rtos_init_mutex(&trng_mutex) != CY_RSLT_SUCCESS )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n cy_rtos_init_mutex failed\n" );
#if defined(CYBSP_ETHERNET_CAPABLE)
        result = CY_RSLT_NETWORK_ERROR_NOMEM;
        goto exit;
#else
        return CY_RSLT_NETWORK_ERROR_NOMEM;
#endif
    }
#endif
    connectivity_lib_init++;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1

exit:
    cy_worker_thread_delete(&cy_tx_worker_thread);
    cy_rtos_deinit_semaphore(&tx_semaphore);
    cy_rtos_deinit_mutex(&tx_mutex);
    cy_buffer_pool_delete(rx_pool_handle);
    cy_rtos_deinit_queue(&rx_input_buffer_queue);
    cy_worker_thread_delete(&cy_rx_worker_thread);
    return result;

#endif
#endif
}

cy_rslt_t cy_network_deinit( void )
{

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if( connectivity_lib_init == 0 )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG,"\n Connectivity library is De-initialized.\n");
        return CY_RSLT_SUCCESS;
    }
    else
    {
        connectivity_lib_init--;
        if(connectivity_lib_init == 0)
        {

#if defined(CYBSP_ETHERNET_CAPABLE)
#ifdef COMPONENT_CAT1
            /* Delete the rx worker thread */
            cy_worker_thread_delete(&cy_rx_worker_thread);

            /* Delete the rx queue */
            cy_rtos_deinit_queue(&rx_input_buffer_queue);

            /* Delete the rx pool */
            cy_buffer_pool_delete(rx_pool_handle);

            /* Delete the tx worker thread */
            cy_worker_thread_delete(&cy_tx_worker_thread);

            /* Delete the tx semaphore */
            cy_rtos_deinit_semaphore(&tx_semaphore);

            /* Delete the tx mutex */
            cy_rtos_deinit_mutex(&tx_mutex);

#endif
#endif

#ifdef COMPONENT_CAT1
            cy_rtos_deinit_mutex(&trng_mutex);
#endif
        }
    }

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_add_nw_interface(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx,
                                         void *hw_interface, uint8_t *mac_address,
                                         cy_network_static_ip_addr_t *static_ipaddr,
                                         cy_network_interface_context **iface_context)
{
    bool iface_found = false;
    uint8_t index = 0;
#if LWIP_IPV4
    ip4_addr_t ipaddr, netmask, gateway ;
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if (iface_context == NULL || hw_interface == NULL || (iface_idx >= MAX_ETHERNET_PORT))
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Bad arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (iface_count >= CY_IFACE_MAX_HANDLE)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding interface \n");
        return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
    }

    /* Static IP address is mandatory for AP */
    if((iface_type == CY_NETWORK_WIFI_AP_INTERFACE) && (static_ipaddr == NULL))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error static IP addr cannot be NULL for AP interface \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /*Check if the interface is already added*/
    for (int i = 0; i < CY_IFACE_MAX_HANDLE; i++)
    {
        if( ( iface_context_database[i].is_initialized == true ) &&
            ( iface_context_database[i].iface_type == iface_type ) &&
            ( iface_context_database[i].iface_idx == iface_idx ) )
        {
           iface_found = true;
           *iface_context = &(iface_context_database[i]);

           break;
        }
    }

    if(iface_found == true)
    {
        iface_idx = ((iface_type == CY_NETWORK_ETH_INTERFACE)? (CY_NETWORK_ETH_INTERFACE + iface_idx) : iface_type);
        if (is_interface_added((uint8_t)iface_idx))
        {
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Interface already exists \n");
            return CY_RSLT_NETWORK_INTERFACE_EXISTS;
        }
    }
    else
    {
        /*No duplicate; check for an available index and update the interface database*/
        for( index = 0; index < CY_IFACE_MAX_HANDLE; index++ )
        {
            if( iface_context_database[index].is_initialized == false )
            {
                iface_context_database[index].iface_type = iface_type;
                iface_context_database[index].iface_idx  = iface_idx;

                iface_context_database[index].hw_interface = hw_interface;
                cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG," Adding interface \n");

                *iface_context = &(iface_context_database[index]);
                break;
            }
        }

        /* Handling index out of bound exception */
        if( index == 4 )
        {
            return CY_RSLT_NETWORK_BAD_ARG;
        }
    }

#if LWIP_IPV4
    /* Assign the IP address if static; otherwise, zero the IP address */
    if (static_ipaddr != NULL)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "static_ipaddr is NOT NULL \n");

#ifdef ENABLE_CONNECTIVITY_MIDDLEWARE_LOGS
#define IPV4_MAX_STR_LEN 15
    char ip_str[IPV4_MAX_STR_LEN];
    cy_nw_ntoa(&(static_ipaddr->addr), ip_str);
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "static ipaddr assigned %s assigned \n", ip_str);
    cy_nw_ntoa(&(static_ipaddr->gateway), ip_str);
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "static gateway assigned %s assigned \n", ip_str);
    cy_nw_ntoa(&(static_ipaddr->netmask), ip_str);
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "static netmask assigned %s assigned \n", ip_str);
#endif

        memcpy(&gateway, &static_ipaddr->gateway.ip.v4, sizeof(gateway));
        memcpy(&ipaddr, &static_ipaddr->addr.ip.v4, sizeof(ipaddr));
        memcpy(&netmask, &static_ipaddr->netmask.ip.v4, sizeof(netmask));
    }
    else
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "static_ipaddr is NULL \n");
        memset(&gateway, 0, sizeof(gateway));
        memset(&ipaddr, 0, sizeof(ipaddr));
        memset(&netmask, 0, sizeof(netmask));
    }
#endif

    /* update netif structure */
#if defined(CYBSP_WIFI_CAPABLE)
    if( CY_NETWORK_WIFI_STA_INTERFACE == iface_context_database[index].iface_type || CY_NETWORK_WIFI_AP_INTERFACE == iface_context_database[index].iface_type )
    {
        /* Assign network instance to the iface database */
        iface_context_database[index].nw_interface = LWIP_IP_HANDLE(iface_context_database[index].iface_type);
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG,"iface_context_database[index].nw_interface:[%p]\n", iface_context_database[index].nw_interface);

        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG,"iface_context_database[index].hw_interface:[%p] hw_interface:[%p]\n", iface_context_database[index].hw_interface, hw_interface);
#if LWIP_IPV4
        /* Add the interface to the lwIP stack and make it the default */
        if(netifapi_netif_add(iface_context_database[index].nw_interface, &ipaddr, &netmask, &gateway, &iface_context_database[index], wifiinit, tcpip_input) != CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding interface \n");
            return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
        }
#else
        if(netifapi_netif_add(iface_context_database[index].nw_interface, &iface_context_database[index], wifiinit, tcpip_input) != CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding interface \n");
            return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
        }
#endif
    }

    if(iface_context_database[index].iface_type == CY_NETWORK_WIFI_STA_INTERFACE)
    {
        if(static_ipaddr == NULL)
        {
            is_dhcp_client_required = true;
        }
        netifapi_netif_set_default(iface_context_database[index].nw_interface);
    }

#if LWIP_NETIF_STATUS_CALLBACK == 1
    /*
     * Register a handler for any address changes.
     * Note: The "status" callback will also be called when the interface
     * goes up or down.
     */
    netif_set_status_callback(iface_context_database[index].nw_interface, internal_ip_change_callback);
#endif /* LWIP_NETIF_STATUS_CALLBACK */

    SET_IP_NETWORK_INITED(iface_context_database[index].iface_type, true);

#endif

#if defined(CYBSP_ETHERNET_CAPABLE)
    if( CY_NETWORK_ETH_INTERFACE == iface_context_database[index].iface_type)
    {
        /* Update the MAC address to iface database */
        memcpy((uint8_t *)&(iface_context_database[index].mac_address[0]), (uint8_t *)mac_address, CY_MAC_ADDR_LEN);

        /* Assign the network instance to the iface database */
        iface_context_database[index].nw_interface = LWIP_IP_HANDLE(iface_context_database[index].iface_type + iface_context_database[index].iface_idx);

#if LWIP_IPV4
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Add interface : [%p]for the netif assigned\n", iface_context_database[index].nw_interface);

        /* Add network interface to the netif_list */
        if(netifapi_netif_add(iface_context_database[index].nw_interface, &ipaddr, &netmask, &gateway, &iface_context_database[index], &ethernetif_init, &tcpip_input) != CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding interface \n");
            return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
        }
#else
        if(netifapi_netif_add(iface_context_database[index].nw_interface, &iface_context_database[index], &ethernetif_init, &tcpip_input) != CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding interface \n");
            return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
        }
#endif
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG,"iface_context_database[index].nw_interface:[%p]\n", iface_context_database[index].nw_interface);

        /*  Register the default network interface.*/
        netifapi_netif_set_default(iface_context_database[index].nw_interface);
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG,"Set default Interface:[%p]\n", LWIP_IP_HANDLE(iface_context_database[index].iface_type + iface_context_database[index].iface_idx));

        /* If callback enabled */
#if LWIP_NETIF_STATUS_CALLBACK == 1
        /* Initialize the interface status change callback */
        netif_set_status_callback(iface_context_database[index].nw_interface, internal_ip_change_callback);
#endif
        SET_IP_NETWORK_INITED((iface_context_database[index].iface_type + iface_context_database[index].iface_idx), true);
    }

    if(iface_context_database[index].iface_type == CY_NETWORK_ETH_INTERFACE)
    {
        if(static_ipaddr == NULL)
        {
            is_dhcp_client_required = true;
        }
    }

#endif


    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "added network net_interface:[%p] \n", iface_context_database[index].nw_interface);

    /* Update the interface initialized flag only after netif is added. This flag will be referred in Rx data handler */
    iface_context_database[index].is_initialized = true;

    /*Update the valid interface count: Used as max cap for interface addition*/

    iface_count++;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

void* cy_network_get_nw_interface(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx)
{
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if( ((iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_type != CY_NETWORK_WIFI_AP_INTERFACE) && (iface_type != CY_NETWORK_ETH_INTERFACE))
        || (iface_idx >= MAX_ETHERNET_PORT) )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return NULL;
    }

    for (int i = 0; i < CY_IFACE_MAX_HANDLE; i++)
    {
        if( ( iface_context_database[i].is_initialized == true ) &&
            ( iface_context_database[i].iface_type == iface_type ) &&
            ( iface_context_database[i].iface_idx == iface_idx ) )
        {
            return (void *)(iface_context_database[i].nw_interface);
        }
    }

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return NULL;
}

cy_rslt_t cy_network_remove_nw_interface(cy_network_interface_context *iface_context)
{
    uint8_t interface_index;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if(is_interface_valid(iface_context) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    interface_index = (uint8_t)((iface_context->iface_type == CY_NETWORK_ETH_INTERFACE)? (CY_NETWORK_ETH_INTERFACE + iface_context->iface_idx) : iface_context->iface_type);

    /* Check any one interface is added */
    if(!iface_count)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error Not added \n");
        return CY_RSLT_NETWORK_INTERFACE_DOES_NOT_EXIST;
    }

    /* Interface can be removed only if the interface was previously added and the network is down */
    if(!is_interface_added(interface_index))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error Interface doesn't exist \n");
        return CY_RSLT_NETWORK_INTERFACE_DOES_NOT_EXIST;
    }

    if(is_network_up(interface_index))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error removing interface, bring down the network before removing the interface \n");
        return CY_RSLT_NETWORK_ERROR_REMOVING_INTERFACE;
    }

    /* Remove the status callback */
    netif_set_remove_callback(LWIP_IP_HANDLE(interface_index), internal_ip_change_callback);
    /* Remove the interface */
    netifapi_netif_remove(LWIP_IP_HANDLE(interface_index));
    if(iface_context->iface_type == CY_NETWORK_WIFI_STA_INTERFACE || iface_context->iface_type == CY_NETWORK_ETH_INTERFACE)
    {
        is_dhcp_client_required = false;
    }

    SET_IP_NETWORK_INITED(interface_index, false);

#ifdef COMPONENT_4390X
    /* cy_prng_mutex_ptr is initialized when the first network interface is initialized.
     * Deinitialize the mutex only after all the interfaces are deinitialized.
     */
    if (!ip_networking_inited[CY_NETWORK_WIFI_STA_INTERFACE] &&
        !ip_networking_inited[CY_NETWORK_WIFI_AP_INTERFACE])
    {
        if (cy_prng_mutex_ptr != NULL)
        {
            cy_rtos_deinit_mutex(cy_prng_mutex_ptr);
            cy_prng_mutex_ptr = NULL;
        }
    }
#endif

    /* Clear interface details from database */
    for (int i = 0; i < CY_IFACE_MAX_HANDLE; i++)
    {
        if( ( iface_context_database[i].iface_type == iface_context->iface_type ) &&
            ( iface_context_database[i].iface_idx == iface_context->iface_idx ) )
        {
            iface_context_database[i].is_initialized = false;
            iface_context_database[i].nw_interface = NULL;
            iface_context_database[i].hw_interface = NULL;
        }
    }

    /*Update the valid interface count: Used as max cap for interface addition*/
    iface_count--;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_ip_up(cy_network_interface_context *iface)
{
    cy_rslt_t result                     = CY_RSLT_SUCCESS;
    uint8_t   interface_index;
#if LWIP_IPV4
    uint32_t    address_resolution_timeout = 0;
    bool        timeout_occurred = false;
    ip4_addr_t  ip_addr;
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if(is_interface_valid(iface) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    interface_index = (uint8_t)((iface->iface_type == CY_NETWORK_ETH_INTERFACE)? (CY_NETWORK_ETH_INTERFACE + iface->iface_idx) : iface->iface_type);
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "interface_index:[%d] \n", interface_index);

#ifdef COMPONENT_CAT3
    if(!netif_is_up(LWIP_IP_HANDLE(interface_index)))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Link is not up \n");
        return CY_RSLT_NETWORK_LINK_NOT_UP;
    }
#endif

    if(is_network_up(interface_index))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Network is already up \n");
        return CY_RSLT_SUCCESS;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the lwIP APIs that requires TCP core lock
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /*
    * Bring up the network interface
    */
    netifapi_netif_set_up((LWIP_IP_HANDLE(interface_index)));

    /*
    * Bring up the network link layer
    */
    netifapi_netif_set_link_up((LWIP_IP_HANDLE(interface_index)));

#if LWIP_IPV6
    /* Wait for the IPv6 address to change from tentative to valid or invalid */
    while(ip6_addr_istentative(netif_ip6_addr_state(LWIP_IP_HANDLE(interface_index), 0)))
    {
        /* Give the lwIP stack time to change the state */
        cy_rtos_delay_milliseconds(ND6_TMR_INTERVAL);
    }

    /* lwIP changes state to either INVALID or VALID. Check if the state is VALID */
    if(ip6_addr_isvalid(netif_ip6_addr_state(LWIP_IP_HANDLE(interface_index), 0)))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPv6 Network ready IP: %s \r\n", ip6addr_ntoa(netif_ip6_addr(LWIP_IP_HANDLE(interface_index), 0)));
    }
    else
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPv6 network not ready \r\n");
    }
#endif

#if LWIP_IPV4
    if(iface->iface_type == CY_NETWORK_WIFI_STA_INTERFACE || iface->iface_type == CY_NETWORK_ETH_INTERFACE)
    {
        if(is_dhcp_client_required)
        {
            /* TO DO :  Save the current power save state */
            /* TO DO :  Disable power save for the DHCP exchange */

            /*
             * For DHCP only, reset netif IP address
             *to avoid reusing the previous netif IP address
             * given from the previous DHCP session.
             */
            ip4_addr_set_zero(&ip_addr);

            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack
             * before invoking the lwIP APIs that require the TCP core lock.
             */
            if (activity_callback)
            {
                activity_callback(true);
            }

            netif_set_ipaddr(LWIP_IP_HANDLE(interface_index), &ip_addr);

            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack
             * before invoking the lwIP APIs that require the TCP core lock.
             */
            if (activity_callback)
            {
                activity_callback(true);
            }

            /* TO DO : DHCPv6 need to be handled when we support IPV6 addresses other than the link local address */
            /* Start DHCP */
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Start DHCP client netif:[%p]\n", LWIP_IP_HANDLE(interface_index) );
            if(netifapi_dhcp_start(LWIP_IP_HANDLE(interface_index)) != CY_RSLT_SUCCESS)
            {
                cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "CY_RSLT_NETWORK_ERROR_STARTING_DHCP error\n");
                return CY_RSLT_NETWORK_ERROR_STARTING_DHCP;
            }
            /* Wait a little to allow DHCP to complete */

            while((netif_dhcp_data(LWIP_IP_HANDLE(interface_index))->state != DHCP_STATE_BOUND) && (timeout_occurred == false))
            {
                cy_rtos_delay_milliseconds(10);
                address_resolution_timeout += 10;
                if(address_resolution_timeout >= DHCP_IP_ADDRESS_RESOLUTION_TIMEOUT_IN_MS)
                {
                    /* Timeout has occurred */
                    timeout_occurred = true;
                }
            }

            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netif_dhcp_data(LWIP_IP_HANDLE(interface_index))->state:[%d]\n",netif_dhcp_data(LWIP_IP_HANDLE(interface_index))->state);
            if (timeout_occurred)
            {
                /*
                 * If LPA is enabled, invoke the activity callback to resume the network stack
                 * before invoking the lwIP APIs that require the TCP core lock.
                 */
                if (activity_callback)
                {
                    activity_callback(true);
                }
                netifapi_dhcp_release_and_stop(LWIP_IP_HANDLE(interface_index));
                cy_rtos_delay_milliseconds(DHCP_STOP_DELAY_IN_MS);

                /*
                * If LPA is enabled, invoke the activity callback to resume the network stack
                * before invoking the lwIP APIs that require the TCP core lock.
                */
                if (activity_callback)
                {
                    activity_callback(true);
                }

                dhcp_cleanup(LWIP_IP_HANDLE(interface_index));
#if LWIP_AUTOIP
                int   tries = 0;
                cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Unable to obtain IP address via DHCP. Perform Auto IP\n");
                address_resolution_timeout = 0;
                timeout_occurred            = false;

                /*
                 * If LPA is enabled, invoke the activity callback to resume the network stack
                 * before invoking the lwIP APIs that require the TCP core lock.
                 */
                if (activity_callback)
                {
                    activity_callback(true);
                }

                if (autoip_start(LWIP_IP_HANDLE(interface_index) ) != ERR_OK )
                {
                    /* trick: Skip the while-loop, do the cleaning up stuff */
                    timeout_occurred = true;
                }

                while ((timeout_occurred == false) && ((netif_autoip_data(LWIP_IP_HANDLE(interface_index))->state != AUTOIP_STATE_BOUND)))
                {
                    cy_rtos_delay_milliseconds(10);
                    address_resolution_timeout += 10;
                    if(address_resolution_timeout >= AUTO_IP_ADDRESS_RESOLUTION_TIMEOUT_IN_MS)
                    {
                        if(tries++ < MAX_AUTO_IP_RETRIES)
                        {
                            address_resolution_timeout = 0;
                        }
                        else
                        {
                            timeout_occurred = true;
                        }
                     }
                }

                if (timeout_occurred)
                {
                    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to obtain IP address via DCHP and AutoIP\n");

                    /*
                     * If LPA is enabled, invoke the activity callback to resume the network stack
                     * before invoking the lwIP APIs that require the TCP core lock.
                     */
                    if (activity_callback)
                    {
                        activity_callback(true);
                    }

                    autoip_stop(LWIP_IP_HANDLE(interface_index));
                    return CY_RSLT_NETWORK_DHCP_WAIT_TIMEOUT;
                }
                else
                {
                    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "IP address obtained through AutoIP \n");
                }
#else
                cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "CY_RSLT_NETWORK_DHCP_WAIT_TIMEOUT error\n");
                return CY_RSLT_NETWORK_DHCP_WAIT_TIMEOUT;
#endif
            }
        }
    }
    else
    {
        memset(&internal_dhcp_server, 0, sizeof(internal_dhcp_server));
#if LWIP_IGMP
        igmp_start(LWIP_IP_HANDLE(interface_index));
#endif
        /* Start the internal DHCP server */
        if((result = cy_lwip_dhcp_server_start(&internal_dhcp_server, iface))!= CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to obtain IP address via DHCP\n");
            return CY_RSLT_NETWORK_ERROR_STARTING_DHCP;
        }
    }
#endif

    SET_IP_UP(interface_index, true);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return result;
}

cy_rslt_t cy_network_ip_down(cy_network_interface_context *iface)
{
    uint8_t interface_index;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if(is_interface_valid(iface) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    interface_index = (uint8_t)((iface->iface_type == CY_NETWORK_ETH_INTERFACE)? (CY_NETWORK_ETH_INTERFACE + iface->iface_idx) : iface->iface_type);

    if(!is_network_up((uint8_t)interface_index))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Network is not UP \r\n");
        return CY_RSLT_NETWORK_INTERFACE_NETWORK_NOT_UP;
    }

#if LWIP_IPV4
    if(is_dhcp_client_required)
    {
#if LWIP_AUTOIP
        if(netif_autoip_data(LWIP_IP_HANDLE(interface_index))->state == AUTOIP_STATE_BOUND)
        {
            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack
             * before invoking the lwIP APIs that require the TCP core lock.
             */
            if (activity_callback)
            {
                activity_callback(true);
            }

            autoip_stop(LWIP_IP_HANDLE(interface_index));
        }
        else
#endif
        {
            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack
             * before invoking the lwIP APIs that require the TCP core lock.
             */
            if (activity_callback)
            {
                activity_callback(true);
            }

            netifapi_dhcp_release_and_stop(LWIP_IP_HANDLE(interface_index));
            cy_rtos_delay_milliseconds(DHCP_STOP_DELAY_IN_MS);

            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack
             * before invoking the lwIP APIs that require the TCP core lock.
             */
            if (activity_callback)
            {
                activity_callback(true);
            }

            dhcp_cleanup(LWIP_IP_HANDLE(interface_index));
        }
    }

    if(iface->iface_type == CY_NETWORK_WIFI_AP_INTERFACE)
    {

        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Stop DHCP for SoftAP interface \n" );
        /* Stop the internal DHCP server for the SoftAP interface */
        cy_lwip_dhcp_server_stop(&internal_dhcp_server);
    }
#endif

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack
     * before invoking the lwIP APIs that require the TCP core lock.
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /*
    * Bring down the network link layer
    */
    netifapi_netif_set_link_down(LWIP_IP_HANDLE(interface_index));

    /*
    * Bring down the network interface
    */
    netifapi_netif_set_down(LWIP_IP_HANDLE(interface_index));

    /* TO DO : clear all ARP cache */

    /** TO DO:
     *  Kick the radio chip if it is in power save mode if the link down event is due to missing beacons.
     *  Setting the chip to the same power save mode is sufficient.
     */
    SET_IP_UP(interface_index, false);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

void cy_network_register_ip_change_cb(cy_network_interface_context *iface, cy_network_ip_change_callback_t cb, void *user_data)
{
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    /* Assign the callback arguments */
    ip_change_callback = cb;
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
}

/*
 * This function helps to register/deregister the callback for network activity
 */
void cy_network_activity_register_cb(cy_network_activity_event_callback_t cb)
{
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    /* Update the activity callback with the argument passed */
    activity_callback = cb;
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );}

/*
 * This function notifies the network activity to the LPA module.
 */
cy_rslt_t cy_network_activity_notify(cy_network_activity_type_t activity_type)
{

    if(activity_callback)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
        if(activity_type == CY_NETWORK_ACTIVITY_TX)
        {
            activity_callback(true);
        }
        else if(activity_type == CY_NETWORK_ACTIVITY_RX)
        {
            activity_callback(false);
        }
        else
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid network activity type\n");
            return CY_RSLT_NETWORK_BAD_ARG;
        }
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_get_ip_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *ip_addr)
{
#if LWIP_IPV4
    struct netif *net_interface  = NULL;
    uint32_t ipv4_addr;
    ip4_addr_t* addr = NULL;

    UNUSED_VARIABLE(addr);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if((ip_addr == NULL) || (is_interface_valid(iface_context) != CY_RSLT_SUCCESS))
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    net_interface = (struct netif *)iface_context->nw_interface;

#if LWIP_IPV6
        ipv4_addr = net_interface->ip_addr.u_addr.ip4.addr;
        addr = &net_interface->ip_addr.u_addr.ip4;
#else
        ipv4_addr = net_interface->ip_addr.addr;
        addr = &net_interface->ip_addr;
#endif

    ip_addr->version = NW_IP_IPV4;
    ip_addr->ip.v4 = ipv4_addr;
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IP Address %s assigned \n", ip4addr_ntoa(addr));

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV4 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_ipv6_address(cy_network_interface_context *iface_context, cy_network_ipv6_type_t type, cy_nw_ip_address_t *ip_addr)
{
#if LWIP_IPV6
    struct netif *net_interface  = NULL;
    const ip6_addr_t* ipv6_addr  = NULL;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if((ip_addr == NULL) || (is_interface_valid(iface_context) != CY_RSLT_SUCCESS))
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    net_interface = (struct netif *)iface_context->nw_interface;

    ipv6_addr = netif_ip6_addr(net_interface, 0);
    if(ipv6_addr != NULL)
    {
        ip_addr->version = NW_IP_IPV6;
        ip_addr->ip.v6[0] = ipv6_addr->addr[0];
        ip_addr->ip.v6[1] = ipv6_addr->addr[1];
        ip_addr->ip.v6[2] = ipv6_addr->addr[2];
        ip_addr->ip.v6[3] = ipv6_addr->addr[3];

        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV6 Address %s assigned \n", ip6addr_ntoa(netif_ip6_addr(net_interface, 0)));
    }
    else
    {
        memset(ip_addr, 0, sizeof(cy_nw_ip_address_t));
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready \n");
        return CY_RSLT_NETWORK_IPV6_INTERFACE_NOT_READY;
    }

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV6 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_gateway_ip_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *gateway_addr)
{
#if LWIP_IPV4
    struct netif *net_interface  = NULL;
    uint32_t ipv4_addr;
    ip4_addr_t* addr = NULL;

    UNUSED_VARIABLE(addr);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if((gateway_addr == NULL) || (is_interface_valid(iface_context) != CY_RSLT_SUCCESS))
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    net_interface = (struct netif *)(iface_context->nw_interface);

#if LWIP_IPV6
    ipv4_addr = net_interface->gw.u_addr.ip4.addr;
    addr = &net_interface->gw.u_addr.ip4;
#else
    ipv4_addr = net_interface->gw.addr;
    addr = &net_interface->gw;
#endif

    gateway_addr->version = NW_IP_IPV4;
    gateway_addr->ip.v4 = ipv4_addr;
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Gateway IP Address %s assigned \n", ip4addr_ntoa(addr));

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV4 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_gateway_mac_address(cy_network_interface_context *iface_context, cy_nw_ip_mac_t *mac_addr)
{
#if LWIP_IPV4
    err_t err;
    cy_nw_ip_address_t gateway_ip_addr;
    struct eth_addr *eth_ret = NULL;
    const ip4_addr_t *ip_ret = NULL;
    int32_t arp_waittime = ARP_WAIT_TIME_IN_MSEC;
    ssize_t arp_index = -1;
    ip4_addr_t ipv4addr;
    struct netif *net_interface  = NULL;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if((mac_addr == NULL) || (is_interface_valid(iface_context) != CY_RSLT_SUCCESS))
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    net_interface = (struct netif *)(iface_context->nw_interface);

    if (cy_network_get_gateway_ip_address(iface_context, &gateway_ip_addr) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_network_get_gateway_ip_address failed\n" );
        return  CY_RSLT_NETWORK_ERROR_GET_MAC_ADDR;
    }

    ipv4addr.addr = gateway_ip_addr.ip.v4;

    /* Check if the gateway address entry is already present in the ARP cache */
    arp_index = etharp_find_addr(net_interface, (const ip4_addr_t *) &ipv4addr, &eth_ret, (const ip4_addr_t **) &ip_ret);
    if(arp_index == -1)
    {
        /* Address entry is not present in the ARP cache. Send the ARP request.*/
        err = etharp_request(net_interface, (const ip4_addr_t *) &ipv4addr);
        if(err != ERR_OK)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "etharp_request failed with error %d\n", err);
            return  CY_RSLT_NETWORK_ERROR_GET_MAC_ADDR;
        }

        do
        {
            arp_index = etharp_find_addr(net_interface, (const ip4_addr_t *) &ipv4addr, &eth_ret, (const ip4_addr_t **) &ip_ret);
            if(arp_index != -1)
            {
                 cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "arp entry found \r\n");
                 break;
            }
            cy_rtos_delay_milliseconds(ARP_CACHE_CHECK_INTERVAL_IN_MSEC);
            arp_waittime -= ARP_CACHE_CHECK_INTERVAL_IN_MSEC;
            if(arp_waittime <= 0)
            {
                cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Could not resolve MAC address for the given destination address \r\n");
                return CY_RSLT_NETWORK_ERROR_GET_MAC_ADDR;
            }
        } while(1);

    }

    memcpy(mac_addr, eth_ret->addr, CY_MAC_ADDR_LEN);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV4 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_netmask_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *net_mask_addr)
{
#if LWIP_IPV4
    struct netif *net_interface  = NULL;
    uint32_t ipv4_addr;
    ip4_addr_t* addr = NULL;

    UNUSED_VARIABLE(addr);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if((net_mask_addr == NULL) || (is_interface_valid(iface_context) != CY_RSLT_SUCCESS))
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    net_interface = (struct netif *)(iface_context->nw_interface);

#if LWIP_IPV6
    ipv4_addr = net_interface->netmask.u_addr.ip4.addr;
    addr = &net_interface->netmask.u_addr.ip4;
#else
    ipv4_addr = net_interface->netmask.addr;
    addr = &net_interface->netmask;
#endif

    net_mask_addr->version = NW_IP_IPV4;
    net_mask_addr->ip.v4 = ipv4_addr;
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "net mask IP Address %s assigned \n", ip4addr_ntoa(addr));

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV4 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_dhcp_renew(cy_network_interface_context *iface)
{
#if LWIP_IPV4
    uint8_t interface_index;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if(is_interface_valid(iface) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    interface_index = (uint8_t)((iface->iface_type == CY_NETWORK_ETH_INTERFACE)? (CY_NETWORK_ETH_INTERFACE + iface->iface_idx) : iface->iface_type);

    /* Invalidate ARP entries */
    netifapi_netif_common(LWIP_IP_HANDLE(interface_index), (netifapi_void_fn) invalidate_all_arp_entries, NULL );


    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack
     * before invoking the lwIP APIs that require the TCP core lock.
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /* Renew DHCP */
    netifapi_netif_common(LWIP_IP_HANDLE(interface_index), NULL, dhcp_renew);

    cy_rtos_delay_milliseconds(DCHP_RENEWAL_DELAY_IN_MS);

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV4 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

/**
 * Remove all ARP table entries of the specified netif.
 * @param netif Points to a network interface
 */
#if LWIP_IPV4
static void invalidate_all_arp_entries(struct netif *netif)
{
     /*Free all entries in ARP list */
    etharp_cleanup_netif(netif);
}
#endif

cy_rslt_t cy_network_ping(void *iface_context, cy_nw_ip_address_t *address, uint32_t timeout_ms, uint32_t* elapsed_time_ms)
{
#if LWIP_IPV4
    cy_time_t send_time;
    cy_time_t recvd_time;
    err_t err;
    struct timeval timeout_val;
    struct icmp_packet ping_packet;
    uint16_t ping_seq_num = 0;
    int socket_for_ping = -1;
    struct netif *net_interface;
    char if_name[PING_IF_NAME_LEN];
    struct ifreq iface;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if(iface_context == NULL || address == NULL || elapsed_time_ms == NULL)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid arguments \n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    cy_network_interface_context *if_ctx;
    if_ctx = (cy_network_interface_context *)iface_context;

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the LwIP APIs
    */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /* Open a local socket for pinging */
    socket_for_ping = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    if (socket_for_ping < 0)
    {
        result = CY_RSLT_NETWORK_ERROR_PING;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "lwiP socket open error \n");
        goto exit;
    }

    /* Convert the timeout into struct timeval */
    timeout_val.tv_sec  = (long)(timeout_ms / 1000);
    timeout_val.tv_usec = (long)((timeout_ms % 1000) * 1000);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the LwIP APIs
    */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /* Set the receive timeout on the local socket, so ping will time out */
    if(lwip_setsockopt(socket_for_ping, SOL_SOCKET, SO_RCVTIMEO, &timeout_val, sizeof(struct timeval)) != ERR_OK)
    {
        result = CY_RSLT_NETWORK_ERROR_PING;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "lwip socket setting receive timeout error \n");
        goto exit;
    }

    /* Bind the interface to the device */
    net_interface = (struct netif *)if_ctx->nw_interface;
    memset(&iface, 0, sizeof(iface));
    memcpy(if_name, net_interface->name, sizeof(net_interface->name));
    snprintf(&if_name[2], (PING_IF_NAME_LEN - 2), "%u", (uint8_t)(net_interface->num));
    memcpy(iface.ifr_name, if_name, PING_IF_NAME_LEN);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the LwIP APIs
    */
    if (activity_callback)
    {
        activity_callback(true);
    }
    if(lwip_setsockopt(socket_for_ping, SOL_SOCKET, SO_BINDTODEVICE, &iface, sizeof(iface)) != ERR_OK)
    {
        result = CY_RSLT_NETWORK_ERROR_PING;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "lwip socket setting socket bind error \n");
        goto exit;
    }

    /* Send a ping request */
    err = ping_send(socket_for_ping, address, &ping_packet, &ping_seq_num);
    if (err != ERR_OK)
    {
        result = CY_RSLT_NETWORK_ERROR_PING;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "sending ping failed\n");
        goto exit;
    }
    /* Record the time the ping request was sent */
    cy_rtos_get_time(&send_time);

    /* Wait for the ping reply */
    err = ping_recv(socket_for_ping, address, &ping_seq_num);
    if (err != ERR_OK)
    {
        result = CY_RSLT_NETWORK_ERROR_PING;
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "receiving ping failed\n");
        goto exit;
    }

    /* Compute the elapsed time since a ping request was initiated */
    cy_rtos_get_time(&recvd_time);
    *elapsed_time_ms = (uint32_t)(recvd_time - send_time);

exit:
    /* Close the socket */
    if(socket_for_ping >= 0)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the LwIP APIs
        */
        if (activity_callback)
        {
            activity_callback(true);
        }
        lwip_close(socket_for_ping);
    }
    return result;
#else
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s() LWIP_IPV4 flag is not enabled \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

#ifdef CYBSP_WIFI_CAPABLE
/* Used to register callback for EAPOL packets */
cy_rslt_t cy_wifimwcore_eapol_register_receive_handler( cy_wifimwcore_eapol_packet_handler_t eapol_packet_handler )
{
    internal_eapol_packet_handler = eapol_packet_handler;
    return CY_RSLT_SUCCESS;
}
#endif

#if LWIP_IPV4
static void ping_prepare_echo(struct icmp_packet *iecho, uint16_t len, uint16_t *ping_seq_num)
{
    int i;
    ICMPH_TYPE_SET(&iecho->hdr, ICMP_ECHO);
    ICMPH_CODE_SET(&iecho->hdr, 0);
    iecho->hdr.chksum = 0;
    iecho->hdr.id = PING_ID;
    iecho->hdr.seqno = htons(++(*ping_seq_num));

    /* Fill the additional data buffer with some data */
    for ( i = 0; i < (int)sizeof(iecho->data); i++ )
    {
        iecho->data[i] = (uint8_t)i;
    }

#ifndef COMPONENT_CAT3
    iecho->hdr.chksum = inet_chksum(iecho, len);
#endif
}

static err_t ping_send(int socket_hnd, const cy_nw_ip_address_t* address, struct icmp_packet *iecho, uint16_t *sequence_number)
{
    int                err;
    struct sockaddr_in to;

    /* Construct ping request */
    ping_prepare_echo(iecho, sizeof(struct icmp_packet), sequence_number);

    /* Send the ping request */
    to.sin_len         = sizeof( to );
    to.sin_family      = AF_INET;
    to.sin_addr.s_addr = address->ip.v4;

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the LwIP APIs
    */
    if (activity_callback)
    {
        activity_callback(true);
    }
    err = lwip_sendto(socket_hnd, iecho, sizeof(struct icmp_packet), 0, (struct sockaddr*) &to, sizeof(to));

    return (err ? ERR_OK : ERR_VAL);
}

static err_t ping_recv(int socket_hnd, cy_nw_ip_address_t* address, uint16_t *ping_seq_num)
{
    char                  buf[PING_RESPONSE_LEN];
    int                   fromlen;
    int                   len;
    struct sockaddr_in    from;
    struct ip_hdr*        iphdr;
    struct icmp_echo_hdr* iecho;
    do
    {
        len = lwip_recvfrom(socket_hnd, buf, sizeof(buf), 0, (struct sockaddr*) &from, (socklen_t*) &fromlen);
        if (len >= (int) (sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr)))
        {
            iphdr = (struct ip_hdr *) buf;
            iecho = (struct icmp_echo_hdr *) (buf + (IPH_HL(iphdr) * 4));

            if ((iecho->id == PING_ID) &&
                 (iecho->seqno == htons(*ping_seq_num)) &&
                 (ICMPH_TYPE(iecho) == ICMP_ER))
            {
                return ERR_OK; /* Echo reply received - return success */
            }
        }
    } while (len > 0);

    return ERR_TIMEOUT; /* No valid echo reply received before timeout */
}
#endif

static void internal_ip_change_callback (struct netif *netif)
{
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IP change callback triggered\n");
    /* Notify ECM about IP address change */
    if(ip_change_callback != NULL)
    {
        for (int i = 0; i < CY_IFACE_MAX_HANDLE; i++)
        {
            if(netif == (struct netif *)iface_context_database[i].nw_interface)
            {
                cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "netif:[%p] iface_context_database[i].nw_interface:[%p]\n", netif, iface_context_database[i].nw_interface);
                ip_change_callback(&(iface_context_database[i]), NULL);
                return;
            }
        }
    }
}

static bool is_interface_added(uint8_t interface_index)
{
    return (ip_networking_inited[interface_index & 3]);
}

static bool is_network_up(uint8_t interface_index)
{
    return (ip_up[interface_index & 3]);
}

static cy_rslt_t is_interface_valid(cy_network_interface_context *iface)
{
    if( (iface == NULL) ||
        (iface->nw_interface == NULL) ||
        (iface->hw_interface == NULL) ||
        (((iface->iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface->iface_type != CY_NETWORK_WIFI_AP_INTERFACE)) &&
        (iface->iface_type != CY_NETWORK_ETH_INTERFACE) && (iface->iface_idx >= MAX_ETHERNET_PORT))
      )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "iface->iface_type:[%d] iface->iface_idx:[%d]\n", iface->iface_type, iface->iface_idx);
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    return CY_RSLT_SUCCESS;
}

#ifdef COMPONENT_4390X
/* CYW43907 kits do not have a TRNG module.
 * The following are the functions to generate pseudorandon numbers.
 * These functions are internal to the AnyCloud library; currently used
 * by Secure Sockets and WCM.
 */
static uint32_t crc32_calc( const uint8_t* buffer, uint16_t buffer_length, uint32_t prev_crc32 )
{
    uint32_t crc32 = ~prev_crc32;
    int i;

    for ( i = 0; i < buffer_length; i++ )
    {
        int j;

        crc32 ^= buffer[ i ];

        for ( j = 0; j < 8; j++ )
        {
            if ( crc32 & 0x1 )
            {
                crc32 = ( crc32 >> 1 ) ^ CY_PRNG_CRC32_POLYNOMIAL;
            }
            else
            {
                crc32 = ( crc32 >> 1 );
            }
        }
    }

    return ~crc32;
}

static uint32_t prng_well512_get_random( void )
{
    /*
     * Implementation of WELL (Well Equidistributed Long-period Linear) pseudorandom number generator.
     * Use the WELL512 source code placed by the inventor to public domain.
     */

    uint32_t a, b, c, d;
    uint32_t result;

    cy_rtos_get_mutex( cy_prng_mutex_ptr , CY_RTOS_NEVER_TIMEOUT);

    a = prng_well512_state[ prng_well512_index ];
    c = prng_well512_state[ ( prng_well512_index + 13 ) & 15 ];
    b = a ^ c ^ ( a << 16 ) ^ ( c << 15 );
    c = prng_well512_state[ ( prng_well512_index + 9 ) & 15 ];
    c ^= ( c >> 11 );
    a = prng_well512_state[ prng_well512_index ] = b ^ c;
    d = a ^ ( ( a << 5 ) & (uint32_t)0xDA442D24UL );
    prng_well512_index = ( prng_well512_index + 15 ) & 15;
    a = prng_well512_state[ prng_well512_index ];
    prng_well512_state[ prng_well512_index ] = a ^ b ^ d ^ ( a << 2 ) ^ ( b << 18 ) ^ ( c << 28 );

    result = prng_well512_state[ prng_well512_index ];

    cy_rtos_set_mutex( cy_prng_mutex_ptr );

    return result;
}

static void prng_well512_add_entropy( const void* buffer, uint16_t buffer_length )
{
    uint32_t crc32[ CY_PRNG_WELL512_STATE_SIZE ];
    uint32_t curr_crc32 = 0;
    unsigned i;

    for ( i = 0; i < CY_PRNG_WELL512_STATE_SIZE; i++ )
    {
        curr_crc32 = crc32_calc( buffer, buffer_length, curr_crc32 );
        crc32[ i ] = curr_crc32;
    }

    cy_rtos_get_mutex( cy_prng_mutex_ptr , CY_RTOS_NEVER_TIMEOUT);

    for ( i = 0; i < CY_PRNG_WELL512_STATE_SIZE; i++ )
    {
        prng_well512_state[ i ] ^= crc32[ i ];
    }

    cy_rtos_set_mutex( cy_prng_mutex_ptr );
}

static bool prng_is_add_cyclecnt_entropy( uint32_t buffer_length )
{
    bool add_entropy = false;

    cy_rtos_get_mutex( cy_prng_mutex_ptr , CY_RTOS_NEVER_TIMEOUT);

    if ( prng_add_cyclecnt_entropy_bytes >= CY_PRNG_ADD_CYCLECNT_ENTROPY_EACH_N_BYTE )
    {
        prng_add_cyclecnt_entropy_bytes %= CY_PRNG_ADD_CYCLECNT_ENTROPY_EACH_N_BYTE;
        add_entropy = true;
    }

    prng_add_cyclecnt_entropy_bytes += buffer_length;

    cy_rtos_set_mutex( cy_prng_mutex_ptr );

    return add_entropy;
}

static void prng_add_cyclecnt_entropy( uint32_t buffer_length )
{
    cy_rslt_t result;
    if ( prng_is_add_cyclecnt_entropy( buffer_length ) )
    {
        cy_time_t cycle_count;
        result = cy_rtos_get_time( &cycle_count );
        if (result == CY_RSLT_SUCCESS)
        {
            prng_well512_add_entropy( &cycle_count, sizeof( cycle_count ) );
        }
    }
    return;
}

cy_rslt_t cy_prng_get_random( void* buffer, uint32_t buffer_length )
{
    uint8_t* p = buffer;

    prng_add_cyclecnt_entropy( buffer_length );

    while ( buffer_length != 0 )
    {
        uint32_t rnd_val = prng_well512_get_random( );
        int      i;

        for ( i = 0; i < 4; i++ )
        {
            *p++ = (uint8_t)( rnd_val & 0xFF );
            if ( --buffer_length == 0 )
            {
                break;
            }
            rnd_val = ( rnd_val >> 8 );
        }
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_prng_add_entropy( const void* buffer, uint32_t buffer_length )
{
    prng_well512_add_entropy( buffer, buffer_length );
    return CY_RSLT_SUCCESS;
}
#endif

#if defined(COMPONENT_CAT1)
static int cy_trng_release()
{
    if (Cy_Crypto_Core_IsEnabled(CRYPTO))
    {
        Cy_Crypto_Core_Disable(CRYPTO);
    }
    return 0;
}
static int cy_trng_reserve()
{
    cy_en_crypto_status_t crypto_status;

    crypto_status = Cy_Crypto_Core_Enable(CRYPTO);
    if(crypto_status != CY_CRYPTO_SUCCESS)
    {
        cy_rtos_set_mutex(&trng_mutex);
        cy_rtos_deinit_mutex(&trng_mutex);
        return CY_RSLT_NETWORK_ERROR_TRNG;
    }
    return 0;
}

cy_rslt_t cy_network_random_number_generate( unsigned char *output, size_t len, size_t *olen )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    *olen = 0;
    /* temporary random data buffer */
    uint32_t random = 0u;

    result = cy_rtos_get_mutex(&trng_mutex, CY_RTOS_NEVER_TIMEOUT);
    if(CY_RSLT_SUCCESS != result)
    {
        cy_rtos_deinit_mutex(&trng_mutex);
        return CY_RSLT_NETWORK_ERROR_TRNG;
    }

    if(cy_trng_reserve() != 0)
    {
        return CY_RSLT_NETWORK_ERROR_TRNG;
    }

    /* Get Random byte */
    while (*olen < len)
    {
        if ( Cy_Crypto_Core_Trng(CRYPTO, CY_CRYPTO_DEF_TR_GARO, CY_CRYPTO_DEF_TR_FIRO,
                                             MAX_TRNG_BIT_SIZE, &random) != CY_CRYPTO_SUCCESS)
        {
            return CY_RSLT_NETWORK_ERROR_TRNG;
        }
        else
        {
            for (uint8_t i = 0; (i < 4) && (*olen < len) ; i++)
            {
                *output++ = ((uint8_t *)&random)[i];
                *olen += 1;
            }
        }
    }
    random = 0uL;

    Cy_Crypto_Core_Trng_DeInit(CRYPTO);
    if(cy_trng_release() != 0)
    {
        return CY_RSLT_NETWORK_ERROR_TRNG;
    }
    cy_rtos_set_mutex(&trng_mutex);
    return result;
}
#endif
