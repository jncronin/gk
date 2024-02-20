/**
 * Copyright (c) 2023 John Cronin
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define LWIP_DHCP                       1
#define LWIP_NETIF_API                  1
#define NO_SYS                          0
#define LWIP_DNS                        1
#define LWIP_ERRNO_STDINCLUDE           1
#define LWIP_POSIX_SOCKETS_IO_NAMES     0
#define TCPIP_MBOX_SIZE                 256
#define TCPIP_THREAD_STACKSIZE          2048
#define TCPIP_THREAD_PRIO               1
#define DEFAULT_RAW_RECVMBOX_SIZE       32
#define DEFAULT_TCP_RECVMBOX_SIZE       32
#define DEFAULT_ACCEPTMBOX_SIZE       32
#define DEFAULT_UDP_RECVMBOX_SIZE       32

#endif
