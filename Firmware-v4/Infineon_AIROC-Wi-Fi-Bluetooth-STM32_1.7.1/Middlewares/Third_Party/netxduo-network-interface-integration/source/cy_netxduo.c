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

#include "cy_network_mw_core.h"

#include <string.h>
#include <stdint.h>

#include "cy_wifimwcore_eapol.h"
#include "cy_result.h"
#include "cy_log.h"
#include "cybsp_wifi.h"
#include "cy_network_buffer.h"
#include "cy_network_buffer_netxduo.h"
#include "nx_api.h"
#include "nx_arp.h"
#include "nxd_dhcp_client.h"

#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
#include "nxd_dhcp_server.h"
#endif

#include "nxd_dns.h"
#include "cy_wifimwcore_eapol.h"

#include "whd.h"
#include "whd_wifi_api.h"
#include "whd_network_types.h"
#include "whd_buffer_api.h"

#include "cyhal.h"

#ifdef COMPONENT_CAT5
#include "whd_hw.h"
#endif

#if defined (COMPONENT_CAT1)
#include "cy_crypto_core.h"
#include "cy_crypto_core_trng_config.h"
#endif

/******************************************************
 *                    Constants
 ******************************************************/

#define IPv4_PACKET_TYPE                0x0800
#define IPv6_PACKET_TYPE                0x86DD
#define ARP_PACKET_TYPE                 0x0806
#define RARP_PACKET_TYPE                0x8035
#define EAPOL_PACKET_TYPE               0x888E

#define MAX_NW_INTERFACE                (2)

#define SIZE_OF_ARP_ENTRY               sizeof(NX_ARP)

#define IP_STACK_SIZE                   (2 * 1024)
#define ARP_CACHE_SIZE                  (6 * SIZE_OF_ARP_ENTRY)
#define DHCP_STACK_SIZE                 (1280)

#define DHCP_IP_ADDRESS_RESOLUTION_TIMEOUT  (10000)

#define DHCP_SERVER_MAX_NUM_CLIENTS         (10)

#define DNS_LOCAL_CACHE_SIZE                (2048)

#define MAX_LINK_LOCAL_IPV6_READY_ATTEMPTS  (10)
#define LINK_LOCAL_IPV6_ATTEMPT_INTERVAL    (500)

#define BLOCK_SIZE_ALIGNMENT                (64)
#define ARP_CACHE_CHECK_INTERVAL_IN_MSEC    (5)
#define ARP_WAIT_TIME_IN_MSEC               (30000)

#ifdef COMPONENT_CAT5
#define IP_THREAD_PRIORITY                  (11)
#define DHCP_THREAD_PRIORITY                (12)
#else
#define IP_THREAD_PRIORITY                  (2)
#endif

#if defined(COMPONENT_CAT1)
#define MAX_TRNG_BIT_SIZE        (32UL)
#endif
/******************************************************
 *                      Macros
 ******************************************************/


#define MULTICAST_IP_TO_MAC(ip)         {   (uint8_t) 0x01,             \
                                            (uint8_t) 0x00,             \
                                            (uint8_t) 0x5e,             \
                                            (uint8_t) ((ip)[1] & 0x7F), \
                                            (uint8_t) (ip)[2],          \
                                            (uint8_t) (ip)[3]           \
                                        }


#define IP_HANDLE(interface)            (cy_nxd_ip_handle[(interface) & 3])

#define DRIVER_FOR_IF(interface)        (cy_nxd_ip_driver_entries[(interface) & 3])
#define STACK_FOR_IF(interface)         (cy_nxd_ip_stack[(interface) & 3])
#define ARP_FOR_IF(interface)           (cy_nxd_arp_cache[(interface) & 3])

#define ULONG_BUFFER_ROUNDUP(x)         (((x) + sizeof(ULONG) - 1) / sizeof(ULONG))

#ifdef ENABLE_NETWORK_CORE_LOGS
#define wm_cy_log_msg cy_log_msg
#else
#define wm_cy_log_msg(a,b,c,...)
#endif

/******************************************************
 *               Static Function Declarations
 ******************************************************/
static void internal_ip_change_callback(NX_IP *netif, VOID *additional_info);

static void cy_sta_netxduo_driver_entry(NX_IP_DRIVER *driver);
static void cy_ap_netxduo_driver_entry(NX_IP_DRIVER *driver);

static void cy_netxduo_driver_entry(NX_IP_DRIVER *driver, whd_interface_t ifp);

static bool is_interface_added(cy_network_hw_interface_type_t iface_type);
static cy_rslt_t is_interface_valid(cy_network_interface_context *iface);
static bool is_network_up(cy_network_hw_interface_type_t iface_type);
static bool are_ip_services_enabled(cy_network_hw_interface_type_t iface_type);

static cy_rslt_t dhcp_client_init(cy_network_interface_context *iface, NX_PACKET_POOL *packet_pool);
static cy_rslt_t dhcp_client_deinit(void);

#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
static cy_rslt_t dhcp_server_init(cy_network_interface_context *iface, NX_PACKET_POOL *packet_pool);
static cy_rslt_t dhcp_server_deinit(void);
#endif

static cy_rslt_t dns_client_init(cy_network_interface_context *iface, NX_PACKET_POOL *packet_pool);
static cy_rslt_t dns_client_deinit(void);

static cy_rslt_t cy_netxduo_add_dns_server(cy_network_hw_interface_type_t iface_type, NXD_ADDRESS *dns_server_addr);

static NX_PACKET *cy_buffer_allocate_dynamic_packet(uint16_t payload_size);
static void       cy_buffer_free_dynamic_packet(NX_PACKET *packet);

#if defined(COMPONENT_CAT1)
static int cy_trng_release( void );
static int cy_trng_reserve( void );
#endif

/******************************************************
 *               Variable Definitions
 ******************************************************/

static NX_IP            wifi_sta_ip_handle;
static ULONG            wifi_sta_ip_stack[ULONG_BUFFER_ROUNDUP(IP_STACK_SIZE)];
static ULONG            wifi_sta_arp_cache[ULONG_BUFFER_ROUNDUP(ARP_CACHE_SIZE)];
static NX_DHCP          wifi_sta_dhcp_handle;
static bool             wifi_sta_dhcp_needed;
static NX_DNS           wifi_sta_dns_handle;
static cy_mutex_t       wifi_sta_dns_mutex;

#ifdef NX_DNS_CACHE_ENABLE
static ULONG            wifi_sta_dns_local_cache[ULONG_BUFFER_ROUNDUP(DNS_LOCAL_CACHE_SIZE)];
#endif

static NX_IP            wifi_ap_ip_handle;
static ULONG            wifi_ap_ip_stack[ULONG_BUFFER_ROUNDUP(IP_STACK_SIZE)];
static ULONG            wifi_ap_arp_cache[ULONG_BUFFER_ROUNDUP(ARP_CACHE_SIZE)];

#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
static NX_DHCP_SERVER   wifi_ap_dhcp_handle;
static ULONG            wifi_ap_dhcp_stack[ULONG_BUFFER_ROUNDUP(NX_DHCP_SERVER_THREAD_STACK_SIZE)];
#endif

static NX_IP *cy_nxd_ip_handle[MAX_NW_INTERFACE] =
{
    [CY_NETWORK_WIFI_STA_INTERFACE] =  &wifi_sta_ip_handle,
    [CY_NETWORK_WIFI_AP_INTERFACE]  =  &wifi_ap_ip_handle
};

/* IP interface whd iface status */
whd_interface_t ip_networking_whd_iface[MAX_NW_INTERFACE];
#define SET_IP_NETWORK_WHD_IFACE(interface, iface)  (ip_networking_whd_iface[(interface)&3] = iface)
#define GET_IP_NETWORK_WHD_IFACE(interface)         (ip_networking_whd_iface[(interface)&3])

/* IP UP status */
bool ip_up[MAX_NW_INTERFACE];
#define SET_IP_UP(interface, status)               (ip_up[(interface)&3] = status)

bool ip_services_enabled[MAX_NW_INTERFACE];
#define SET_IP_SERVICES_ENABLED(interface, status)  (ip_services_enabled[(interface)&3] = status)

static cy_network_activity_event_callback_t activity_callback;
static cy_wifimwcore_eapol_packet_handler_t internal_eapol_packet_handler;
static cy_network_ip_change_callback_t ip_change_callback;

static void (* const cy_nxd_ip_driver_entries[MAX_NW_INTERFACE])(struct NX_IP_DRIVER_STRUCT *) =
{
    [CY_NETWORK_WIFI_STA_INTERFACE] = cy_sta_netxduo_driver_entry,
    [CY_NETWORK_WIFI_AP_INTERFACE]  = cy_ap_netxduo_driver_entry
};

/* Network objects */
static char *cy_nxd_ip_stack[MAX_NW_INTERFACE] =
{
    [CY_NETWORK_WIFI_STA_INTERFACE] = (char *)wifi_sta_ip_stack,
    [CY_NETWORK_WIFI_AP_INTERFACE]  = (char *)wifi_ap_ip_stack,
};

static char *cy_nxd_arp_cache[MAX_NW_INTERFACE] =
{
    [CY_NETWORK_WIFI_STA_INTERFACE] = (char *)wifi_sta_arp_cache,
    [CY_NETWORK_WIFI_AP_INTERFACE]  = (char *)wifi_ap_arp_cache,
};


#define TX_PACKET_POOL              (0)
#define RX_PACKET_POOL              (1)

#define NUM_PACKET_POOLS            (2)

#ifndef TX_PACKET_POOL_SIZE
#ifdef COMPONENT_CAT5
#define TX_PACKET_POOL_SIZE         (16)
#else
#define TX_PACKET_POOL_SIZE         (24)
#endif
#endif

#ifndef RX_PACKET_POOL_SIZE
#ifdef COMPONENT_CAT5
#define RX_PACKET_POOL_SIZE         (16)
#else
#define RX_PACKET_POOL_SIZE         (24)
#endif
#endif

#ifdef COMPONENT_CAT5

#define TX_BUFFER_PAYLOAD_SIZE      (WHD_LINK_MTU + 20)
/* Note: Additional 20 bytes buffer is to ensure that all size payloads which gives rise to a network packet of size less than MTU(network stack MTU: payload + headers) can be accommodated into a single buffer to avoid packet chaining.
 * Packet sizes greater than MTU will be fragmented before sending out.
 * Ref: SWWLAN-147146
 */
#define RX_BUFFER_PAYLOAD_SIZE      (WHD_MSGBUF_DATA_MAX_RX_SIZE + sizeof(whd_buffer_header_t))

/*
 * Make sure the total size of each packet and payload is rounded up to a four byte boundary.
 */
#define APP_TX_BUFFER_POOL_SIZE     (((TX_BUFFER_PAYLOAD_SIZE + sizeof(NX_PACKET) + 0x03) & ~(0x03)) * (TX_PACKET_POOL_SIZE))
#define APP_RX_BUFFER_POOL_SIZE     (((RX_BUFFER_PAYLOAD_SIZE + sizeof(NX_PACKET) + 0x03) & ~(0x03)) * (RX_PACKET_POOL_SIZE))

#ifndef NUM_IOCTL_PACKETS
#define NUM_IOCTL_PACKETS           (3)
#endif

#define IOCTL_BUFFER_SIZE           ((WLC_IOCTL_MAXLEN + sizeof(NX_PACKET) + 0x03) & ~(0x03))
#define APP_IOCTL_BUFFER_POOL_SIZE  (IOCTL_BUFFER_SIZE * NUM_IOCTL_PACKETS)

#define PACKET_POOL_REGION_INDEX    (1)

static void *tx_buffer_pool_memory;
static void *rx_buffer_pool_memory;
static void *ioctl_buffer_memory;
static NX_PACKET *ioctl_packets[NUM_IOCTL_PACKETS];
static bool ioctl_packet_in_use[NUM_IOCTL_PACKETS];

static void *allocated_buffer_pool_memory;

#else /* COMPONENT_CAT5 */

#define TX_BUFFER_PAYLOAD_SIZE      (WHD_LINK_MTU)
#define RX_BUFFER_PAYLOAD_SIZE      (WHD_LINK_MTU + BLOCK_SIZE_ALIGNMENT)

/*
 * Make sure the total size of each packet and payload is rounded up to a four byte boundary.
 */
#define APP_TX_BUFFER_POOL_SIZE     (((TX_BUFFER_PAYLOAD_SIZE + sizeof(NX_PACKET) + 0x03) & ~(0x03)) * (TX_PACKET_POOL_SIZE))
#define APP_RX_BUFFER_POOL_SIZE     (((RX_BUFFER_PAYLOAD_SIZE + sizeof(NX_PACKET) + 0x03) & ~(0x03)) * (RX_PACKET_POOL_SIZE))

static ULONG tx_buffer_pool_memory[ULONG_BUFFER_ROUNDUP(APP_TX_BUFFER_POOL_SIZE)];
static ULONG rx_buffer_pool_memory[ULONG_BUFFER_ROUNDUP(APP_RX_BUFFER_POOL_SIZE)];
#endif /* COMPONENT_CAT5 */

static NX_PACKET_POOL whd_packet_pools[NUM_PACKET_POOLS];  /* 0=TX/COM, 1=RX/Default */

#if defined(COMPONENT_CAT1)
/** mutex to protect trng count */
static cy_mutex_t trng_mutex;
#endif
/******************************************************
 *               Function Definitions
 ******************************************************/
cy_rslt_t cy_network_init(void)
{
#ifdef COMPONENT_CAT5
    uint32_t memory_size;
    BOOL32 result;
    int i;
#endif

    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_network_init\n");

    // Initialize NetXDuo.
    nx_system_initialize();

#if defined(COMPONENT_CAT1)
    if( cy_rtos_init_mutex(&trng_mutex) != CY_RSLT_SUCCESS )
    {
        return CY_RSLT_NETWORK_ERROR_TRNG;
    }
#endif

#ifdef COMPONENT_CAT5
    if (allocated_buffer_pool_memory == NULL)
    {
        /*
         * Allocate the memory for the packet pools.
         */

        memory_size = APP_TX_BUFFER_POOL_SIZE + APP_RX_BUFFER_POOL_SIZE + APP_IOCTL_BUFFER_POOL_SIZE + (sizeof(uint32_t) * 3);
        allocated_buffer_pool_memory = whd_hw_allocatePermanentApi(memory_size);
        if (allocated_buffer_pool_memory == NULL)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "whd_hw_allocatePermanentApi fail\n");
            return CY_RSLT_TCPIP_ERROR;
        }

        /*
         * Map the memory region for WLAN to use.
         */

        result = whd_hw_openDeviceAccessApi(WHD_HW_DEVICE_WLAN, allocated_buffer_pool_memory, memory_size, PACKET_POOL_REGION_INDEX);
        if (!result)
        {
            return CY_RSLT_TCPIP_ERROR;
        }

        /*
         * Set up the regions for the packet pools. Make sure the address is aligned on a 4 byte boundary.
         */

        tx_buffer_pool_memory   = (void*)(((uint32_t)allocated_buffer_pool_memory + 0x03) & ~(0x03));
        rx_buffer_pool_memory   = (void*)(((uint32_t)tx_buffer_pool_memory + APP_TX_BUFFER_POOL_SIZE + 0x03) & ~(0x03));
        ioctl_buffer_memory     = (void*)(((uint32_t)rx_buffer_pool_memory + APP_RX_BUFFER_POOL_SIZE + 0x03) & ~(0x03));

        /*
         * Set up the pointers for the IOCTL packets.
         */

        for (i = 0; i < NUM_IOCTL_PACKETS; i++)
        {
            ioctl_packets[i] = (NX_PACKET*)(((uint32_t)ioctl_buffer_memory + (i * IOCTL_BUFFER_SIZE) + 0x03) & ~(0x03));
        }
    }
#endif

    /*
     * Create the TX and RX packet pools.
     */

    if ((nx_packet_pool_create(&whd_packet_pools[TX_PACKET_POOL], "", TX_BUFFER_PAYLOAD_SIZE, tx_buffer_pool_memory, APP_TX_BUFFER_POOL_SIZE) != NX_SUCCESS) ||
        (nx_packet_pool_create(&whd_packet_pools[RX_PACKET_POOL], "", RX_BUFFER_PAYLOAD_SIZE, rx_buffer_pool_memory, APP_RX_BUFFER_POOL_SIZE) != NX_SUCCESS))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "TX/RX pkt pool fail\n");
        return CY_RSLT_TCPIP_ERROR;
    }

    cy_buffer_pool_init(&whd_packet_pools[TX_PACKET_POOL], &whd_packet_pools[RX_PACKET_POOL]);
    cy_buffer_enable_dynamic_buffers(cy_buffer_allocate_dynamic_packet, cy_buffer_free_dynamic_packet);

    return CY_RSLT_SUCCESS;
}

static VOID cy_sta_netxduo_driver_entry(NX_IP_DRIVER *driver)
{
    cy_netxduo_driver_entry(driver, GET_IP_NETWORK_WHD_IFACE(CY_NETWORK_WIFI_STA_INTERFACE));
}

static VOID cy_ap_netxduo_driver_entry(NX_IP_DRIVER *driver)
{
    cy_netxduo_driver_entry(driver, GET_IP_NETWORK_WHD_IFACE(CY_NETWORK_WIFI_AP_INTERFACE));
}

void *cy_network_get_nw_interface(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx)
{
    if ((iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_type != CY_NETWORK_WIFI_AP_INTERFACE))
    {
        return NULL;
    }
    if (!is_interface_added(iface_type))
    {
        return NULL;
    }
    return (IP_HANDLE(iface_type));
}

/*
 * This function takes packets from the radio driver and passes them into the
 * NetXDuo stack.  If the stack is not initialized, or if the NetXDuo stack does not
 * accept the packet, the packet is freed (dropped). If packet is of type EAPOL
 * and if EAPOL handler is registered, packet will be redirected to registered
 * handler and should be freed by EAPOL handler.
 */
void cy_network_process_ethernet_data(whd_interface_t interface, whd_buffer_t buffer)
{
    NX_PACKET     *packet_ptr = (NX_PACKET *)buffer;
    unsigned char *data       = packet_ptr->nx_packet_prepend_ptr;
    USHORT         ethertype;
    NX_IP         *net_interface = NULL;

    if (interface)
    {
        if (interface->role == WHD_STA_ROLE)
        {
            net_interface = IP_HANDLE(CY_NETWORK_WIFI_STA_INTERFACE);
        }
        else if (interface->role == WHD_AP_ROLE)
        {
            net_interface = IP_HANDLE(CY_NETWORK_WIFI_AP_INTERFACE);
        }
    }

    ethertype = (uint16_t)(data[12] << 8 | data[13]);
    if (ethertype == EAPOL_PACKET_TYPE)
    {
        if (internal_eapol_packet_handler != NULL)
        {
            internal_eapol_packet_handler(interface, buffer);
        }
        else
        {
            cy_buffer_release(buffer, WHD_NETWORK_RX);
        }
    }
    else
    {
        if (net_interface == NULL || net_interface->nx_ip_id != NX_IP_ID)
        {
            cy_buffer_release(buffer, WHD_NETWORK_RX);
            return;
        }

        /* Call activity handler which is registered with argument as false
         * indicating there is RX packet
         */
        if (activity_callback)
        {
            activity_callback(false);
        }

        /* Remove the ethernet header, so packet is ready for reading by NetXDuo */
        packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr + WHD_ETHERNET_SIZE;
        packet_ptr->nx_packet_length      = packet_ptr->nx_packet_length - WHD_ETHERNET_SIZE;

#ifdef COMPONENT_CAT5
        if ((uint32_t)packet_ptr->nx_packet_prepend_ptr & 0x03)
        {
            /*
             * Packets from WLAN FW can be received where the IP header
             * does not start on a 4 byte boundary. This is a temporary
             * workaround until SWWLAN-146769 is addressed.
             */
            UCHAR *ptr = &packet_ptr->nx_packet_data_start[20];

            memmove(ptr, packet_ptr->nx_packet_prepend_ptr, packet_ptr->nx_packet_length);
            packet_ptr->nx_packet_prepend_ptr = ptr;
            packet_ptr->nx_packet_append_ptr  = (UCHAR*)((uint32_t)packet_ptr->nx_packet_prepend_ptr + packet_ptr->nx_packet_length);
        }
#endif

        if ((ethertype == IPv4_PACKET_TYPE) || ethertype == IPv6_PACKET_TYPE)
        {
#ifdef NX_DIRECT_ISR_CALL
            _nx_ip_packet_receive(net_interface, packet_ptr);
#else
            _nx_ip_packet_deferred_receive(net_interface, packet_ptr);
#endif
        }
        else if (ethertype == ARP_PACKET_TYPE)
        {
            _nx_arp_packet_deferred_receive(net_interface, packet_ptr);
        }
        else if (ethertype == RARP_PACKET_TYPE)
        {
            _nx_rarp_packet_deferred_receive(net_interface, packet_ptr);
        }
        else
        {
            /* Unknown ethertype - just release the packet */
            cy_buffer_release(buffer, WHD_NETWORK_RX);
        }
    }
}

static void cy_netxduo_multicast_driver_request_to_address(NX_IP_DRIVER *driver, whd_mac_t *mac)
{
    mac->octet[0] = (uint8_t) ((driver->nx_ip_driver_physical_address_msw & 0x0000ff00) >> 8);
    mac->octet[1] = (uint8_t) ((driver->nx_ip_driver_physical_address_msw & 0x000000ff) >> 0);
    mac->octet[2] = (uint8_t) ((driver->nx_ip_driver_physical_address_lsw & 0xff000000) >> 24);
    mac->octet[3] = (uint8_t) ((driver->nx_ip_driver_physical_address_lsw & 0x00ff0000) >> 16);
    mac->octet[4] = (uint8_t) ((driver->nx_ip_driver_physical_address_lsw & 0x0000ff00) >> 8);
    mac->octet[5] = (uint8_t) ((driver->nx_ip_driver_physical_address_lsw & 0x000000ff) >> 0);
}

static void cy_netxduo_add_ethernet_header(NX_IP *ip_ptr_in, NX_PACKET *packet_ptr, ULONG destination_mac_msw, ULONG destination_mac_lsw, USHORT ethertype)
{
    ULONG *ethernet_header;

    /* Make space at the front of the packet buffer for the ethernet header */
    packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr - WHD_ETHERNET_SIZE;
    packet_ptr->nx_packet_length = packet_ptr->nx_packet_length + WHD_ETHERNET_SIZE;

    /* Ensure ethernet header writing starts with 32 bit alignment */
    ethernet_header = (ULONG *)(packet_ptr->nx_packet_prepend_ptr - 2);

    *ethernet_header = destination_mac_msw;
    *(ethernet_header + 1) = destination_mac_lsw;
    *(ethernet_header + 2) = (ip_ptr_in->nx_ip_arp_physical_address_msw << 16) | (ip_ptr_in->nx_ip_arp_physical_address_lsw >> 16);
    *(ethernet_header + 3) = (ip_ptr_in->nx_ip_arp_physical_address_lsw << 16) | ethertype;

    NX_CHANGE_ULONG_ENDIAN(*(ethernet_header));
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_header+1));
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_header+2));
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_header+3));
}

/*
 * This function takes packets from the NetXDuo stack and sends them down to the radio.
 * If the radio is not ready, we return and error, otherwise we add a reference to
 * the packet for the radio driver and send the packet to the radio driver.  The radio
 * driver puts the packet into a send queue and will send based on another thread.  This
 * other thread will release the packet reference once the packet is actually sent.
 */
static void cy_netxduo_driver_entry(NX_IP_DRIVER *driver, whd_interface_t ifp)
{
    NX_PACKET *packet_ptr;
    whd_mac_t mac;
    NX_IP *ip;

    CY_ASSERT(driver != NULL);

    packet_ptr = driver->nx_ip_driver_packet;

    driver->nx_ip_driver_status = NX_NOT_SUCCESSFUL;

    /* Save the IP instance pointer so that the receive function will know where to send data */
    ip = driver->nx_ip_driver_ptr;

    /* Process commands which are valid independent of the link state */
    switch (driver->nx_ip_driver_command)
    {
        case NX_LINK_INITIALIZE:
            ip->nx_ip_driver_mtu            = (ULONG) WHD_PAYLOAD_MTU;
            ip->nx_ip_driver_mapping_needed = (UINT) NX_TRUE;
            driver->nx_ip_driver_status     = (UINT) NX_SUCCESS;
            break;

        case NX_LINK_UNINITIALIZE:
            return;

        case NX_LINK_ENABLE:
            if (whd_wifi_get_mac_address(ifp, &mac) != WHD_SUCCESS)
            {
                ip->nx_ip_driver_link_up = NX_FALSE;
                break;
            }

            ip->nx_ip_arp_physical_address_msw = (ULONG) ((mac.octet[0] << 8) + mac.octet[1]);
            ip->nx_ip_arp_physical_address_lsw = (ULONG) ((mac.octet[2] << 24) + (mac.octet[3] << 16) + (mac.octet[4] << 8) + mac.octet[5]);

            ip->nx_ip_driver_link_up    = (UINT) NX_TRUE;
            driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
            break;

        case NX_LINK_DISABLE:
            ip->nx_ip_driver_link_up    = NX_FALSE;
            driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
            break;

        case NX_LINK_MULTICAST_JOIN:
            cy_netxduo_multicast_driver_request_to_address(driver, &mac);

            if (whd_wifi_register_multicast_address(ifp, &mac) != WHD_SUCCESS)
            {
                driver->nx_ip_driver_status = (UINT) NX_NOT_SUCCESSFUL;
            }
            else
            {
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
            }
            break;

        case NX_LINK_MULTICAST_LEAVE:
            cy_netxduo_multicast_driver_request_to_address(driver, &mac);

            if (whd_wifi_unregister_multicast_address(ifp, &mac) != WHD_SUCCESS)
            {
                driver->nx_ip_driver_status = (UINT) NX_NOT_SUCCESSFUL;
            }
            else
            {
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
            }
            break;

        case NX_LINK_GET_STATUS:
            /* Signal status through return pointer */
            *(driver->nx_ip_driver_return_ptr) = (ULONG) ip->nx_ip_driver_link_up;
            driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
            break;

        case NX_LINK_PACKET_SEND:
        case NX_LINK_ARP_RESPONSE_SEND:
        case NX_LINK_ARP_SEND:
        case NX_LINK_RARP_SEND:
        case NX_LINK_PACKET_BROADCAST:
            /* These cases require the link to be up, and will be processed below if it is up. */
            break;

        case NX_LINK_DEFERRED_PROCESSING:
        default:
            /* Invalid driver request */
            driver->nx_ip_driver_status = (UINT) NX_UNHANDLED_COMMAND;
            break;
    }

    /* Check if the link is up */
    if ((ip->nx_ip_driver_link_up == NX_TRUE) && (whd_wifi_is_ready_to_transceive(ifp) == WHD_SUCCESS))
    {
        switch (driver->nx_ip_driver_command)
        {
            case NX_LINK_PACKET_SEND:
                if (packet_ptr->nx_packet_ip_version == NX_IP_VERSION_V4)
                {
                    cy_netxduo_add_ethernet_header(ip, packet_ptr, driver->nx_ip_driver_physical_address_msw, driver->nx_ip_driver_physical_address_lsw, (USHORT) WHD_ETHERTYPE_IPv4);
                }
                else if (packet_ptr->nx_packet_ip_version == NX_IP_VERSION_V6)
                {
                    cy_netxduo_add_ethernet_header(ip, packet_ptr, driver->nx_ip_driver_physical_address_msw, driver->nx_ip_driver_physical_address_lsw, (USHORT) WHD_ETHERTYPE_IPv6);
                }
                else
                {
                    whd_assert("Bad packet IP version", 0 != 0);
                    nx_packet_release(packet_ptr);
                    break;
                }
                whd_network_send_ethernet_data(ifp, (whd_buffer_t)packet_ptr);
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
                break;

            case NX_LINK_ARP_RESPONSE_SEND:
                cy_netxduo_add_ethernet_header(ip, packet_ptr, driver->nx_ip_driver_physical_address_msw, driver->nx_ip_driver_physical_address_lsw, (USHORT) WHD_ETHERTYPE_ARP);
                whd_network_send_ethernet_data(ifp, (whd_buffer_t)packet_ptr);
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
                break;

            case NX_LINK_ARP_SEND:
                cy_netxduo_add_ethernet_header(ip, packet_ptr, (ULONG) 0xFFFF, (ULONG) 0xFFFFFFFF, (USHORT) WHD_ETHERTYPE_ARP);
                whd_network_send_ethernet_data(ifp, (whd_buffer_t)packet_ptr);
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
                break;

            case NX_LINK_RARP_SEND:
                cy_netxduo_add_ethernet_header(ip, packet_ptr, (ULONG) 0xFFFF, (ULONG) 0xFFFFFFFF, (USHORT) WHD_ETHERTYPE_RARP);
                whd_network_send_ethernet_data(ifp, (whd_buffer_t)packet_ptr);
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
                break;

            case NX_LINK_PACKET_BROADCAST:
                if (packet_ptr->nx_packet_ip_version == NX_IP_VERSION_V4)
                {
                    cy_netxduo_add_ethernet_header(ip, packet_ptr, (ULONG) 0xFFFF, (ULONG) 0xFFFFFFFF, (USHORT) WHD_ETHERTYPE_IPv4);
                }
                else if (packet_ptr->nx_packet_ip_version == NX_IP_VERSION_V6)
                {
                    cy_netxduo_add_ethernet_header(ip, packet_ptr, (ULONG) 0xFFFF, (ULONG) 0xFFFFFFFF, (USHORT) WHD_ETHERTYPE_IPv6);
                }
                else
                {
                    whd_assert("Bad packet IP version", 0 != 0);
                    nx_packet_release(packet_ptr);
                    break;
                }
                whd_network_send_ethernet_data(ifp, (whd_buffer_t)packet_ptr);
                driver->nx_ip_driver_status = (UINT) NX_SUCCESS;
                break;

            case NX_LINK_INITIALIZE:
            case NX_LINK_ENABLE:
            case NX_LINK_DISABLE:
            case NX_LINK_MULTICAST_JOIN:
            case NX_LINK_MULTICAST_LEAVE:
            case NX_LINK_GET_STATUS:
            case NX_LINK_DEFERRED_PROCESSING:
            default:
                /* Handled in above case statement */
                break;
        }
    }
    else
    {
        /* Link is down, free any packet provided by the command */
        if (packet_ptr != NULL)
        {
            switch (driver->nx_ip_driver_command)
            {
                case NX_LINK_PACKET_BROADCAST:
                case NX_LINK_RARP_SEND:
                case NX_LINK_ARP_SEND:
                case NX_LINK_ARP_RESPONSE_SEND:
                case NX_LINK_PACKET_SEND:
                    nx_packet_release(packet_ptr);
                    break;

                default:
                    break;
            }
        }
    }
}

/*
 * This is the main entry point in this file.  This function takes a WHD radio driver
 * handle and and optional static IP address, and adds interface to NetXDuo.
 * Note: static IP address is optional for STA interface.
 */
cy_rslt_t cy_network_add_nw_interface(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx, void *hw_interface, uint8_t *mac_address,
                                      cy_network_static_ip_addr_t *static_ipaddr, cy_network_interface_context **iface_context)
{
    UINT  status;
    ULONG ip_addr;
    ULONG netmask;

    /* Currently only WIFI Interface is supported */
    if (((iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_type != CY_NETWORK_WIFI_AP_INTERFACE)) || iface_context == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }
    /* static IP addr is mandatory for AP */
    if ((iface_type == CY_NETWORK_WIFI_AP_INTERFACE) && (static_ipaddr == NULL))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error static IP addr cannot be NULL for AP interface\n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (is_interface_added(iface_type))
    {
        return CY_RSLT_SUCCESS;
    }

    /* Allocate memory for interface context */
    *iface_context =  calloc(sizeof(cy_network_interface_context), 1);
    if (*iface_context == NULL)
    {
        return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
    }
    /*
     * Set the WHD interface for the network role.
     */

    SET_IP_NETWORK_WHD_IFACE(iface_type, (whd_interface_t)hw_interface);

    /* Assign the IP address if static, otherwise, zero the IP address */
    if (static_ipaddr == NULL)
    {
        ip_addr = 0;
        netmask = 0xFFFFF000UL;
    }
    else
    {
        ip_addr = ntohl(static_ipaddr->addr.ip.v4);
        netmask = ntohl(static_ipaddr->netmask.ip.v4);
    }
    status = nx_ip_create(IP_HANDLE(iface_type), (char*)"NetXDuo IP", ip_addr, netmask,
                          &whd_packet_pools[TX_PACKET_POOL], DRIVER_FOR_IF(iface_type),
                          STACK_FOR_IF(iface_type), IP_STACK_SIZE, IP_THREAD_PRIORITY);

    if (status != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding interface\n");
        SET_IP_NETWORK_WHD_IFACE(iface_type, NULL);
        return CY_RSLT_NETWORK_ERROR_ADDING_INTERFACE;
    }

    if (iface_type == CY_NETWORK_WIFI_STA_INTERFACE && static_ipaddr == NULL)
    {
        wifi_sta_dhcp_needed = true;
    }

    if (!wifi_sta_dhcp_needed)
    {
        nx_ip_gateway_address_set(IP_HANDLE(iface_type), ntohl(static_ipaddr->gateway.ip.v4));
    }

    (*iface_context)->iface_type = iface_type;
    (*iface_context)->iface_idx = iface_idx;
    (*iface_context)->nw_interface = IP_HANDLE(iface_type);
    (*iface_context)->is_initialized = true;

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_remove_nw_interface(cy_network_interface_context *iface_context)
{
    NX_IP *ip_ptr;

    if (is_interface_valid(iface_context) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    ip_ptr = IP_HANDLE(iface_context->iface_type);

    /* Interface can be removed only if the interface was previously added and network is down */
    if (!is_interface_added(iface_context->iface_type))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error Interface doesn't exist\n");
        return CY_RSLT_NETWORK_INTERFACE_DOES_NOT_EXIST;
    }

    if (is_network_up(iface_context->iface_type))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error removing interface, bring down the network before removing the interface\n");
        return CY_RSLT_NETWORK_ERROR_REMOVING_INTERFACE;
    }

    /* Delete the network interface */
    if (ip_ptr->nx_ip_id == NX_IP_ID)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }

        if (nx_ip_delete(IP_HANDLE(iface_context->iface_type)) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Could not delete IP instance\n");
            return CY_RSLT_NETWORK_ERROR_REMOVING_INTERFACE;
        }
        memset(ip_ptr, 0, sizeof(NX_IP));
        SET_IP_SERVICES_ENABLED(iface_context->iface_type, false);
    }

    if (iface_context->iface_type == CY_NETWORK_WIFI_STA_INTERFACE)
    {
        wifi_sta_dhcp_needed = false;
    }

    SET_IP_NETWORK_WHD_IFACE(iface_context->iface_type, NULL);

    memset(iface_context, 0, sizeof(cy_network_interface_context));

    free(iface_context);
    return CY_RSLT_SUCCESS;
}

#ifndef NX_DISABLE_IPV6
cy_rslt_t cy_netxduo_autoipv6(cy_network_interface_context *iface_context)
{
    UINT        ipv6_address_index;
    UINT        ipv6_interface_index;
    ULONG       ipv6_prefix;
    NXD_ADDRESS ipv6_address;
    uint8_t     ipv6_address_attempt = 0;

    /* Set the IPv6 linklocal address using our MAC */
    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Setting IPv6 link-local address\n");

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    nxd_ipv6_address_delete(IP_HANDLE(iface_context->iface_type), 0);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    nxd_ipv6_enable(IP_HANDLE(iface_context->iface_type));

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    nxd_ipv6_address_set(IP_HANDLE(iface_context->iface_type), CY_NETWORK_PRIMARY_INTERFACE, NX_NULL, 10, &ipv6_address_index);

    /* Wait until the link-local address is properly advertised using network solicitation frame
     * and nobody on the local network complains.
     * If the address is not valid after MAX_LINK_LOCAL_IPV6_READY_ATTEMPS disable ipv6 */

    do
    {
        if (IP_HANDLE(iface_context->iface_type)->nx_ipv6_address[ipv6_address_index].nxd_ipv6_address_state == NX_IPV6_ADDR_STATE_VALID)
        {
#ifdef ENABLE_NETWORK_CORE_LOGS
            uint16_t *ipv6 = (uint16_t *)ipv6_address.nxd_ip_address.v6;
#endif
            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack,
             * before invoking the netxduo APIs
             */
            if (activity_callback)
            {
                activity_callback(true);
            }
            nxd_ipv6_address_get(IP_HANDLE(iface_context->iface_type), ipv6_address_index, &ipv6_address, &ipv6_prefix, &ipv6_interface_index);
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPv6 network ready IP: %.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X\n",
                          (unsigned int)(ipv6[1]), (unsigned int)(ipv6[0]), (unsigned int)(ipv6[3]),
                          (unsigned int)(ipv6[2]), (unsigned int)(ipv6[5]), (unsigned int)(ipv6[4]),
                          (unsigned int)(ipv6[7]), (unsigned int)(ipv6[6]));
            break;
        }
        cy_rtos_delay_milliseconds(LINK_LOCAL_IPV6_ATTEMPT_INTERVAL);
        ipv6_address_attempt++;

    } while (ipv6_address_attempt < MAX_LINK_LOCAL_IPV6_READY_ATTEMPTS);

    if (IP_HANDLE(iface_context->iface_type)->nx_ipv6_address[ipv6_address_index].nxd_ipv6_address_state != NX_IPV6_ADDR_STATE_VALID)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPv6 network is not ready\n");

        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nxd_ipv6_disable(IP_HANDLE(iface_context->iface_type));
        return CY_RSLT_TCPIP_ERROR;
    }

    return CY_RSLT_SUCCESS;
}
#endif

cy_rslt_t cy_network_ip_up(cy_network_interface_context *iface_context)
{
    UINT status;
    UINT res;

    if (is_interface_valid(iface_context) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (is_network_up(iface_context->iface_type))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Network is already up\n");
        return CY_RSLT_SUCCESS;
    }

    if (!are_ip_services_enabled(iface_context->iface_type))
    {
        if ((res = nx_arp_enable(IP_HANDLE(iface_context->iface_type), ARP_FOR_IF(iface_context->iface_type), ARP_CACHE_SIZE)) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "error enable arp: %d\n", res);
            return CY_RSLT_TCPIP_ERROR;
        }

        if ((res = nx_tcp_enable(IP_HANDLE(iface_context->iface_type))) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "error enable tcp: %d\n", res);
            return CY_RSLT_TCPIP_ERROR;
        }

        if ((res = nx_udp_enable(IP_HANDLE(iface_context->iface_type))) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "error enable udp: %d\n", res);
            return CY_RSLT_TCPIP_ERROR;
        }

        if ((res = nxd_icmp_enable(IP_HANDLE(iface_context->iface_type))) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "error enable icmp: %d\n", res);
            return CY_RSLT_TCPIP_ERROR;
        }

        if ((res = nx_igmp_enable(IP_HANDLE(iface_context->iface_type))) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "error enable igmp: %d\n", res);
            return CY_RSLT_TCPIP_ERROR;
        }

        if ((res = nx_ip_fragment_enable(IP_HANDLE(iface_context->iface_type))) != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "error enable ip frag: %d\n", res);
            return CY_RSLT_TCPIP_ERROR;
        }

        SET_IP_SERVICES_ENABLED(iface_context->iface_type, true);
    }

#ifndef NX_DISABLE_IPV6
    cy_netxduo_autoipv6(iface_context);
#endif

#ifndef NX_DISABLE_IPV4
    if (iface_context->iface_type == CY_NETWORK_WIFI_STA_INTERFACE && wifi_sta_dhcp_needed)
    {
        if (dhcp_client_init(iface_context, whd_packet_pools) != CY_RSLT_SUCCESS)
        {
            return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
        }

        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }

        /* Check for address resolution and wait for our addresses to be ready */
        res = nx_ip_status_check(IP_HANDLE(iface_context->iface_type), NX_IP_ADDRESS_RESOLVED, (ULONG *)&status, DHCP_IP_ADDRESS_RESOLUTION_TIMEOUT);
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "ip: %d status:%X\n", res, status);

        if (res == NX_SUCCESS)
        {
            ULONG ip;
            ULONG netmask;

            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack,
             * before invoking the netxduo APIs
             */
            if (activity_callback)
            {
                activity_callback(true);
            }
            nx_ip_address_get(IP_HANDLE(iface_context->iface_type), &ip, &netmask);
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "=======================================\n");
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPv4: %u.%u.%u.%u  netmask: %u.%u.%u.%u\n", PRINT_HOST_IP(ip), PRINT_HOST_IP(netmask));

            /*
             * If LPA is enabled, invoke the activity callback to resume the network stack,
             * before invoking the netxduo APIs
             */
            if (activity_callback)
            {
                activity_callback(true);
            }
            /* Register a handler for any address changes */
            res = nx_ip_address_change_notify(IP_HANDLE(iface_context->iface_type), internal_ip_change_callback, iface_context);
            if (res != NX_SUCCESS)
            {
                wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "ip_change: %d\n", res);
                dhcp_client_deinit();
                return CY_RSLT_TCPIP_ERROR;
            }
        }
        else
        {
            dhcp_client_deinit();
            return CY_RSLT_TCPIP_ERROR;
        }
    }
    else if (iface_context->iface_type == CY_NETWORK_WIFI_AP_INTERFACE)
    {
#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
        if (dhcp_server_init(iface_context, whd_packet_pools) != CY_RSLT_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error starting DHCP server\n", res);
            return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
        }
#endif
    }
#endif

    if (iface_context->iface_type == CY_NETWORK_WIFI_STA_INTERFACE && wifi_sta_dhcp_needed)
    {
        /*
         * Start the DNS client.
         */

        res = dns_client_init(iface_context, whd_packet_pools);
        if (res == NX_SUCCESS)
        {
            NXD_ADDRESS addr;
            ULONG dns_ip[3];
            UINT size = sizeof(dns_ip);

            /*
             * Get the DNS server address from DHCP.
             */

            res = nx_dhcp_interface_user_option_retrieve(&wifi_sta_dhcp_handle, 0, NX_DHCP_OPTION_DNS_SVR, (UCHAR *)dns_ip, &size);

            if (res == NX_SUCCESS)
            {
                addr.nxd_ip_version = NX_IP_VERSION_V4;
                addr.nxd_ip_address.v4 = dns_ip[0];
                if (cy_netxduo_add_dns_server(iface_context->iface_type, &addr) != CY_RSLT_SUCCESS)
                {
                    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding DNS server\n");
                }
            }
        }
    }

    SET_IP_UP(iface_context->iface_type, true);

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_ip_down(cy_network_interface_context *iface_context)
{
    if (is_interface_valid(iface_context) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (!is_network_up(iface_context->iface_type))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Network is not UP\n");
        return CY_RSLT_NETWORK_INTERFACE_NETWORK_NOT_UP;
    }

    /*
     * Stop any network services that we started.
     */

    if (iface_context->iface_type == CY_NETWORK_WIFI_STA_INTERFACE && wifi_sta_dhcp_needed)
    {
        dhcp_client_deinit();

        dns_client_deinit();
    }

    if (iface_context->iface_type == CY_NETWORK_WIFI_AP_INTERFACE)
    {
#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
        dhcp_server_deinit();
#endif
    }

    /*
     * Mark the network down.
     */

    SET_IP_UP(iface_context->iface_type, false);

    return CY_RSLT_SUCCESS;
}

void cy_network_register_ip_change_cb(cy_network_interface_context *iface_context, cy_network_ip_change_callback_t cb, void *userdata)
{
    ip_change_callback = cb;
}

/*
 * This functions helps to register/unregister callback for network activity
 */
void cy_network_activity_register_cb(cy_network_activity_event_callback_t cb)
{
    /* update the activity callback with the argument passed */
    activity_callback = cb;
}

/*
 * This function notifies network activity to LPA module.
 */
cy_rslt_t cy_network_activity_notify(cy_network_activity_type_t activity_type)
{
    if (activity_callback)
    {
        if (activity_type == CY_NETWORK_ACTIVITY_TX)
        {
            activity_callback(true);
        }
        else if (activity_type == CY_NETWORK_ACTIVITY_RX)
        {
            activity_callback(false);
        }
        else
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid network activity type\n");
            return CY_RSLT_NETWORK_BAD_ARG;
        }
    }

    return CY_RSLT_SUCCESS;
}

#ifndef NX_DISABLE_IPV4

void *cy_network_get_dhcp_handle(cy_network_hw_interface_type_t iface_type, uint8_t iface_idx)
{
    if ((iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_type != CY_NETWORK_WIFI_AP_INTERFACE))
    {
        return NULL;
    }
    if (!is_interface_added(iface_type))
    {
        return NULL;
    }
    if(wifi_sta_dhcp_needed == false)
    {
        return NULL;
    }
    return (&wifi_sta_dhcp_handle);
}

cy_rslt_t cy_network_dhcp_renew(cy_network_interface_context *iface_context)
{
    NX_DHCP *dhcp_handle = &wifi_sta_dhcp_handle;

    if (is_interface_valid(iface_context) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /* Invalidate ARP entries */
    _nx_arp_interface_entries_delete(IP_HANDLE(iface_context->iface_type), 0);

    /* DHCP renewal*/
    nx_dhcp_force_renew(dhcp_handle);

    return CY_RSLT_SUCCESS;
}
#endif

/* Used to register callback for EAPOL packets */
cy_rslt_t cy_wifimwcore_eapol_register_receive_handler(cy_wifimwcore_eapol_packet_handler_t eapol_packet_handler)
{
    internal_eapol_packet_handler = eapol_packet_handler;
    return CY_RSLT_SUCCESS;
}

static void internal_ip_change_callback(NX_IP *netif, VOID *additional_info)
{
    /* notify wcm about ip change */
    if (ip_change_callback != NULL)
    {
        ip_change_callback(additional_info, NULL);
    }
}

static bool is_interface_added(cy_network_hw_interface_type_t iface_type)
{
    /*
     * If the WHD interface has been set for the
     * interface then we know that it's been added.
     */
    return ((GET_IP_NETWORK_WHD_IFACE(iface_type) != NULL));
}

static bool is_network_up(cy_network_hw_interface_type_t iface_type)
{
    return (ip_up[iface_type & 3]);
}

static bool are_ip_services_enabled(cy_network_hw_interface_type_t iface_type)
{
    return (ip_services_enabled[iface_type & 3]);
}

static cy_rslt_t is_interface_valid(cy_network_interface_context *iface)
{
    if ((iface == NULL) || iface->is_initialized == false)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t dhcp_client_init(cy_network_interface_context *iface, NX_PACKET_POOL *packet_pool)
{
    NX_IP       *ip_handle   = IP_HANDLE(iface->iface_type);
    NX_DHCP     *dhcp_handle = &wifi_sta_dhcp_handle;
    UINT        res;

    /* clear DHCP info to start */
    memset(dhcp_handle, 0, sizeof(*dhcp_handle));

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /* Create the DHCP instance. */
#ifdef COMPONENT_CAT5
    res = nx_dhcp_create(dhcp_handle, ip_handle, "CY WHD", DHCP_THREAD_PRIORITY);
#else
    res = nx_dhcp_create(dhcp_handle, ip_handle, "CY WHD");
#endif
    if (res != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "dhcp_create: 0x%02x\n", res);
        return CY_RSLT_TCPIP_ERROR;
    }

    nx_dhcp_packet_pool_set(dhcp_handle, packet_pool);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    /* Start DHCP. */
    res = nx_dhcp_start(dhcp_handle);
    if (res != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "dhcp_start: %d\n", res);

        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nx_dhcp_delete(dhcp_handle);

        /* Clear the DHCP handle structure */
        memset(dhcp_handle, 0, sizeof(*dhcp_handle));

        return CY_RSLT_NETWORK_ERROR_STARTING_DHCP;
    }

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t dhcp_client_deinit(void)
{
    NX_DHCP *dhcp_handle = &wifi_sta_dhcp_handle;
    UINT     res;

    if (dhcp_handle->nx_dhcp_id != NX_DHCP_ID)
    {
        return CY_RSLT_SUCCESS;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    res = nx_dhcp_stop(dhcp_handle);
    if ((res != NX_SUCCESS) && (res != NX_DHCP_NOT_STARTED))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "Failed to stop DHCP client\n");
     }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    nx_dhcp_delete(dhcp_handle);

    /* Clear the DHCP handle structure */
    memset(dhcp_handle, 0, sizeof(*dhcp_handle));

    return CY_RSLT_SUCCESS;
}

#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
static cy_rslt_t dhcp_server_init(cy_network_interface_context *iface, NX_PACKET_POOL *packet_pool)
{
    NX_IP *ip_handle = IP_HANDLE(iface->iface_type);
    NX_DHCP_SERVER *dhcp_handle = &wifi_ap_dhcp_handle;
    ULONG ip_addr;
    ULONG netmask;
    ULONG start_address;
    ULONG end_address;
    UINT addresses_added;
    UINT res;

    /* clear DHCP info to start */
    memset(dhcp_handle, 0, sizeof(*dhcp_handle));

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /*
     * Get the IP address and netmask.
     */

    if (nx_ip_address_get(ip_handle, &ip_addr, &netmask) != NX_SUCCESS)
    {
        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }

    /*
     * Calculate the pool of addresses to use for the DHCP server.
     */

    start_address = ip_addr;
    if ((start_address & 0xFF) + DHCP_SERVER_MAX_NUM_CLIENTS > 0xFF)
    {
        /*
         * Not enough room for all the addresses.
         */

        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }
    start_address += 1;
    end_address = start_address + DHCP_SERVER_MAX_NUM_CLIENTS - 1;

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    /* Create the DHCP Server.  */
#ifdef COMPONENT_CAT5
    res = nx_dhcp_server_create(dhcp_handle, ip_handle, wifi_ap_dhcp_stack, NX_DHCP_SERVER_THREAD_STACK_SIZE, "CY DHCP Server", packet_pool, DHCP_THREAD_PRIORITY);
#else
    res = nx_dhcp_server_create(dhcp_handle, ip_handle, wifi_ap_dhcp_stack, NX_DHCP_SERVER_THREAD_STACK_SIZE, "CY DHCP Server", packet_pool);
#endif

    /* Check for errors creating the DHCP Server. */
    if (res != NX_SUCCESS)
    {
        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }

    /* Load the assignable DHCP IP addresses for the first interface.  */
    res = nx_dhcp_create_server_ip_address_list(dhcp_handle, 0, start_address, end_address, &addresses_added);

    /* Check for errors creating the list. */
    if (res != NX_SUCCESS)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nx_dhcp_server_delete(dhcp_handle);

        /* Clear the DHCP handle structure */
        memset(dhcp_handle, 0, sizeof(*dhcp_handle));

        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }

    /* Verify all the addresses were added to the list. */
    if (addresses_added != DHCP_SERVER_MAX_NUM_CLIENTS)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nx_dhcp_server_delete(dhcp_handle);

        /* Clear the DHCP handle structure */
        memset(dhcp_handle, 0, sizeof(*dhcp_handle));

        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }

    /*
     * Set the interface network parameters.
     * Use our address for the gateway and DNS server address.
     */

    res = nx_dhcp_set_interface_network_parameters(dhcp_handle, 0, netmask, ip_addr, ip_addr);

    /* Check for errors setting network parameters. */
    if (res != NX_SUCCESS)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nx_dhcp_server_delete(dhcp_handle);

        /* Clear the DHCP handle structure */
        memset(dhcp_handle, 0, sizeof(*dhcp_handle));

        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    /* Start DHCP Server task.  */
    res = nx_dhcp_server_start(dhcp_handle);
    if (res != NX_SUCCESS)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nx_dhcp_server_delete(dhcp_handle);

        /* Clear the DHCP handle structure */
        memset(dhcp_handle, 0, sizeof(*dhcp_handle));

        return CY_RSLT_NETWORK_ERROR_STARTING_INTERNAL_DHCP;
    }

    return CY_RSLT_SUCCESS;
}
#endif

#ifndef CY_NETWORK_DISABLE_DHCP_SERVER
static cy_rslt_t dhcp_server_deinit(void)
{
    NX_DHCP_SERVER *dhcp_handle = &wifi_ap_dhcp_handle;
    UINT res;

    if (dhcp_handle->nx_dhcp_id != NX_DHCP_SERVER_ID)
    {
        return CY_RSLT_SUCCESS;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    res = nx_dhcp_server_stop(dhcp_handle);
    if ((res != NX_SUCCESS) && (res != NX_DHCP_SERVER_NOT_STARTED))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_WARNING, "Failed to stop DHCP server\n");
     }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    nx_dhcp_server_delete(dhcp_handle);

    /* Clear the DHCP handle structure */
    memset(dhcp_handle, 0, sizeof(*dhcp_handle));

    return CY_RSLT_SUCCESS;
}
#endif

static cy_rslt_t dns_client_init(cy_network_interface_context *iface, NX_PACKET_POOL *packet_pool)
{
    NX_IP       *ip_handle   = IP_HANDLE(iface->iface_type);
    NX_DNS      *dns_handle = &wifi_sta_dns_handle;
    UINT        res;

    /* clear DHCP info to start */
    memset(dns_handle, 0, sizeof(*dns_handle));

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }
    res = nx_dns_create(dns_handle, ip_handle, (UCHAR *)"DNS Client");
    if (res != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "dns_create: 0x%02x\n", res);
        return CY_RSLT_NETWORK_ERROR_STARTING_DNS;
    }

#ifdef NX_DNS_CACHE_ENABLE
    res = nx_dns_cache_initialize(dns_handle, dns_local_cache, DNS_LOCAL_CACHE_SIZE);
    if (res != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to init DNS cache: 0x%02x\n", res);
    }
#endif

#ifdef NX_DNS_CLIENT_USER_CREATE_PACKET_POOL
    res = nx_dns_packet_pool_set(dns_handle, packet_pool);
    if (res != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_dns_packet_pool_set: 0x%02x\n", res);

        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        nx_dns_delete(dns_handle);

        /* Clear the DNS handle structure */
        memset(dns_handle, 0, sizeof(*dns_handle));

        return CY_RSLT_NETWORK_ERROR_STARTING_DNS;
    }
#endif

    cy_rtos_init_mutex(&wifi_sta_dns_mutex);
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t dns_client_deinit(void)
{
    NX_DNS *dns_handle = &wifi_sta_dns_handle;

    if (dns_handle->nx_dns_id != NX_DNS_ID)
    {
        return CY_RSLT_SUCCESS;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    nx_dns_delete(dns_handle);

    /* Clear the DNS handle structure */
    memset(dns_handle, 0, sizeof(*dns_handle));

    cy_rtos_deinit_mutex(&wifi_sta_dns_mutex);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_netxduo_add_dns_server(cy_network_hw_interface_type_t iface_type, NXD_ADDRESS *dns_server_addr)
{
    NX_DNS      *dns_handle = &wifi_sta_dns_handle;
    UINT        res = NX_SUCCESS;

    /* Valid interface? */
    if (iface_type != CY_NETWORK_WIFI_STA_INTERFACE)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /* Is DNS running and do we have a valid address pointer? */
    if (dns_handle->nx_dns_id != NX_DNS_ID || dns_server_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (dns_server_addr->nxd_ip_version == NX_IP_VERSION_V4)
    {
        res = nx_dns_server_add(dns_handle, dns_server_addr->nxd_ip_address.v4);
    }
    else if (dns_server_addr->nxd_ip_version == NX_IP_VERSION_V6)
    {
        res = nxd_dns_server_add(dns_handle, dns_server_addr);
    }
    else
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error adding DNS server: 0x%02x\n", res);
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (res != NX_SUCCESS)
    {
        return CY_RSLT_NETWORK_ERROR_ADDING_DNS_SERVER;
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_get_hostbyname(cy_network_hw_interface_type_t iface_type, unsigned char *hostname, uint32_t lookup_type, uint32_t timeout, void *ipaddr)
{
    NX_DNS      *dns_handle = &wifi_sta_dns_handle;
    UINT        res = NX_SUCCESS;

    /* Valid interface? */
    if (iface_type != CY_NETWORK_WIFI_STA_INTERFACE)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if ((lookup_type != NX_IP_VERSION_V4 && lookup_type != NX_IP_VERSION_V6) || ipaddr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    cy_rtos_get_mutex(&wifi_sta_dns_mutex, CY_RTOS_NEVER_TIMEOUT);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    res = nxd_dns_host_by_name_get(dns_handle, hostname, (NXD_ADDRESS *)ipaddr, NX_TIMEOUT(timeout), lookup_type);
    cy_rtos_set_mutex(&wifi_sta_dns_mutex);
    if (res != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error looking up addr for %s: 0x%02x\n", hostname, res);
        return CY_RSLT_TCPIP_ERROR;
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_get_packet_pool(cy_network_packet_dir_t direction, void **pool)
{
    if (pool == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /*
     * Return the pool based on the direction.
     */

    if (direction == CY_NETWORK_PACKET_TX)
    {
        (*(NX_PACKET_POOL **)pool) = &whd_packet_pools[TX_PACKET_POOL];
    }
    else
    {
        (*(NX_PACKET_POOL **)pool) = &whd_packet_pools[RX_PACKET_POOL];
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_get_packet(cy_network_packet_type_t packet_type, uint32_t timeout, void **packet)
{
    NX_PACKET_POOL *pool;
    ULONG type;
    UINT status;

    if (packet == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /*
     * What type of packet are we allocating?
     */

    switch (packet_type)
    {
        case CY_NETWORK_IP_PACKET:   type = NX_IP_PACKET;   break;
        case CY_NETWORK_TCP_PACKET:  type = NX_TCP_PACKET;  break;
        case CY_NETWORK_UDP_PACKET:  type = NX_UDP_PACKET;  break;
        case CY_NETWORK_ICMP_PACKET: type = NX_ICMP_PACKET; break;
        default:
            return CY_RSLT_NETWORK_BAD_ARG;
    }

    /*
     * Allocate from the TX packet pool
     */

    pool = &whd_packet_pools[TX_PACKET_POOL];

    /*
     * Get that packet.
     */

    status = nx_packet_allocate(pool, (NX_PACKET **)packet, type, NX_TIMEOUT(timeout));
    if (status != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to allocate packet: 0x%02x\n", status);
        return CY_RSLT_NETWORK_ERROR_NO_PACKET;
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_network_get_ip_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *ip_addr)
{
#ifndef NX_DISABLE_IPV4
    NX_IP *net_interface;
    ULONG ipv4_addr;
    ULONG netmask;

    if ((ip_addr == NULL) || (iface_context == NULL) ||
        ((iface_context->iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_context->iface_type != CY_NETWORK_WIFI_AP_INTERFACE)))
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    memset(ip_addr, 0, sizeof(cy_nw_ip_address_t));
    net_interface = IP_HANDLE(iface_context->iface_type);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    if (nx_ip_address_get(net_interface, &ipv4_addr, &netmask) == NX_SUCCESS)
    {
        ip_addr->version = NW_IP_IPV4;
        ip_addr->ip.v4   = htonl(ipv4_addr);
    }
    else
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready\n");
        return CY_RSLT_NETWORK_NO_INTERFACE_ADDRESS;
    }

    return CY_RSLT_SUCCESS;

#else
    /* IPV4 disabled */

    if (ip_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERROR, "IPV4 is disabled\n");
    memset(ip_addr, 0, sizeof(cy_nw_ip_address_t));

    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_ipv6_address(cy_network_interface_context *iface_context, cy_network_ipv6_type_t ipv6_addr_type, cy_nw_ip_address_t *ip_addr)
{
#ifndef NX_DISABLE_IPV6
    NX_IP *net_interface;
    NXD_ADDRESS ipv6_addr;
    UINT address_index;
    ULONG prefix_length;
    UINT interface_index;
#ifdef ENABLE_NETWORK_CORE_LOGS
    uint16_t *ipv6 = (uint16_t *)ipv6_addr.nxd_ip_address.v6;
#endif
    if ((ip_addr == NULL) || (iface_context == NULL) ||
        ((iface_context->iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_context->iface_type != CY_NETWORK_WIFI_AP_INTERFACE)))
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    memset(ip_addr, 0, sizeof(cy_nw_ip_address_t));

    if (ipv6_addr_type != CY_NETWORK_IPV6_LINK_LOCAL)
    {
        /*TODO : Need to add support for Global IPV6 address */
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    address_index = 0;
    net_interface = IP_HANDLE(iface_context->iface_type);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    if (nxd_ipv6_address_get(net_interface, address_index, &ipv6_addr, &prefix_length, &interface_index) == NX_SUCCESS)
    {
        ip_addr->version = NW_IP_IPV6;
        ip_addr->ip.v6[0] = htonl(ipv6_addr.nxd_ip_address.v6[0]);
        ip_addr->ip.v6[1] = htonl(ipv6_addr.nxd_ip_address.v6[1]);
        ip_addr->ip.v6[2] = htonl(ipv6_addr.nxd_ip_address.v6[2]);
        ip_addr->ip.v6[3] = htonl(ipv6_addr.nxd_ip_address.v6[3]);

        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "IPV6 Address %.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X assigned\n",
                      (unsigned int)(ipv6[1]), (unsigned int)(ipv6[0]), (unsigned int)(ipv6[3]),
                      (unsigned int)(ipv6[2]), (unsigned int)(ipv6[5]), (unsigned int)(ipv6[4]),
                      (unsigned int)(ipv6[7]), (unsigned int)(ipv6[6]));
    }
    else
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 network not ready\n");
        return CY_RSLT_NETWORK_NO_INTERFACE_ADDRESS;
    }

    return CY_RSLT_SUCCESS;
#else
    /* IPV6 disabled */

    if (ip_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERROR, "IPV6 is disabled\n");
    memset(ip_addr, 0, sizeof(cy_nw_ip_address_t));

    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_gateway_ip_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *gateway_addr)
{
#ifndef NX_DISABLE_IPV4
    NX_IP *net_interface;
    ULONG ipv4_addr;

    if (gateway_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }
    memset(gateway_addr, 0, sizeof(cy_nw_ip_address_t));

    net_interface = IP_HANDLE(iface_context->iface_type);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    if (nx_ip_gateway_address_get(net_interface, &ipv4_addr) == NX_SUCCESS)
    {
        gateway_addr->version = NW_IP_IPV4;
        gateway_addr->ip.v4   = htonl(ipv4_addr);
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Gateway IP Address %u.%u.%u.%u assigned\n", PRINT_HOST_IP(ipv4_addr));
        return CY_RSLT_SUCCESS;
    }
    else
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error retrieving gateway address\n");
        return CY_RSLT_NETWORK_GW_ADDRESS_NOT_FOUND;
    }

#else
    /* IPV4 disabled */

    if (gateway_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERROR, "IPV4 is disabled\n");
    memset(gateway_addr, 0, sizeof(cy_nw_ip_address_t));

    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_get_gateway_mac_address(cy_network_interface_context *iface_context, cy_nw_ip_mac_t *mac_addr)
{
#ifndef NX_DISABLE_IPV4
    UINT err;
    ULONG ipv4_addr;
    ULONG physical_msw;
    ULONG physical_lsw;
    int32_t arp_waittime = ARP_WAIT_TIME_IN_MSEC;
    NX_IP *net_interface;

    if (mac_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }
    memset(mac_addr, 0, sizeof(cy_nw_ip_mac_t));

    if (iface_context->iface_type == CY_NETWORK_WIFI_AP_INTERFACE)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "feature not supported for AP interface\n");
        return CY_RSLT_NETWORK_NOT_SUPPORTED;
    }

    net_interface = IP_HANDLE(CY_NETWORK_WIFI_STA_INTERFACE);
    if (net_interface == NULL)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "network interface is NULL\n");
        return CY_RSLT_NETWORK_INTERFACE_DOES_NOT_EXIST;
    }

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    nx_ip_gateway_address_get(net_interface, &ipv4_addr);

    if (nx_arp_hardware_address_find(net_interface, ipv4_addr, &physical_msw, &physical_lsw) != NX_SUCCESS)
    {
        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        /* Entry for the address is not present in the ARP cache. Sent ARP request.*/
        err = _nx_arp_dynamic_entry_set(net_interface, ipv4_addr, 0, 0);
        if (err != NX_SUCCESS)
        {
            wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "_nx_arp_packet_send failed with error 0x%02x\n", err);
            return CY_RSLT_NETWORK_ARP_REQUEST_FAILURE;
        }

        /*
         * If LPA is enabled, invoke the activity callback to resume the network stack,
         * before invoking the netxduo APIs
         */
        if (activity_callback)
        {
            activity_callback(true);
        }
        do
        {
            if (nx_arp_hardware_address_find(net_interface, ipv4_addr, &physical_msw, &physical_lsw) == NX_SUCCESS)
            {
                wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "arp entry found\n");
                break;
            }
            cy_rtos_delay_milliseconds(ARP_CACHE_CHECK_INTERVAL_IN_MSEC);
            arp_waittime -= ARP_CACHE_CHECK_INTERVAL_IN_MSEC;
            if (arp_waittime <= 0)
            {
                wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Could not resolve MAC address for the given destination address\n");
                return CY_RSLT_NETWORK_WAIT_TIMEOUT;
            }
        } while(1);
    }


    mac_addr->mac[0] = (uint8_t)((physical_msw >>  8) & 0xFF);
    mac_addr->mac[1] = (uint8_t)((physical_msw >>  0) & 0xFF);
    mac_addr->mac[2] = (uint8_t)((physical_lsw >> 24) & 0xFF);
    mac_addr->mac[3] = (uint8_t)((physical_lsw >> 16) & 0xFF);
    mac_addr->mac[4] = (uint8_t)((physical_lsw >>  8) & 0xFF);
    mac_addr->mac[5] = (uint8_t)((physical_lsw >>  0) & 0xFF);

    return CY_RSLT_SUCCESS;
#else
    /* IPV4 disabled */

    if (mac_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERROR, "IPV4 is disabled\n");
    memset(mac_addr, 0, sizeof(mac_addr));

    return CY_RSLT_NETXDUO_BAD_ARG;
#endif
}


cy_rslt_t cy_network_get_netmask_address(cy_network_interface_context *iface_context, cy_nw_ip_address_t *net_mask_addr)
{
#ifndef NX_DISABLE_IPV4
    NX_IP *net_interface;
    ULONG ipv4_addr;
    ULONG netmask;

    if ((net_mask_addr == NULL) || (iface_context == NULL) ||
        ((iface_context->iface_type != CY_NETWORK_WIFI_STA_INTERFACE) && (iface_context->iface_type != CY_NETWORK_WIFI_AP_INTERFACE)))
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    memset(net_mask_addr, 0, sizeof(cy_nw_ip_address_t));
    net_interface = IP_HANDLE(iface_context->iface_type);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    nx_ip_address_get(net_interface, &ipv4_addr, &netmask);

    net_mask_addr->version = NW_IP_IPV4;
    net_mask_addr->ip.v4   = htonl(netmask);

    return CY_RSLT_SUCCESS;

#else
    /* IPV4 disabled */

    if (net_mask_addr == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERROR, "IPV4 is disabled\n");
    memset(net_mask_addr, 0, sizeof(cy_nw_ip_address_t));

    return CY_RSLT_NETWORK_NOT_SUPPORTED;
#endif
}

cy_rslt_t cy_network_ping(cy_network_interface_context *iface_context, cy_nw_ip_address_t *address, uint32_t timeout_ms, uint32_t* elapsed_time_ms)
{
#ifndef NX_DISABLE_IPV4
    cy_time_t send_time;
    cy_time_t recvd_time;
    UINT err;
    NX_IP *net_interface;
    NX_PACKET *response_ptr;
    ULONG timeout_ticks;

    if((address == NULL) || (elapsed_time_ms == NULL))
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "BAD arguments \r\n");
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /*
     * Make sure we aren't going to wait forever.
     */

    if (timeout_ms == TX_WAIT_FOREVER)
    {
        timeout_ms = TX_WAIT_FOREVER - 1;
    }
    net_interface = iface_context->nw_interface;

    /* Record time ping was sent */
    cy_rtos_get_time(&send_time);

    /*
     * If LPA is enabled, invoke the activity callback to resume the network stack,
     * before invoking the netxduo APIs
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

    /* Convert milliseconds to timer ticks */
    timeout_ticks = NX_TIMEOUT(timeout_ms);

    /* Send the ping */
    err = nx_icmp_ping(net_interface, ntohl(address->ip.v4), "abcd", 4, &response_ptr, timeout_ticks);
    if (err != NX_SUCCESS)
    {
        return CY_RSLT_NETWORK_PING_FAILURE;
    }

    /* compute the elapsed time since a ping request was initiated */
    cy_rtos_get_time(&recvd_time);
    *elapsed_time_ms = (uint32_t)(recvd_time - send_time);

    /*
     * Don't forget to release the packet.
     */

    nx_packet_release(response_ptr);

    return CY_RSLT_SUCCESS;
#else
    /* IPV4 disabled */

    if (address == NULL || elapsed_time_ms)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    memset(address, 0, sizeof(*address));
    memset(elapsed_time_ms, 0, sizeof(*elapsed_time_ms));

    return CY_RSLT_NETWORK_PING_FAILURE;
#endif
}
cy_rslt_t cy_network_deinit(void)
{
#ifdef COMPONENT_CAT1
    cy_rtos_deinit_mutex(&trng_mutex);
#endif
    return CY_RSLT_SUCCESS;
}

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

/* True random number function for NetXDuo.
 * In nx_user.h file NX_RAND is defined with cy_rand.
 */
UINT cy_rand( void )
{
    UINT output;
    size_t olen;

    if (cy_network_random_number_generate( (unsigned char *)&output, sizeof(UINT), &olen ) != CY_RSLT_SUCCESS )
    {
        return 0;
    }
    return output;
}
#endif

//--------------------------------------------------------------------------------------------------
// cy_buffer_allocate_dynamic_packet
//--------------------------------------------------------------------------------------------------
static NX_PACKET *cy_buffer_allocate_dynamic_packet(uint16_t payload_size)
{
    NX_PACKET *packet;
    ULONG header_size;
#ifdef COMPONENT_CAT5
    int i;
#endif

    /*
     * Allocate a dynamic packet to satisfy a request for a payload size that is larger than
     * the size in the packet pool.
     *
     * NOTE: This API is only used for WHD communications to support IOVARS with payloads
     * larger than WHD_LINK_MTU. The nx_packet_pool_owner pointer in the packet is left
     * as NULL. When the packet is released in cy_buffer_release, the NULL nx_packet_pool_owner
     * pointer will trigger a call to cy_buffer_free_dynamic_packet rather than trying to release
     * the packet back to the packet pool.
     *
     * Packets allocated via this API should never be passed to the IP stack as they
     * can not be released properly by the stack since they do not belong to a packet pool.
     */

#ifdef COMPONENT_CAT5
    if (payload_size > WLC_IOCTL_MAXLEN)
    {
        return NULL;
    }

    /*
     * Do we have a free buffer?
     */

    for (i = 0; i < NUM_IOCTL_PACKETS; i++)
    {
        if (!ioctl_packet_in_use[i])
        {
            break;
        }
    }

    if (i == NUM_IOCTL_PACKETS)
    {
        return NULL;
    }

    memset(ioctl_packets[i], 0, IOCTL_BUFFER_SIZE);
    packet = ioctl_packets[i];
    ioctl_packet_in_use[i] = true;
#else
    packet = calloc(1, (payload_size + BLOCK_SIZE_ALIGNMENT + sizeof(NX_PACKET)));
#endif

    if (packet)
    {
        /*
         * Initialize the packet structure elements that are needed for use.
         */

        header_size =
            (ULONG)(((sizeof(NX_PACKET) + NX_PACKET_ALIGNMENT - 1) / NX_PACKET_ALIGNMENT) *
                    NX_PACKET_ALIGNMENT);
        packet->nx_packet_data_start  = (UCHAR*)((uint32_t)packet + header_size);
        packet->nx_packet_data_end    = (UCHAR*)((uint32_t)packet + header_size + payload_size);
        packet->nx_packet_prepend_ptr = packet->nx_packet_data_start;
        packet->nx_packet_append_ptr  =
            (UCHAR*)((uint32_t)packet->nx_packet_prepend_ptr + payload_size);
        packet->nx_packet_length      = payload_size;
    }

    return packet;
}


//--------------------------------------------------------------------------------------------------
// cy_buffer_free_dynamic_packet
//--------------------------------------------------------------------------------------------------
static void cy_buffer_free_dynamic_packet(NX_PACKET *packet)
{
    if (packet != NULL)
    {
#ifdef COMPONENT_CAT5
        int i;

        for (i = 0; i < NUM_IOCTL_PACKETS; i++)
        {
            if (ioctl_packets[i] == packet)
            {
                ioctl_packet_in_use[i] = false;
                break;
            }
        }
#else
        free(packet);
#endif
    }
}

//--------------------------------------------------------------------------------------------------
// cy_network_get_pool_info is used to get the pool information based on packet direction.
//--------------------------------------------------------------------------------------------------
void cy_network_get_packet_pool_info(cy_network_packet_dir_t direction, cy_network_packet_pool_info_t *pool_info)
{
    NX_PACKET_POOL *pool;
    UINT status;

    if(direction == CY_NETWORK_PACKET_TX)
    {
        pool = &whd_packet_pools[TX_PACKET_POOL];
    }
    else if(direction == CY_NETWORK_PACKET_RX)
    {
        pool = &whd_packet_pools[RX_PACKET_POOL];
    }
    else
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "BAD arguments \r\n");
        return;
    }

    status = nx_packet_pool_info_get(pool, &(pool_info->total_packets),
                                           &(pool_info->free_packets),
                                           &(pool_info->empty_pool_requests),
                                           &(pool_info->empty_pool_suspensions),
                                           &(pool_info->invalid_packet_releases));
    if (status != NX_SUCCESS)
    {
        wm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to get packet pool information: 0x%02x\n", status);
    }

    return;
}
