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

#ifndef IPERF_SOCKETS_H
#define IPERF_SOCKETS_H

#include "iperf_sockets_internal.h"
#include "iperf_utility.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

void sockets_layer_init(void);
void iperf_network_init( void * networkInterface );
int iperf_socket( int protocolFamily, int type, int protocol );
int iperf_select( int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout );
int iperf_close( int sockID);
int iperf_write( int sockID, const char *msg, size_t msgLength );
int iperf_read(int s, void *mem, size_t len);
bool iperf_setsock_blocking (int fd, bool blocking);
int iperf_send(int sockID, const char *msg, size_t msgLength, int flags);

int iperf_getpeername(int sockID, struct sockaddr *remoteAddress, uint32_t *addressLength);
int iperf_getsockname(int sockID, struct sockaddr *localAddress, uint32_t *addressLength);
int iperf_recvfrom(int sockID, char *buffer, size_t buffersize, int flags,struct sockaddr *fromAddr, uint32_t *fromAddrLen);
int iperf_recv(int sockID, void *rcvBuffer, size_t bufferLength, int flags);
int iperf_accept(int sockID, struct sockaddr *ClientAddress, uint32_t *addressLength);
int iperf_listen(int sockID, int backlog);
int iperf_bind(int sockID, struct sockaddr *localAddress, uint32_t addressLength);
int iperf_connect(int sockID, struct sockaddr *remoteAddress, uint32_t addressLength);
int iperf_setsockopt(int sockID, int option_level, int option_name, const void *option_value, uint32_t option_length);
int iperf_getsockopt(int sockID, int option_level, int option_name, void *option_value,uint32_t *option_length);
int iperf_sendto(int sockID, char *msg, size_t msgLength, int flags, struct sockaddr *destAddr, uint32_t destAddrLen);
struct hostent* iperf_gethostbyname(const char *name);
#ifdef __cplusplus
} /*extern "C" */
#endif

#endif //IPERF_SOCKETS_H
