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
 * Implementation of socket layer APIs using Anycloud
 * Secure sockets library used by IPERF
 */
#include "cy_secure_sockets.h"
#include "iperf_sockets.h"
#ifdef COMPONENT_LWIP
#include "lwipopts.h"
#endif
#ifdef COMPONENT_NETXDUO
#include "nx_user.h"
#include "whd.h"
#endif
#include "cyabs_rtos_impl.h"
#include "cyabs_rtos.h"
#include <time.h>

#ifdef COMPONENT_NETXDUO
#ifndef DEFAULT_TCP_WINDOW_SIZE
#define DEFAULT_TCP_WINDOW_SIZE     (16 * 1024)
#endif

#ifndef NX_TCP_MAXIMUM_TX_QUEUE
/* Default value set is 20 */
#define NX_TCP_MAXIMUM_TX_QUEUE     (20)
#endif /* NX_TCP_MAXIMUM_TX_QUEUE */

#ifndef COMPONENT_CAT5
#define TX_BUFFER_PAYLOAD_SIZE      (WHD_LINK_MTU)
#define CY_NX_TCP_SND_BUF           (NX_TCP_MAXIMUM_TX_QUEUE*TX_BUFFER_PAYLOAD_SIZE)
#endif /* !COMPONENT_CAT5 */

#ifdef COMPONENT_CAT5
#define TX_BUFFER_PAYLOAD_SIZE      (WHD_LINK_MTU + 20)
/* NX_TCP_MAXIMUM_TX_QUEUE is not configurable in CAT5(H1-CP) devices. Default value set is 20 */
#define CY_NX_TCP_SND_BUF           (NX_TCP_MAXIMUM_TX_QUEUE*TX_BUFFER_PAYLOAD_SIZE)
#endif /* COMPONENT_CAT5 */

#endif

#define IPERF_SOCKET_DEBUG(x) //printf x
#define IPERF_SOCKET_ERROR(x) //printf x

#ifndef MAX_BSD_SOCKETS
#define MAX_BSD_SOCKETS                      (5)
#endif

#define BUFFER_LENGTH                        (100)
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
#define UDP_RECEIVE_TIMEOUT                  (500)
#else
#define UDP_RECEIVE_TIMEOUT                  (10000)
#endif
/* convert into ms */
#define CONVERT_TO_MS                        (1000)
#define ANYCLOUD_RECV_SOCKET_TIMEOUT_ERROR   (-3)
/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    int               protocol_type;
    bool              available;
    uint32_t          select_timeout;
    struct sockaddr   remoteAddress;
    cy_socket_t       client_socket;
    cy_socket_t       socket;
    char              rcv_buffer[BUFFER_LENGTH];
    int               rcv_buffer_len;
    int               id;
} bsd_socket_t;

static bsd_socket_t   sockets[MAX_BSD_SOCKETS];
static bool           socket_layer_inited = false;
cy_mutex_t            sockets_mutex;
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
static uint32_t       udp_recv_retry = 0;
#endif

void iperf_network_init( void * networkInterface )
{
    if(socket_layer_inited != true)
    {
        sockets_layer_init();
    }
}

void sockets_layer_init( void )
{
    cy_rslt_t  result = CY_RSLT_SUCCESS;

    /* Initialize secure socket library */
    cy_socket_init();

    result = cy_rtos_init_mutex(&sockets_mutex);
    if( CY_RSLT_SUCCESS != result )
    {
        cy_socket_deinit();
        return;
    }

    int a;
    for ( a = 0; a < MAX_BSD_SOCKETS; ++a )
    {
        sockets[a].available = true;
        sockets[a].select_timeout = 0;
        sockets[a].id = a;
    }

    socket_layer_inited = true;
}

bsd_socket_t* find_free_socket()
{
    int a = 0;
    bsd_socket_t* free_socket = NULL;
    cy_rslt_t     result = CY_RSLT_SUCCESS;

    result = cy_rtos_get_mutex(&sockets_mutex, CY_RTOS_NEVER_TIMEOUT);
    if( CY_RSLT_SUCCESS != result )
    {
        return free_socket;
    }
    for ( ; a < MAX_BSD_SOCKETS; ++a )
    {
        if ( sockets[a].available == true )
        {
            free_socket = &sockets[a];
            memset(free_socket, 0, sizeof(bsd_socket_t));
            free_socket->available        = false;
            free_socket->select_timeout   = 0;
            free_socket->id = a;
            break;
        }
    }

    result = cy_rtos_set_mutex(&sockets_mutex);
    if( CY_RSLT_SUCCESS != result )
    {
        if( free_socket != NULL )
        {
            free_socket->available = true;
        }
        return NULL;
    }

    return free_socket;
}

void convert_from_lwip_to_secure_sockets(int protocolfamily, int type, int* domain, int* stream_type)
{
    if(protocolfamily == AF_INET)
    {
        *domain = CY_SOCKET_DOMAIN_AF_INET;
    }
    else
    {
        *domain = CY_SOCKET_DOMAIN_AF_INET6;
    }

    if(type == SOCK_STREAM)
    {
        *stream_type = CY_SOCKET_TYPE_STREAM;
    }
    else
    {
        *stream_type = CY_SOCKET_TYPE_DGRAM;
    }
}

int iperf_socket( int protocolFamily, int type, int protocol )
{
    cy_rslt_t result;
    int id=0;
    bsd_socket_t* free_socket;
    int domain, stream_type;
    IPERF_SOCKET_DEBUG(("iperf_socket : Creating sockets \n"));

    /* Find free socket */
    free_socket = find_free_socket();
    if(free_socket == NULL)
    {
        IPERF_SOCKET_DEBUG(("Failed to find free socket \n"));
        return -1;
    }

    id = free_socket->id;

    convert_from_lwip_to_secure_sockets(protocolFamily, type, &domain, &stream_type);

    switch ( type )
    {
        case SOCK_DGRAM:
            free_socket->protocol_type = CY_SOCKET_IPPROTO_UDP;

            result = cy_socket_create(domain, stream_type, free_socket->protocol_type, &free_socket->socket);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_DEBUG(("cy_socket_create failed with error %d\r\n", result));
                id = -1;
                goto exit;
            }
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
            udp_recv_retry = 0;
#endif
            break;

        case SOCK_STREAM:
            free_socket->protocol_type = CY_SOCKET_IPPROTO_TCP;

            result = cy_socket_create(domain, stream_type, free_socket->protocol_type, &free_socket->socket);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_DEBUG(("cy_socket_create failed with error %d\r\n", result));
                id = -1;
                goto exit;
            }
            break;
        default:
            IPERF_SOCKET_DEBUG(("Invalid socket type \n"));
            id = -1;
            goto exit;
    }

    IPERF_SOCKET_DEBUG(("iperf_socket : Done \n"));
    return id;

exit:
    IPERF_SOCKET_DEBUG(("iperf_socket : Done with error \n"));
    memset(free_socket,0,sizeof(bsd_socket_t));
    free_socket->available = true;
    return id;
}


int iperf_write( int sockID, const char *msg, size_t msgLength )
{
    uint32_t  bytes_sent = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    IPERF_SOCKET_DEBUG(("iperf_write : Write on socket with length : %d \r\n", msgLength));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            /* cy_socket_send may return CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK for lower network speeds which means that the socket send buffer is full.
             * Example: This situation can occur with ethernet link with 10M speed. The application posts packets to network stack at a faster rate than
             * it can be sent out through the network and the send buffer might overflow returning CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK.
             * In such situation, we can send the packet again so that when the send buffer is freed after sometime, cy_socket_send returns success.
             */
            do
            {
                result = cy_socket_send(sockets[sockID].socket, msg, msgLength, 0, &bytes_sent);
            } while(result == CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK);

            if( result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_send failed with error : %ld \n", result));
                return -1;
            }
            break;
        }
        case CY_SOCKET_IPPROTO_UDP:
        {
            cy_socket_sockaddr_t address;

            struct sockaddr* socket_addr = &sockets[sockID].remoteAddress;

            /* Fill Port number, IP address into cy_socket_sockaddr_t format */
            address.port = ((uint16_t)socket_addr->sa_data[0]) << 8;
            address.port |= socket_addr->sa_data[1];
            address.ip_address.version = CY_SOCKET_IP_VER_V4;
            address.ip_address.ip.v4 = (socket_addr->sa_data[2]) | (socket_addr->sa_data[3] << 8) | (socket_addr->sa_data[4] << 16) | (socket_addr->sa_data[5] << 24);

            result = cy_socket_sendto(sockets[sockID].socket, msg, msgLength, 0, &address, sizeof(cy_socket_sockaddr_t), &bytes_sent);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_sendto failed with error : %ld \n", result));
                return -1;
            }

            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            return -1;
            break;
        }
    }

    IPERF_SOCKET_DEBUG(("iperf_write : Done, Status %d\n", bytes_sent));

    return bytes_sent;
}

int iperf_read(int sockID, void *mem, size_t len)
{
    uint32_t bytes_received = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    uint32_t address_length = 0;

    IPERF_SOCKET_DEBUG(("IPERF_read with length : %d\n", len));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            if(sockets[sockID].client_socket)
            {
                result = cy_socket_recv(sockets[sockID].client_socket, mem, len, CY_SOCKET_FLAGS_NONE, &bytes_received);
                if(result != CY_RSLT_SUCCESS)
                {
                    IPERF_SOCKET_ERROR(("cy_socket_recv failed with error : %ld \n", result));
                    return -1;
                }
            }
            break;
        }
        case CY_SOCKET_IPPROTO_UDP:
        {
            if(sockets[sockID].socket)
            {
                cy_socket_sockaddr_t src_addr;
                struct sockaddr* socket_addr = &sockets[sockID].remoteAddress;

                /* Fill port number, IP address in cy_socket_sockaddr_t format */
                src_addr.ip_address.version = CY_SOCKET_IP_VER_V4;
                src_addr.port = ((uint16_t)socket_addr->sa_data[0]) << 8;
                src_addr.port |= socket_addr->sa_data[1];
                src_addr.ip_address.ip.v4 = (socket_addr->sa_data[2]) | (socket_addr->sa_data[3] << 8) | (socket_addr->sa_data[4] << 16) | (socket_addr->sa_data[5] << 24);
                address_length = sizeof(cy_socket_sockaddr_t);

                result = cy_socket_recvfrom(sockets[sockID].socket, mem, len, CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER, &src_addr, &address_length, &bytes_received);
                if(result != CY_RSLT_SUCCESS)
                {
                    IPERF_SOCKET_ERROR(("cy_socket_recvfrom failed with error : %ld \n", result));
                    return -1;
                }
            }
            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            bytes_received = -1;
            break;
        }
    }

    IPERF_SOCKET_DEBUG(("iperf_read : Done, Status %d \n", bytes_received));

    return bytes_received;
}

int iperf_getpeername(int sockID, struct sockaddr *remoteAddress, uint32_t *addressLength)
{
    IPERF_SOCKET_DEBUG(("getpeername\n"));
    return 0;
}

int iperf_getsockname(int sockID, struct sockaddr *localAddress, uint32_t *addressLength)
{
    IPERF_SOCKET_DEBUG(("getsockname\n"));
    return 0;
}

int iperf_recvfrom(int sockID, char *buffer, size_t buffersize, int flags,struct sockaddr *fromAddr, uint32_t *fromAddrLen)
{
    uint32_t bytes_received = 0;
    uint32_t address_length = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
    uint32_t timeout = DEFAULT_IPERF_SERVER_TIMEOUT_SEC*1000;
#else
#ifdef COMPONENT_NETXDUO
    uint32_t timeout = NX_WAIT_FOREVER;
#else
    int timeout = 0;
#endif
#endif
    IPERF_SOCKET_DEBUG(("iperf_recvfrom : Read data with buffer length : %d \r\n", buffersize));

    cy_socket_sockaddr_t src_addr;
    address_length = sizeof(cy_socket_sockaddr_t);

    /* set the receive timeout to block till UDP datagram is received from Client */
    result = cy_socket_setsockopt(sockets[sockID].socket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_RCVTIMEO, &timeout, sizeof(int));
    if ( result != CY_RSLT_SUCCESS)
    {
        IPERF_SOCKET_ERROR(("Failed to set RCV timeout option : %d \n", result));
        return -1;
    }

    result = cy_socket_recvfrom(sockets[sockID].socket, buffer, buffersize, CY_SOCKET_FLAGS_RECVFROM_NONE, &src_addr, &address_length, &bytes_received);
    if(result != CY_RSLT_SUCCESS)
    {
        IPERF_SOCKET_ERROR(("cy_socket_recvfrom failed with error : %ld \n", result));
        return -1;
    }

    /* Copy received peer port number and IP address into fromAddr */
    fromAddr->sa_data[0] = (src_addr.port >> 8) & 0XFF;
    fromAddr->sa_data[1] = (src_addr.port) & 0xFF;
    memcpy(&fromAddr->sa_data[2], &src_addr.ip_address.ip.v4, sizeof(uint32_t));

    IPERF_SOCKET_DEBUG(("iperf_recvfrom : Done, Status : %d \r\n", bytes_received));

    /* set the UDP receive timeout */
    timeout = UDP_RECEIVE_TIMEOUT;
    result = cy_socket_setsockopt(sockets[sockID].socket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_RCVTIMEO, &timeout, sizeof(int));
    if ( result != CY_RSLT_SUCCESS)
    {
        IPERF_SOCKET_ERROR(("Failed to set RCV timeout option : %d \n", result));
        return -1;
    }

    return bytes_received;
}

int iperf_recv(int sockID, void *rcvBuffer, size_t bufferLength, int flags)
{
    uint32_t bytes_received=0;
    uint32_t address_length = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int received_data_length = 0;
    int remaining_data_length  = bufferLength;

    IPERF_SOCKET_DEBUG(("iperf_recv : Read data with buffer length : %d\n", bufferLength));

    if((flags & MSG_PEEK) != MSG_PEEK)
    {
        if(sockets[sockID].rcv_buffer_len != 0)
        {
            memcpy(rcvBuffer, sockets[sockID].rcv_buffer, sockets[sockID].rcv_buffer_len);
            memset(sockets[sockID].rcv_buffer, 0, sizeof(sockets[sockID].rcv_buffer));
            received_data_length += sockets[sockID].rcv_buffer_len;
            remaining_data_length -= sockets[sockID].rcv_buffer_len;
            sockets[sockID].rcv_buffer_len = 0;
        }
    }

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            if(sockets[sockID].client_socket)
            {
                while(received_data_length < bufferLength)
                {
                    result = cy_socket_recv(sockets[sockID].client_socket, ((char*)rcvBuffer) + received_data_length, remaining_data_length, CY_SOCKET_FLAGS_NONE, &bytes_received);
                    if(result != CY_RSLT_SUCCESS)
                    {
                        IPERF_SOCKET_ERROR(("cy_socket_recv failed with error : %d \n", result));
                        return -1;
                    }

                    remaining_data_length -= bytes_received;
                    received_data_length += bytes_received;
                }
            }
            break;
        }
        case CY_SOCKET_IPPROTO_UDP:
        {
            if(sockets[sockID].socket)
            {
                cy_socket_sockaddr_t src_addr;

                struct sockaddr* socket_addr = &sockets[sockID].remoteAddress;

                /* Fill port number, IP address to cy_socket_sockaddr_t format */
                src_addr.port = ((uint16_t)socket_addr->sa_data[0]) << 8;
                src_addr.port |= socket_addr->sa_data[1];
                src_addr.ip_address.version = CY_SOCKET_IP_VER_V4;
                src_addr.ip_address.ip.v4 = (socket_addr->sa_data[2]) | (socket_addr->sa_data[3] << 8) | (socket_addr->sa_data[4] << 16) | (socket_addr->sa_data[5] << 24);
                address_length = sizeof(cy_socket_sockaddr_t);

                while(received_data_length < bufferLength)
                {
                    result = cy_socket_recvfrom(sockets[sockID].socket, ((char*)rcvBuffer) + received_data_length, remaining_data_length, CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER, &src_addr, &address_length, &bytes_received);

                    if(result == CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT)
                    {
                        IPERF_SOCKET_ERROR(("cy_socket_recvfrom failed with Timeout error : %d \n", result));
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
                        udp_recv_retry ++;
                        if(udp_recv_retry >= (DEFAULT_IPERF_SERVER_TIMEOUT_SEC*2))
                        {
                            return -1;
                        }
#endif
                        return ANYCLOUD_RECV_SOCKET_TIMEOUT_ERROR;
                    } else if(result != CY_RSLT_SUCCESS)
                    {
                        IPERF_SOCKET_ERROR(("cy_socket_recvfrom failed with error : %d \n", result));
                        return -1;
                    } else
                    {
                      //DO Nothing
                    }
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
                    udp_recv_retry = 0;
#endif
                    remaining_data_length -= bytes_received;
                    received_data_length += bytes_received;
                }
            }
            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            bytes_received = -1;
            break;
        }
    }

    /* Copy read data to buffer if MSG_PEEK flag is set */
    if((flags & MSG_PEEK) == MSG_PEEK)
    {
        memcpy(sockets[sockID].rcv_buffer + sockets[sockID].rcv_buffer_len, rcvBuffer, bytes_received);
        sockets[sockID].rcv_buffer_len += bytes_received;
    }

    IPERF_SOCKET_DEBUG(("iperf_recv : Done, Status %d \n", bytes_received));

    return bytes_received;
}

int iperf_accept(int sockID, struct sockaddr *ClientAddress, uint32_t *addressLength)
{
    uint32_t address_length = 0;
    cy_socket_sockaddr_t address;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    IPERF_SOCKET_DEBUG(("iperf_accept : Accept new client connection \n"));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            struct sockaddr* socket_addr = ClientAddress;

#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
            uint32_t time_ms = (DEFAULT_IPERF_SERVER_TIMEOUT_SEC*1000)/2;
#endif

            /* Fill port number, IP address into cy_socket_sockaddr_t format */
            address.port = ((uint16_t)socket_addr->sa_data[0]) << 8;
            address.port |= socket_addr->sa_data[1];
            address.ip_address.version = CY_SOCKET_IP_VER_V4;
            address.ip_address.ip.v4 = (socket_addr->sa_data[2]) | (socket_addr->sa_data[3] << 8) | (socket_addr->sa_data[4] << 16) | (socket_addr->sa_data[5] << 24);
            address_length = sizeof(cy_socket_sockaddr_t);

            result = cy_socket_accept(sockets[sockID].socket, &address, &address_length, &sockets[sockID].client_socket);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_accept failed with error : %d \n", result));
                return -1;
            }
#ifdef DEFAULT_IPERF_SERVER_TIMEOUT_SEC
            result = cy_socket_setsockopt(sockets[sockID].client_socket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_RCVTIMEO, &time_ms, sizeof(time_ms));
            if ( result != CY_RSLT_SUCCESS)
            {
               IPERF_SOCKET_ERROR(("Failed to set RCV timeout option for client_socket: %d \n", result));
               return -1;
            }
#endif
            break;
        }
        case CY_SOCKET_IPPROTO_UDP:
        {
            IPERF_SOCKET_DEBUG(("Error: accept called on udp socket \n"));
            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            result = -1;
            break;
        }
    }

    IPERF_SOCKET_DEBUG(("iperf_accept : Done, Status %d\n", result));
    return sockID;
}

int iperf_listen(int sockID, int backlog)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    IPERF_SOCKET_DEBUG(("iperf_listen : Listen on socket \r\n"));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            result = cy_socket_listen(sockets[sockID].socket, backlog);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_listen failed with error : %d \n", result));
                return -1;
            }
            break;
        }
        case CY_SOCKET_IPPROTO_UDP:
        {
            IPERF_SOCKET_DEBUG(("Error: listen called on udp socket \n"));
            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            result = -1;
            break;
        }
    }

    IPERF_SOCKET_DEBUG(("iperf_listen : Done, Status %d\n", result));
    return result;
}

int iperf_bind(int sockID, struct sockaddr *localAddress, uint32_t addressLength)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_socket_sockaddr_t address;

    IPERF_SOCKET_DEBUG(("iperf_bind : Bind to a socket \r\n"));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        case CY_SOCKET_IPPROTO_UDP:
        {

            /* Fill port number, IP address into cy_socket_sockaddr_t format */
            address.port = ((uint16_t)localAddress->sa_data[0]) << 8;
            address.port |= localAddress->sa_data[1];
            address.ip_address.version = CY_SOCKET_IP_VER_V4;
            address.ip_address.ip.v4 = (localAddress->sa_data[2]) | (localAddress->sa_data[3] << 8) | (localAddress->sa_data[4] << 16) | (localAddress->sa_data[5] << 24);

            result  = cy_socket_bind(sockets[sockID].socket, &address, sizeof(cy_socket_sockaddr_t));
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_setsockopt failed with error : %d \n", result));
                return -1;
            }
            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            result = -1;
            break;
        }
    }

    IPERF_SOCKET_DEBUG(("iperf_bind : Done, Status %d\n", result));
    return result;
}

int iperf_connect(int sockID, struct sockaddr *remoteAddress, uint32_t addressLength)
{
    uint32_t address_length = 0;

    cy_rslt_t result = CY_RSLT_SUCCESS;

    IPERF_SOCKET_DEBUG(("iperf_connect : Connect to a server \r\n"));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            cy_socket_sockaddr_t address;

            address.port = ((uint16_t)remoteAddress->sa_data[0]) << 8;
            address.port |= remoteAddress->sa_data[1];
            address.ip_address.version = CY_SOCKET_IP_VER_V4;
            address.ip_address.ip.v4 = (remoteAddress->sa_data[2]) | (remoteAddress->sa_data[3] << 8) | (remoteAddress->sa_data[4] << 16) | (remoteAddress->sa_data[5] << 24);
            address_length = sizeof(cy_socket_sockaddr_t);

            result = cy_socket_connect(sockets[sockID].socket, &address, address_length);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_connect failed with error : %d \n", result));
                return -1;
            }
            break;
        }
       case CY_SOCKET_IPPROTO_UDP:
       {
           memcpy(&sockets[sockID].remoteAddress, remoteAddress, sizeof(struct sockaddr) );
           result = 0;
           break;
       }
       default:
       {
           IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
           result = -1;
           break;
       }
   }

    IPERF_SOCKET_DEBUG(("iperf_connect : Done, Status %d\n", result));
    return result;
}

int iperf_close( int sockID)
{
    int ret = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    IPERF_SOCKET_DEBUG(("iperf_close : Close connection, socket id : %d \n", sockID));

    if(sockets[sockID].available == false)
    {
        switch ( sockets[sockID].protocol_type )
        {
            case CY_SOCKET_IPPROTO_TCP:
            {
                if(sockets[sockID].client_socket != NULL)
                {
                    result = cy_socket_disconnect(sockets[sockID].client_socket, 0);
                    if(result != CY_RSLT_SUCCESS)
                    {
                        IPERF_SOCKET_ERROR(("cy_socket_disconnect failed with error : %d \n", result));
                    }

                    result = cy_socket_delete(sockets[sockID].client_socket);
                    if(result != CY_RSLT_SUCCESS)
                    {
                        IPERF_SOCKET_ERROR(("cy_socket_delete failed with error : %d \n", result));
                    }
                }

                result = cy_socket_disconnect(sockets[sockID].socket, 0);
                if(result != CY_RSLT_SUCCESS)
                {
                    IPERF_SOCKET_ERROR(("cy_socket_disconnect failed with error : %d \n", result));
                }

                result = cy_socket_delete(sockets[sockID].socket);
                if(result != CY_RSLT_SUCCESS)
                {
                    IPERF_SOCKET_ERROR(("cy_socket_delete failed with error : %d \n", result));
                    return -1;
                }
                break;
            }
            case CY_SOCKET_IPPROTO_UDP:
            {
                result = cy_socket_delete(sockets[sockID].socket);
                if(result != CY_RSLT_SUCCESS)
                {
                    IPERF_SOCKET_ERROR(("cy_socket_delete failed with error : %d \n", result));
                    return -1;
                }
                break;
            }
            default:
            {
                IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
                ret = -1;
                break;
            }
        }
        sockets[sockID].available = true;
    }

    IPERF_SOCKET_DEBUG(("iperf_close : Done, Status : %d\n", result));
    return ret;
}

int iperf_setsockopt(int sockID, int option_level, int option_name, const void *option_value, uint32_t option_length)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    IPERF_SOCKET_DEBUG(("iperf_setsockopt : Set socket option : %d\n", option_name));
    /*
     * Socket options used by iperf:
     * Listener.cpp:
     * SO_REUSEADDR
     * IP_ADD_MEMBERSHIP
     * IPV6_ADD_MEMBERSHIP
     * IP_MULTICAST_TTL
     * IPV6_MULTICAST_HOPS
     *
     * PerfSocket.cpp:
     * TCP_CONGESTION
     * IP_MULTICAST_TTL
     * IPV6_MULTICAST_HOPS
     * IP_TOS
     * TCP_NODELAY
     *
     * tcp_window_size:
     * TCP_WINSHIFT
     * TCP_RFC1323
     * SO_RCVBUF
     * SO_SNDBUF
     */

    switch ( option_name )
    {
        case SO_REUSEADDR:
            IPERF_SOCKET_DEBUG(("SO_REUSEADDR\n"));
            break;
        case SO_SNDBUF:
            IPERF_SOCKET_DEBUG(("SO_SNDBUF %ld\n", (*(uint32_t*) option_value)));
            break;
        case SO_RCVBUF:
            IPERF_SOCKET_DEBUG(("SO_RCVBUF %ld\n", (*(uint32_t*) option_value)));
            break;
        case SO_RCVTIMEO:
        {
            struct timeval *tm = (struct timeval*) option_value;
            uint32_t *timeout = (uint32_t*)(&(tm->tv_sec));
            uint32_t time_ms = (uint32_t)(((*timeout) * CONVERT_TO_MS) + ((tm->tv_usec)/CONVERT_TO_MS));

            result = cy_socket_setsockopt(sockets[sockID].socket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_RCVTIMEO, &time_ms, sizeof(time_ms));
            if ( result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("Failed to set RCV timeout option : %d \n", result));
                return -1;
            }
            break;
        }

        case SO_SNDTIMEO:
        {
            struct timeval *tm = (struct timeval*) option_value;
            uint32_t *timeout = (uint32_t*)(&(tm->tv_sec));
            uint32_t time_ms = (uint32_t)(((*timeout) * CONVERT_TO_MS) + ((tm->tv_usec)/CONVERT_TO_MS));
            result = cy_socket_setsockopt(sockets[sockID].socket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_SNDTIMEO, &time_ms, sizeof(time_ms));
            if ( result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("Failed to set SND timeout option : %d \n", result));
                return -1;
            }
            break;
        }

        default:
            IPERF_SOCKET_DEBUG(("Setsockopt %d not supported\n", option_name));
            return -1;
    }
    return 0;
}

int iperf_getsockopt(int sockID, int option_level, int option_name, void *option_value,uint32_t *option_length)
{
    /* Socket options requested by iperf:
     * sockets.c:
     * TCP_MAXSEG
     *
     * tcp_window_size.c:
     * SO_SNDBUF
     * SO_RCVBUF
     */
    IPERF_SOCKET_DEBUG(("getsockopt %d\n", option_name));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
            switch ( option_name )
            {
                case SO_SNDBUF:
#ifdef COMPONENT_NETXDUO
                    *(uint32_t*) option_value = CY_NX_TCP_SND_BUF;
#else
                    *(uint32_t*) option_value = TCP_SND_BUF;
#endif
                    *option_length = sizeof(uint32_t);
                    break;
                case SO_RCVBUF:
#ifdef COMPONENT_NETXDUO
                    *(uint32_t*) option_value = DEFAULT_TCP_WINDOW_SIZE;
#else
                    *(uint32_t*) option_value = TCP_WND;
#endif
                    *option_length = sizeof(uint32_t);
                    break;
                default:
                    IPERF_SOCKET_DEBUG(("Getsockopt %d not supported\n", option_name));
                    return -1;
            }
         break;
         case CY_SOCKET_IPPROTO_UDP:
            switch ( option_name )
            {
                case SO_SNDBUF:
                case SO_RCVBUF:
#ifdef COMPONENT_NETXDUO
                    *(uint32_t*) option_value = DEFAULT_TCP_WINDOW_SIZE;
#else
                    *(uint32_t*) option_value = TCP_WND;
#endif
                    *option_length = sizeof(uint32_t);
                    break;
                default:
                    IPERF_SOCKET_DEBUG(("Getsockopt %d not supported\n", option_name));
                    return -1;
            }
         break;
         default:
             IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
             return -1;
    }

    return 0;
}

int iperf_sendto(int sockID, char *msg, size_t msgLength, int flags, struct sockaddr *destAddr, uint32_t destAddrLen)
{
    IPERF_SOCKET_DEBUG(("iperf_sendto : Write data to socket with length : %d \n", msgLength));
    return 1;
}

int iperf_send(int sockID, const char *msg, size_t msgLength, int flags)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t bytes_sent = 0;

    IPERF_SOCKET_DEBUG(("iperf_send : Write data to socket with length : %d \n", msgLength));

    switch ( sockets[sockID].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
        {
            result = cy_socket_send(sockets[sockID].socket, msg, msgLength, 0, &bytes_sent);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_send failed with error : %d \n", result));
                return -1;
            }

            break;
        }
        case CY_SOCKET_IPPROTO_UDP:
        {
            cy_socket_sockaddr_t address;

            struct sockaddr* socket_addr = &sockets[sockID].remoteAddress;

            /* Fill Port number, IP address into cy_socket_sockaddr_t format */
            address.port = ((uint16_t)socket_addr->sa_data[0]) << 8;
            address.port |= socket_addr->sa_data[1];
            address.ip_address.version = CY_SOCKET_IP_VER_V4;
            address.ip_address.ip.v4 = (socket_addr->sa_data[2]) | (socket_addr->sa_data[3] << 8) | (socket_addr->sa_data[4] << 16) | (socket_addr->sa_data[5] << 24);

            result = cy_socket_sendto(sockets[sockID].socket, msg, msgLength, 0, &address, sizeof(cy_socket_sockaddr_t), &bytes_sent);
            if(result != CY_RSLT_SUCCESS)
            {
                IPERF_SOCKET_ERROR(("cy_socket_sendto failed with error : %d \n", result));
                return -1;
            }
            break;
        }
        default:
        {
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            bytes_sent = -1;
            break;
        }
    }

    IPERF_SOCKET_DEBUG(("iperf_send : Done, Status %d\n", bytes_sent));

    return bytes_sent;
}

#ifdef __cplusplus
extern "C" {
#endif

struct hostent* iperf_gethostbyname(const char *name)
{
    /* TODO: Implement code for resolving the hostname */
    IPERF_SOCKET_DEBUG(("gethostbyname \n"));
    return 0;
}

int  iperf_select( int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout )
{
    /*
     * iperf only uses select() in two places. Both are used to check if the socket is readable.
     * So instead of waiting here, we'll record the wait timeout and use it in the recv() call that follows
     */
    IPERF_SOCKET_DEBUG(("iperf_select \n"));
    return 1;
}

bool iperf_setsock_blocking (int fd, bool blocking) {

    IPERF_SOCKET_DEBUG(("iperf_setsock_blocking : Set the mode to blocking/non-blocking - %d \n", blocking));

    switch ( sockets[fd].protocol_type )
    {
        case CY_SOCKET_IPPROTO_TCP:
            break;

        case CY_SOCKET_IPPROTO_UDP:
            break;

        default:
            IPERF_SOCKET_DEBUG(("Invalid protocol type \n"));
            break;
    }

    return true;
}
