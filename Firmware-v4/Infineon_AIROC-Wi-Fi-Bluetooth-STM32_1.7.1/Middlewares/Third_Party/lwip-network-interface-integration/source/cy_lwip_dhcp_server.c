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
 *  Implementation of a simple DHCP server
 */

#include "lwip/err.h"
#include "lwip/api.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"

#if LWIP_IPV4

#include "cy_lwip_dhcp_server.h"
#include "cy_nw_mw_core_error.h"
#include "cyabs_rtos.h"
#include "cy_lwip_log.h"
#include <string.h>

/******************************************************
 *                      Macros
 ******************************************************/

#ifndef MIN
#define MIN(x,y)  ((x) < (y) ? (x) : (y))
#endif /* ifndef MIN */

#define MAKE_IPV4_ADDRESS(a, b, c, d)                 ((((uint32_t) d) << 24) | (((uint32_t) c) << 16) | (((uint32_t) b) << 8) |((uint32_t) a))
#define SET_IPV4_ADDRESS(addr_var, addr_val)          (((addr_var).version = CY_LWIP_IP_VER_V4),((addr_var).ip.v4 = (uint32_t)(addr_val)))
#define GET_IPV4_ADDRESS(addr_var)                    ((addr_var).ip.v4)
#define MEMCAT(destination, source, source_length)    (void*)((uint8_t*)memcpy((destination),(source),(source_length)) + (source_length))

/******************************************************
 *                    Constants
 ******************************************************/

#define ULONG_MAX_STR                           "4294967295"
#define ULONG_MIN_STR                           "0000000000"

#ifndef DHCP_IP_ADDRESS_CACHE_MAX
#define DHCP_IP_ADDRESS_CACHE_MAX               (5)
#endif

#define DHCP_SERVER_RECEIVE_TIMEOUT             (500)
#define ALLOCATE_PACKET_TIMEOUT                 (2000)

/* BOOTP operations */
#define BOOTP_OP_REQUEST                        (1)
#define BOOTP_OP_REPLY                          (2)

/* DHCP options */
#define DHCP_SUBNETMASK_OPTION_CODE             (1)
#define DHCP_MTU_OPTION_CODE                    (26)
#define DHCP_REQUESTED_IP_ADDRESS_OPTION_CODE   (50)
#define DHCP_LEASETIME_OPTION_CODE              (51)
#define DHCP_MESSAGETYPE_OPTION_CODE            (53)
#define DHCP_SERVER_IDENTIFIER_OPTION_CODE      (54)
#define DHCP_WPAD_OPTION_CODE                   (252)
#define DHCP_END_OPTION_CODE                    (255)

/* DHCP commands */
#define DHCPDISCOVER                            (1)
#define DHCPOFFER                               (2)
#define DHCPREQUEST                             (3)
#define DHCPDECLINE                             (4)
#define DHCPACK                                 (5)
#define DHCPNAK                                 (6)
#define DHCPRELEASE                             (7)
#define DHCPINFORM                              (8)
#define CY_LWIP_PAYLOAD_MTU                     (1500)
#define PHYSICAL_HEADER                         (44)

/* UDP port numbers for DHCP server and client */
#define IPPORT_DHCPS                            (67)
#define IPPORT_DHCPC                            (68)
#define WAIT_FOREVER                            ((uint32_t) 0xFFFFFFFF)
#define MAX_UDP_PAYLOAD_SIZE                    (CY_LWIP_PAYLOAD_MTU - UDP_HLEN - IP_HLEN - PHYSICAL_HEADER)
#define CY_DHCP_MAX_MUTEX_WAIT_TIME_MS          (120000)
#define WPAD_SAMPLE_URL                         "http://xxx.xxx.xxx.xxx/wpad.dat"

/******************************************************
 *               Variable Definitions
 ******************************************************/
static const uint8_t mtu_option_buff[]             = { DHCP_MTU_OPTION_CODE, 2, CY_LWIP_PAYLOAD_MTU>>8, CY_LWIP_PAYLOAD_MTU&0xff };
static const uint8_t dhcp_offer_option_buff[]      = { DHCP_MESSAGETYPE_OPTION_CODE, 1, DHCPOFFER };
static const uint8_t dhcp_ack_option_buff[]        = { DHCP_MESSAGETYPE_OPTION_CODE, 1, DHCPACK };
static const uint8_t dhcp_nak_option_buff[]        = { DHCP_MESSAGETYPE_OPTION_CODE, 1, DHCPNAK };
static const uint8_t lease_time_option_buff[]      = { DHCP_LEASETIME_OPTION_CODE, 4, 0x00, 0x01, 0x51, 0x80 }; /* 1-day lease */
static const uint8_t dhcp_magic_cookie[]           = { 0x63, 0x82, 0x53, 0x63 };
static const cy_lwip_mac_addr_t empty_cache        = { .octet = {0} };
typedef struct netbuf cy_lwip_packet_t;
static cy_mutex_t dhcp_mutex;

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/* DHCP data structure */
typedef struct
{
    uint8_t  opcode;                     /* Packet opcode type */
    uint8_t  hardware_type;              /* Hardware addr type */
    uint8_t  hardware_addr_len;          /* Hardware addr length */
    uint8_t  hops;                       /* Gateway hops */
    uint32_t transaction_id;             /* Transaction ID */
    uint16_t second_elapsed;             /* Seconds since boot began */
    uint16_t flags;                      /* DCHP flags, reserved for future */
    uint8_t  client_ip_addr[4];          /* Client IP address */
    uint8_t  your_ip_addr[4];            /* 'Your' IP address */
    uint8_t  server_ip_addr[4];          /* Server IP address */
    uint8_t  gateway_ip_addr[4];         /* Gateway IP address */
    uint8_t  client_hardware_addr[16];   /* Client hardware address */
    uint8_t  legacy[192];                /* DHCP legacy header */
    uint8_t  magic[4];                   /* DHCP magic cookie */
    uint8_t  options[275];               /* Options area */
    /* as of RFC2131, it is of variable length */
} dhcp_header_t;

/******************************************************
 *               Static Function Declarations
 ******************************************************/

static const uint8_t* find_option (const dhcp_header_t* request, uint8_t option_num);
static bool get_client_ip_address_from_cache (const cy_lwip_mac_addr_t* client_mac_address, cy_lwip_ip_address_t* client_ip_address);
static cy_rslt_t add_client_to_cache (const cy_lwip_mac_addr_t* client_mac_address, const cy_lwip_ip_address_t* client_ip_address);
static void ipv4_to_string (char* buffer, uint32_t ipv4_address);
static void cy_dhcp_thread_func (cy_thread_arg_t thread_input);
static cy_rslt_t udp_create_socket(cy_lwip_udp_socket_t *socket, uint16_t port, cy_network_interface_context *iface_context);
static cy_rslt_t udp_delete_socket(cy_lwip_udp_socket_t *socket);
static cy_rslt_t udp_receive(cy_lwip_udp_socket_t *socket, cy_lwip_packet_t** packet, uint32_t timeout);
static cy_rslt_t packet_get_data(cy_lwip_packet_t *packet, uint16_t offset, uint8_t** data, uint16_t* fragment_available_data_length, uint16_t *total_available_data_length);
static cy_rslt_t packet_set_data_end(cy_lwip_packet_t *packet, uint8_t* data_end);
static cy_rslt_t packet_delete(cy_lwip_packet_t* packet);
static cy_rslt_t packet_create_udp(cy_lwip_packet_t** packet, uint8_t** data, uint16_t* available_space);
static cy_rslt_t internal_packet_create(cy_lwip_packet_t** packet, uint16_t content_length, uint8_t** data, uint16_t* available_space);
static cy_rslt_t cy_udp_send(cy_lwip_udp_socket_t* socket, const cy_lwip_ip_address_t* address, uint16_t port, cy_lwip_packet_t* packet);
static cy_rslt_t internal_udp_send(struct netconn* handler, cy_lwip_packet_t* packet, cy_network_hw_interface_type_t type, uint8_t index);
static void cy_ip_to_lwip(ip_addr_t *dest, const cy_lwip_ip_address_t *src);

/******************************************************
 *               Variable Definitions
 ******************************************************/

static cy_lwip_mac_addr_t             cached_mac_addresses[DHCP_IP_ADDRESS_CACHE_MAX];
static cy_lwip_ip_address_t           cached_ip_addresses [DHCP_IP_ADDRESS_CACHE_MAX];
static struct netif *net_interface    = NULL;
static bool is_dhcp_server_started    = false;
/******************************************************
 *               Function Definitions
 ******************************************************/

#define DHCP_THREAD_PRIORITY                  (CY_RTOS_PRIORITY_ABOVENORMAL)
#define DHCP_THREAD_STACK_SIZE                (1280)


cy_rslt_t cy_lwip_dhcp_server_start(cy_lwip_dhcp_server_t* server, cy_network_interface_context *iface_context)
{
    cy_rslt_t result;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if(is_dhcp_server_started)
    {
        return CY_RSLT_SUCCESS;
    }

    if((server == NULL) || (iface_context->iface_type != CY_NETWORK_WIFI_AP_INTERFACE))
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error DHCP bad arguments. iface_context->iface_type:[%d] \n", iface_context->iface_type);
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    if (cy_rtos_init_mutex(&dhcp_mutex) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Unable to acquire DHCP mutex \n");
        return CY_RSLT_NETWORK_DHCP_MUTEX_ERROR;
    }

    /* Create a DHCP socket */
    if((result = udp_create_socket(&server->socket, IPPORT_DHCPS, iface_context)) != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : UDP socket creation failed \n");
        goto exit;
    }

    /* Clear the cache */
    memset(cached_mac_addresses, 0, sizeof(cached_mac_addresses));
    memset(cached_ip_addresses,  0, sizeof(cached_ip_addresses));

    /* Initialize the server quit flag - done here if a quit is requested before the thread runs */
    server->quit = false;

    /* Start the DHCP server thread */
    result = cy_rtos_create_thread(&server->thread, cy_dhcp_thread_func, "DHCPserver", NULL, DHCP_THREAD_STACK_SIZE, DHCP_THREAD_PRIORITY, (cy_thread_arg_t) server);
    if (result != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : Unable to create the DHCP thread \n");
        udp_delete_socket(&server->socket);
    }

exit:
    if(result != CY_RSLT_SUCCESS)
    {
        cy_rtos_deinit_mutex(&dhcp_mutex);
    }
    else
    {
        is_dhcp_server_started = true;
    }

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): STOP \n", __FUNCTION__ );
    return result;
}

cy_rslt_t cy_lwip_dhcp_server_stop(cy_lwip_dhcp_server_t* server)
{
    cy_rslt_t res = CY_RSLT_SUCCESS;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if(!is_dhcp_server_started)
    {
        return CY_RSLT_SUCCESS;
    }

    if(server == NULL)
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    server->quit = true;
    cy_rtos_terminate_thread(&server->thread);
    cy_rtos_join_thread(&server->thread);
    /* Delete DHCP socket */
    res = udp_delete_socket(&server->socket);
    cy_rtos_deinit_mutex(&dhcp_mutex);
    is_dhcp_server_started = false;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): STOP \n", __FUNCTION__ );
    return res;
}

/**
 *  Implements a very simple DHCP server.
 *
 *  The server will always offer the next available address to a DISCOVER command.
 *  The server will NAK any REQUEST command which is not requesting the next available address.
 *  The server will ACK any REQUEST command which is for the next available address, and then increment the next available address.
 *
 * @param my_addr : Local IP address for binding of the server port
 */
static void cy_dhcp_thread_func(cy_thread_arg_t thread_input)
{
    cy_lwip_packet_t             *received_packet;
    cy_lwip_packet_t             *transmit_packet = NULL;
    cy_lwip_ip_address_t         local_ip_address;
    cy_lwip_ip_address_t         netmask;
    uint32_t                     next_available_ip_addr;
    uint32_t                     ip_mask;
    uint32_t                     subnet;
    uint32_t                     netmask_htobe;
    uint32_t                     server_ip_addr_htobe;
    char                         *option_ptr;
    cy_lwip_dhcp_server_t        *server                      = (cy_lwip_dhcp_server_t*)thread_input;
    uint8_t                      subnet_mask_option_buff[]    = { DHCP_SUBNETMASK_OPTION_CODE, 4, 0, 0, 0, 0 };
    uint8_t                      server_ip_addr_option_buff[] = { DHCP_SERVER_IDENTIFIER_OPTION_CODE, 4, 0, 0, 0, 0 };
    uint8_t                      wpad_option_buff[ 2 + sizeof(WPAD_SAMPLE_URL)-1 ] = { DHCP_WPAD_OPTION_CODE, sizeof(WPAD_SAMPLE_URL)-1 };
    cy_lwip_ip_address_t         broadcast_addr;

    memset(&local_ip_address, 0x00, sizeof(cy_lwip_ip_address_t));

    SET_IPV4_ADDRESS(broadcast_addr, MAKE_IPV4_ADDRESS(255, 255, 255, 255));
    /* Save the local IP address to be sent in DHCP packets */

    net_interface = (struct netif *)cy_network_get_nw_interface( server->socket.type, server->socket.index );
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "DHCP_SERVER net_interface:[%p] \n", net_interface);

    local_ip_address.version = CY_LWIP_IP_VER_V4;
#if LWIP_IPV6
    local_ip_address.ip.v4 = ntohl(net_interface->ip_addr.u_addr.ip4.addr);
#else
    local_ip_address.ip.v4 = ntohl(net_interface->ip_addr.addr);
#endif

    server_ip_addr_htobe = htobe32(GET_IPV4_ADDRESS(local_ip_address));
    memcpy(&server_ip_addr_option_buff[2], &server_ip_addr_htobe, 4);

    /* Save the current netmask to be sent in DHCP packets as the 'subnet mask option' */
    netmask.version = CY_LWIP_IP_VER_V4;
#if LWIP_IPV4 && LWIP_IPV6
    netmask.ip.v4 = ntohl(net_interface->netmask.u_addr.ip4.addr);
#elif LWIP_IPV4
    netmask.ip.v4 = ntohl(net_interface->netmask.addr);
#endif
    netmask_htobe = htobe32(GET_IPV4_ADDRESS(netmask));
    memcpy(&subnet_mask_option_buff[2], &netmask_htobe, 4);

    /* Calculate the first available IP address which will be served - based on the netmask and the local IP address*/
    ip_mask = ~(GET_IPV4_ADDRESS(netmask));
    subnet = GET_IPV4_ADDRESS(local_ip_address) & GET_IPV4_ADDRESS(netmask);
    next_available_ip_addr = subnet | ((GET_IPV4_ADDRESS(local_ip_address) + 1) & ip_mask);

    /* Prepare the web proxy auto-discovery URL */
    memcpy(&wpad_option_buff[2], WPAD_SAMPLE_URL, sizeof(WPAD_SAMPLE_URL)-1);
    ipv4_to_string((char*)&wpad_option_buff[2 + 7], server_ip_addr_option_buff[2]);

    /* Loop endlessly */
    while ( server->quit == false )
    {
        uint16_t       data_length = 0;
        uint16_t       available_data_length = 0;
        dhcp_header_t  *request_header;

        /* Sleep until the data is received from the socket */
        if (udp_receive(&server->socket, &received_packet, WAIT_FOREVER) != CY_RSLT_SUCCESS)
        {
            continue;
        }

        /* Get a pointer to the data in the packet */
        packet_get_data(received_packet, 0, (uint8_t**) &request_header, &data_length, &available_data_length);

        if (data_length != available_data_length)
        {
            /* Fragmented packets are not supported */
            packet_delete(received_packet);
            continue;
        }

        /* Check if the received data length is at least the size of dhcp_header_t. */
        /* The Options field in the DHCP header is of variable length. Look for the "DHCP Message Type" option that is 3 octets in size (code, length and type). */
        if (data_length < (sizeof(dhcp_header_t) - sizeof(request_header->options) + 3))
        {
            packet_delete(received_packet);
            continue;
        }

        /* Check if the option in the DHCP header is "DHCP Message Type", code value for the "DHCP Message Type" option is 53 as per RFC2132 */
        if (request_header->options[0] != DHCP_MESSAGETYPE_OPTION_CODE)
        {
            packet_delete(received_packet);
            continue;
        }

        /* Check the DHCP command */
        switch (request_header->options[2])
        {
            case DHCPDISCOVER:
            {
                dhcp_header_t           *reply_header   = NULL;
                uint16_t                available_space = 0;
                cy_lwip_mac_addr_t      client_mac_address;
                cy_lwip_ip_address_t    client_ip_address;
                uint32_t                temp;

                cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): DHCPDISCOVER \n", __FUNCTION__ );
                /* Create the reply packet */
                if (packet_create_udp(&transmit_packet, (uint8_t**) &reply_header, &available_space) != CY_RSLT_SUCCESS)
                {
                    /* Cannot reply - release the incoming packet */
                    packet_delete( received_packet );
                    break;
                }

                /* Copy the DHCP header content from the received discover packet into the reply packet */
                memcpy(reply_header, request_header, sizeof(dhcp_header_t) - sizeof(reply_header->options));

                /* Finished with the received discover packet - release it */
                packet_delete(received_packet);

                /* Now construct the OFFER response */
                reply_header->opcode = BOOTP_OP_REPLY;

                /* Clear the DHCP options list */
                memset( &reply_header->options, 0, sizeof( reply_header->options ) );

                /* Record the client MAC address */
                memcpy( &client_mac_address, request_header->client_hardware_addr, sizeof( client_mac_address ) );

                /* Check whether the device is already cached */
                if (!get_client_ip_address_from_cache( &client_mac_address, &client_ip_address ))
                {
                    /* Address not found in cache. Use the next available IP address */
                    client_ip_address.version = CY_LWIP_IP_VER_V4;
                    client_ip_address.ip.v4   = next_available_ip_addr;
                }

                /* Create the IP address for the offer */
                temp = htonl(client_ip_address.ip.v4);
                memcpy(reply_header->your_ip_addr, &temp, sizeof(temp));

                /* Copy the magic DHCP number */
                memcpy(reply_header->magic, dhcp_magic_cookie, 4);

                /* Add options */
                option_ptr     = (char *) &reply_header->options;
                option_ptr     = MEMCAT( option_ptr, dhcp_offer_option_buff, 3 );                                   /* DHCP message type            */
                option_ptr     = MEMCAT( option_ptr, server_ip_addr_option_buff, 6 );                               /* Server identifier            */
                option_ptr     = MEMCAT( option_ptr, lease_time_option_buff, 6 );                                   /* Lease time                   */
                option_ptr     = MEMCAT( option_ptr, subnet_mask_option_buff, 6 );                                  /* Subnet mask                  */
                option_ptr     = (char*)MEMCAT( option_ptr, wpad_option_buff, sizeof(wpad_option_buff) );           /* Web proxy auto-discovery URL */
                /* Copy the local IP address into the router & DNS server options */
                memcpy( option_ptr, server_ip_addr_option_buff, 6 );                                                /* Router (gateway)             */
                option_ptr[0]  = 3;                                                                                 /* Router ID                    */
                option_ptr    += 6;
                memcpy( option_ptr, server_ip_addr_option_buff, 6 );                                                /* DNS server                   */
                option_ptr[0]  = 6;                                                                                 /* DNS server ID                */
                option_ptr    += 6;
                option_ptr     = MEMCAT( option_ptr, mtu_option_buff, 4 );                                          /* Interface MTU                */
                option_ptr[0]  = (char) DHCP_END_OPTION_CODE;                                                       /* End options                  */
                option_ptr++;

                /* Send OFFER reply packet */
                packet_set_data_end(transmit_packet, (uint8_t*) option_ptr);
                if (cy_udp_send(&server->socket, &broadcast_addr, IPPORT_DHCPC, transmit_packet) != CY_RSLT_SUCCESS)
                {
                    packet_delete(transmit_packet);
                }
            }
            break;

            case DHCPREQUEST:
            {
                cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): DHCPREQUEST \n", __FUNCTION__ );
                /* REQUEST command - send back ACK or NAK */
                uint32_t                temp;
                dhcp_header_t           *reply_header;
                uint16_t                available_space;
                cy_lwip_mac_addr_t      client_mac_address;
                cy_lwip_ip_address_t    given_ip_address;
                cy_lwip_ip_address_t    requested_ip_address;
                bool                    next_avail_ip_address_used = false;
                const uint8_t           *find_option_ptr;

                /* Check if the REQUEST is for this server */
                find_option_ptr = find_option( request_header, DHCP_SERVER_IDENTIFIER_OPTION_CODE );
                if ( ( find_option_ptr != NULL ) && ( GET_IPV4_ADDRESS( local_ip_address ) != htobe32( LWIP_MAKEU32( find_option_ptr[3], find_option_ptr[2], find_option_ptr[1], find_option_ptr[0] ) ) ) )
                {
                    break; /* Server ID does not match the local IP address */
                }

                /* Create the reply packet */
                if ( packet_create_udp(&transmit_packet, (uint8_t**) &reply_header, &available_space ) != CY_RSLT_SUCCESS )
                {
                    /* Cannot reply - release the incoming packet */
                    packet_delete( received_packet );
                    break;
                }

                /* Copy the DHCP header content from the received request packet into the reply packet */
                memcpy( reply_header, request_header, sizeof(dhcp_header_t) - sizeof(reply_header->options) );
                
                /* Initialize requested address*/
                memset( &requested_ip_address, 0, sizeof(requested_ip_address));
                
                /* Record the client MAC address */
                memcpy( &client_mac_address, request_header->client_hardware_addr, sizeof( client_mac_address ) );


                /* Locate the requested address in the options and keep the requested address */
                requested_ip_address.version = CY_LWIP_IP_VER_V4;
                find_option_ptr = find_option( request_header, DHCP_REQUESTED_IP_ADDRESS_OPTION_CODE );
                if( find_option_ptr != NULL )
                {
                    requested_ip_address.ip.v4   = ntohl( LWIP_MAKEU32( find_option_ptr[3], find_option_ptr[2], find_option_ptr[1], find_option_ptr[0] ) );
                }

                /* Delete the received packet. Not required anymore */
                packet_delete( received_packet );

                reply_header->opcode = BOOTP_OP_REPLY;

                /* Blank options list */
                memset( &reply_header->options, 0, sizeof( reply_header->options ) );

                /* Copy the DHCP magic number into the packet */
                memcpy( reply_header->magic, dhcp_magic_cookie, 4 );

                option_ptr = (char *) &reply_header->options;

                /* Check if device is cached. If it is, give the previous IP address. Otherwise, give the next available IP address */
                if ( !get_client_ip_address_from_cache( &client_mac_address, &given_ip_address ) )
                {
                    /* Address not found in cache. Use the next available IP address */
                    next_avail_ip_address_used = true;
                    given_ip_address.version   = CY_LWIP_IP_VER_V4;
                    given_ip_address.ip.v4     = next_available_ip_addr;
                }

                /* Check if the requested IP address matches the assigned address */
                if ( memcmp( &requested_ip_address.ip.v4, &given_ip_address.ip.v4, sizeof( requested_ip_address.ip.v4 ) ) != 0 )
                {
                    /* Request is not for the assigned IP - force the client to take the next available IP address by sending NAK */
                    /* Add appropriate options */
                    option_ptr = (char*)MEMCAT( option_ptr, dhcp_nak_option_buff, 3 );             /* DHCP message type */
                    option_ptr = (char*)MEMCAT( option_ptr, server_ip_addr_option_buff, 6 );       /* Server identifier */
                    memset( reply_header->your_ip_addr, 0, sizeof( reply_header->your_ip_addr ) ); /* Clear the IP address     */
                }
                else
                {
                    /* Request is for the next available IP */
                    /* Add appropriate options */
                    option_ptr     = (char*)MEMCAT( option_ptr, dhcp_ack_option_buff, 3 );                              /* DHCP message type            */
                    option_ptr     = (char*)MEMCAT( option_ptr, server_ip_addr_option_buff, 6 );                        /* Server identifier            */
                    option_ptr     = (char*)MEMCAT( option_ptr, lease_time_option_buff, 6 );                            /* Lease time                   */
                    option_ptr     = (char*)MEMCAT( option_ptr, subnet_mask_option_buff, 6 );                           /* Subnet mask                  */
                    option_ptr     = (char*)MEMCAT( option_ptr, wpad_option_buff, sizeof(wpad_option_buff) );           /* Web proxy auto-discovery URL */
                    /* Copy the local IP address into the router & DNS server options */
                    memcpy( option_ptr, server_ip_addr_option_buff, 6 );                                                /* Router (gateway)             */
                    option_ptr[0]  = 3;                                                                                 /* Router ID                    */
                    option_ptr    += 6;
                    memcpy( option_ptr, server_ip_addr_option_buff, 6 );                                                /* DNS server                   */
                    option_ptr[0]  = 6;                                                                                 /* DNS server ID                */
                    option_ptr    += 6;
                    option_ptr     = (char*)MEMCAT( option_ptr, mtu_option_buff, 4 );                                   /* Interface MTU                */

                    /* Create the IP address for the Offer */
                    temp = htonl(given_ip_address.ip.v4);
                    memcpy( reply_header->your_ip_addr, &temp, sizeof( temp ) );

                    /* Increment the next available IP address only if not found in cache */
                    if ( next_avail_ip_address_used == true )
                    {
                        do
                        {
                            next_available_ip_addr = subnet | ( ( next_available_ip_addr + 1 ) & ip_mask );
                        } while ( next_available_ip_addr == GET_IPV4_ADDRESS(local_ip_address) );
                    }

                    /* Cache the client */
                    add_client_to_cache( &client_mac_address, &given_ip_address );
                }

                option_ptr[0] = (char) DHCP_END_OPTION_CODE; /* End options */
                option_ptr++;

                /* Send the reply packet */
                packet_set_data_end( transmit_packet, (uint8_t*) option_ptr );
                if (cy_udp_send( &server->socket, &broadcast_addr, IPPORT_DHCPC, transmit_packet ) != CY_RSLT_SUCCESS)
                {
                    packet_delete( transmit_packet );
                }
            }
            break;

            default:
                /* Unknown packet type - release the received packet */
                packet_delete( received_packet );
            break;
        }
    }
    cy_rtos_exit_thread();
}

/**
 *  Finds a specified DHCP option
 *
 *  Searches the given DHCP request and returns a pointer to the
 *  specified DHCP option data, or NULL if not found
 *
 * @param[out] request    : DHCP request structure
 * @param[in]  option_num : DHCP option number to find
 *
 * @return Pointer to the DHCP option data, or NULL if not found
 */
static const uint8_t* find_option( const dhcp_header_t* request, uint8_t option_num )
{
    const uint8_t* option_ptr = request->options;
    while ( ( option_ptr[0] != DHCP_END_OPTION_CODE ) &&                               /* Check for the end-of-options flag */
            ( option_ptr[0] != option_num ) &&                                         /* Check for the matching option number */
            ( option_ptr < ( (const uint8_t*) request ) + sizeof( dhcp_header_t ) ) )  /* Check for buffer overrun */
    {
        option_ptr += option_ptr[1] + 2;
    }

    /* Was the option found? */
    if ( option_ptr[0] == option_num )
    {
        return &option_ptr[2];
    }
    return NULL;
}

/**
 *  Searches the cache for a given MAC address to find the matching IP address
 *
 * @param[in]  client_mac_address : MAC address to search for
 * @param[out] client_ip_address  : Receives any IP address which is found
 *
 * @return true if found; false otherwise
 */
static bool get_client_ip_address_from_cache( const cy_lwip_mac_addr_t* client_mac_address, cy_lwip_ip_address_t* client_ip_address )
{
    uint32_t a;

    /* Check whether the device is already cached */
    for ( a = 0; a < DHCP_IP_ADDRESS_CACHE_MAX; a++ )
    {
        if ( memcmp( &cached_mac_addresses[ a ], client_mac_address, sizeof( *client_mac_address ) ) == 0 )
        {
            *client_ip_address = cached_ip_addresses[ a ];
            return true;
        }
    }

    return false;
}

/**
 *  Adds the MAC and IP addresses of a client to cache
 *
 * @param[in] client_mac_address : MAC address of the client to store
 * @param[in] client_ip_address  : IP address of the client to store
 *
 * @return CY_RSLT_SUCCESS
 */
static cy_rslt_t add_client_to_cache( const cy_lwip_mac_addr_t* client_mac_address, const cy_lwip_ip_address_t* client_ip_address )
{
    uint32_t a;
    uint32_t first_empty_slot;
    uint32_t cached_slot;

    /* Search for empty slot in cache */
    for ( a = 0, first_empty_slot = DHCP_IP_ADDRESS_CACHE_MAX, cached_slot = DHCP_IP_ADDRESS_CACHE_MAX; a < DHCP_IP_ADDRESS_CACHE_MAX; a++ )
    {
        /* Check for the matching MAC address */
        if ( memcmp( &cached_mac_addresses[ a ], client_mac_address, sizeof( *client_mac_address ) ) == 0 )
        {
            /* Cached device found */
            cached_slot = a;
            break;
        }
        else if ( first_empty_slot == DHCP_IP_ADDRESS_CACHE_MAX && memcmp( &cached_mac_addresses[ a ], &empty_cache, sizeof(cy_lwip_mac_addr_t) ) == 0 )
        {
            /* Device not found in cache. Return the first empty slot */
            first_empty_slot = a;
        }
    }

    if ( cached_slot != DHCP_IP_ADDRESS_CACHE_MAX )
    {
        /* Update the IP address of the cached device */
        cached_ip_addresses[cached_slot] = *client_ip_address;
    }
    else if ( first_empty_slot != DHCP_IP_ADDRESS_CACHE_MAX )
    {
        /* Add device to the first empty slot */
        cached_mac_addresses[ first_empty_slot ] = *client_mac_address;
        cached_ip_addresses [ first_empty_slot ] = *client_ip_address;
    }
    else
    {
        /* Cache is full. Add the device to slot 0 */
        cached_mac_addresses[ 0 ] = *client_mac_address;
        cached_ip_addresses [ 0 ] = *client_ip_address;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * Converts a unsigned 32-bit long int to a decimal string
 *
 * @param value[in]      : Unsigned long to be converted.
 * @param output[out]    : Buffer which will receive the decimal string. A terminating 'null' is added. Ensure that there is space in the buffer for this.
 * @param min_length[in] : Minimum number of characters to output (zero padding will apply if required).
 * @param max_length[in] : Maximum number of characters to output. The max number of characters it can have is of the length of (ULONG_MAX + 1).
 *
 * @return The number of characters returned (excluding terminating null)
 *
 */
uint8_t unsigned_to_decimal_string( uint32_t value, char* output, uint8_t min_length, uint8_t max_length )
{
    uint8_t digits_left;
    char buffer[sizeof(ULONG_MAX_STR) + 1] = ULONG_MIN_STR; /* Buffer for storing ULONG_MAX with +1 for the sign */

    if ( output == NULL )
    {
        return 0;
    }

    max_length = (uint8_t) MIN( max_length, sizeof( buffer ) );
    digits_left = max_length;
    do
    {
        --digits_left;
        buffer[ digits_left ] = (char) (( value % 10 ) + '0');
        value = value / 10;
    } while ( ( value != 0 ) && ( digits_left != 0 ) );

    digits_left = (uint8_t) MIN( ( max_length - min_length ), digits_left );
    memcpy( output, &buffer[ digits_left ], (size_t)( max_length - digits_left ) );

    /* Add the terminating null */
    output[( max_length - digits_left )] = '\x00';

    return (uint8_t) ( max_length - digits_left );
}


/**
 *  Convert the IPv4 address to a string
 *
 *  @note: String is 16 bytes including the terminating null
 *
 * @param[out] buffer       : Buffer which will receive the IPv4 string
 * @param[in]  ipv4_address : IPv4 address to convert
 */
static void ipv4_to_string( char* buffer, uint32_t ipv4_address )
{
    uint8_t* ip = (uint8_t*)&ipv4_address;
    unsigned_to_decimal_string(ip[0], buffer, 3, 3);
    *(buffer + 3) = '.';
    unsigned_to_decimal_string(ip[1], buffer + 4, 3, 3);
    *(buffer + 7) = '.';
    unsigned_to_decimal_string(ip[2], buffer + 8, 3, 3);
    *(buffer + 11) = '.';
    unsigned_to_decimal_string(ip[3], buffer + 12, 3, 3);
}


static cy_rslt_t udp_create_socket(cy_lwip_udp_socket_t *socket, uint16_t port, cy_network_interface_context *iface_context)
{
    err_t status;
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    memset( socket, 0, sizeof(cy_lwip_udp_socket_t) );

    /* Call the wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    socket->conn_handler = netconn_new( NETCONN_UDP );
    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "socket->conn_handler:[%p] \n", socket->conn_handler);

    if( socket->conn_handler == NULL )
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to create UDP socket \n");
        return CY_RSLT_NETWORK_SOCKET_CREATE_FAIL;
    }

    /* Call the wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "UDP socket :[%d]\n", port);
    /* Bind it to the designated port and IP address */
    status = netconn_bind( socket->conn_handler, IP_ANY_TYPE, port );
    if( status != ERR_OK )
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "socket bind failed \n");
        netconn_delete( socket->conn_handler );
        socket->conn_handler = NULL;
        return CY_RSLT_NETWORK_SOCKET_ERROR;
    }
    socket->is_bound  = true;
    socket->type = iface_context->iface_type;
    socket->index = iface_context->iface_idx;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t udp_delete_socket(cy_lwip_udp_socket_t *socket)
{
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    if (socket->conn_handler == NULL)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Error : Socket deletion failed due to invalid socket \n");
        return CY_RSLT_NETWORK_INVALID_SOCKET;
    }

    /* Call the wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    /* Note: No need to check return value of netconn_delete. It always returns ERR_OK */
    netconn_delete(socket->conn_handler);
    socket->conn_handler = NULL;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t udp_receive(cy_lwip_udp_socket_t *socket, cy_lwip_packet_t** packet, uint32_t timeout)
{
    err_t status;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

    if (socket->conn_handler == NULL)
    {
        return CY_RSLT_NETWORK_SOCKET_ERROR;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "socket->conn_handler:[%p] \n", socket->conn_handler);

    netconn_set_recvtimeout(socket->conn_handler, (int)timeout);
    status = netconn_recv(socket->conn_handler, packet);
    if ( status != ERR_OK )
    {
        return CY_RSLT_NETWORK_SOCKET_ERROR;
    }
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t packet_get_data(cy_lwip_packet_t *packet, uint16_t offset, uint8_t** data, uint16_t* fragment_available_data_length, uint16_t *total_available_data_length)
{
    s8_t get_next_result;

    netbuf_first( packet );
    *total_available_data_length = (uint16_t)( netbuf_len(packet) - offset );

    do
    {
        uint16_t frag_size = packet->ptr->len;
        *data        = packet->ptr->payload;

        if ( frag_size == 0 && *total_available_data_length == 0 )
        {
            *data                           = NULL;
            *fragment_available_data_length = 0;
            *total_available_data_length    = 0;
            return CY_RSLT_SUCCESS;
        }
        else if ( offset < frag_size )
        {
            *data += offset;
            *fragment_available_data_length = (uint16_t)(frag_size - offset);
            return CY_RSLT_SUCCESS;
        }
        else
        {
            offset = (uint16_t)(offset - frag_size);
            get_next_result = netbuf_next( packet );
        }
    } while ( get_next_result != -1 );

    return CY_RSLT_NETWORK_CORRUPT_BUFFER;
}

static cy_rslt_t packet_set_data_end(cy_lwip_packet_t *packet, uint8_t* data_end)
{
    packet->ptr->len     = (uint16_t) ( data_end - ( (uint8_t*) packet->ptr->payload ) );
    packet->p->tot_len = packet->p->len;

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t packet_delete(cy_lwip_packet_t* packet)
{
    netbuf_delete( packet );
    return CY_RSLT_SUCCESS;
}

static cy_rslt_t cy_udp_send(cy_lwip_udp_socket_t* socket, const cy_lwip_ip_address_t* address, uint16_t port, cy_lwip_packet_t* packet)
{
    ip_addr_t temp;
    err_t status;
    cy_rslt_t result;

    if((socket == NULL) || (address == NULL) || (packet == NULL))
    {
        return CY_RSLT_NETWORK_BAD_ARG;
    }

    /* Associate the UDP socket with the specific remote IP address and port */
    cy_ip_to_lwip(&temp, address);

    /* Call the wifi-mw-core network activity function to resume the network stack */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    status = netconn_connect(socket->conn_handler, &temp, port);
    if ( status != ERR_OK )
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket error unable to associate socket with IP address \n");
        return CY_RSLT_NETWORK_SOCKET_ERROR;
    }

    /* Total length and a length must be equal for a packet to be valid */
    packet->p->len = packet->p->tot_len;

    /* Send the packet via the UDP socket */
    result = internal_udp_send(socket->conn_handler, packet, (cy_network_hw_interface_type_t)socket->type, socket->index);
    if ( result != CY_RSLT_SUCCESS )
    {
        /* Call the wifi-mw-core network activity function to resume the network stack */
        cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

        netconn_disconnect(socket->conn_handler);
        return result;
    }

    netbuf_delete( packet );

    /* Call the wifi-mw-core network activity function to resume the network stack */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    /* Return to the disconnected state
     * Note: Ignore the return for this because CY_RSLT_SUCCESS MUST be returned; otherwise, the caller may attempt to
     * free the packet a second time.
     */
    netconn_disconnect(socket->conn_handler);
    return CY_RSLT_SUCCESS;
}

void cy_ip_to_lwip(ip_addr_t *dest, const cy_lwip_ip_address_t *src)
{
    if(src->version == CY_LWIP_IP_VER_V4)
    {
        ip_addr_set_ip4_u32(dest, htonl(GET_IPV4_ADDRESS(*src)));
    }
}

static cy_rslt_t internal_udp_send(struct netconn* handler, cy_lwip_packet_t* packet,
        cy_network_hw_interface_type_t type, uint8_t index)
{
    err_t status;
    if(cy_rtos_get_mutex(&dhcp_mutex, CY_DHCP_MAX_MUTEX_WAIT_TIME_MS) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_DHCP_WAIT_TIMEOUT;
    }

    /* Call the wifi-mw-core network activity function to resume the network stack */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    /* Bind the interface to the socket */
    udp_bind_netif(handler->pcb.udp, cy_network_get_nw_interface(type, index));

    /* Send a packet */
    packet->p->len = packet->p->tot_len;
    status = netconn_send( handler, packet );
    if (cy_rtos_set_mutex(&dhcp_mutex) != CY_RSLT_SUCCESS)
    {
        return CY_RSLT_NETWORK_DHCP_MUTEX_ERROR;
    }
    netbuf_free(packet);
    return ((status == CY_RSLT_SUCCESS) ? CY_RSLT_SUCCESS : CY_RSLT_NETWORK_SOCKET_ERROR);
}

static cy_rslt_t packet_create_udp(cy_lwip_packet_t** packet, uint8_t** data, uint16_t* available_space )
{
    return internal_packet_create(packet, MAX_UDP_PAYLOAD_SIZE, data, available_space);
}

static cy_rslt_t internal_packet_create( cy_lwip_packet_t** packet, uint16_t content_length, uint8_t** data, uint16_t* available_space )
{
    int i = 0;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );
    for (i = 0; i < ALLOCATE_PACKET_TIMEOUT; ++i)
    {
        *packet = netbuf_new();
        if (*packet != NULL)
        {
            *data = netbuf_alloc(*packet, content_length);
            if (*data != NULL)
            {
                *available_space = content_length;
                return CY_RSLT_SUCCESS;
            }
            netbuf_delete(*packet);
            *packet = NULL;
            *available_space = 0;
        }
        cy_rtos_delay_milliseconds(1);
    }

    *available_space = 0;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return CY_RSLT_NETWORK_DHCP_TIMEOUT;
}
#endif //LWIP_IPV4
