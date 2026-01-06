/*
 * Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
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
 *  Implementation of Secure Sockets Interface for NetXDuo.
 *
 */

#include "cy_secure_sockets.h"
#include "cy_tls.h"
#include "cyabs_rtos.h"
#include "cy_worker_thread.h"
#include "cyabs_rtos_impl.h"
#include "cy_network_mw_core.h"
#include "cy_network_buffer.h"
#include "cy_log.h"
#include "nx_api.h"
#include "nx_packet.h"


#ifndef DEFAULT_TCP_WINDOW_SIZE
#define DEFAULT_TCP_WINDOW_SIZE             (7 * 1024)
#endif

#ifndef DEFAULT_UDP_QUEUE_SIZE
#define DEFAULT_UDP_QUEUE_SIZE              (5)
#endif

#ifndef DEFAULT_PACKET_ALLOCATE_TIMEOUT
#define DEFAULT_PACKET_ALLOCATE_TIMEOUT     (2 * 1000)
#endif

#ifndef DEFAULT_BIND_TIMEOUT
#define DEFAULT_BIND_TIMEOUT                (1000)
#endif

#ifndef DEFAULT_ACCEPT_TIMEOUT
#define DEFAULT_ACCEPT_TIMEOUT              (3000)
#endif

#ifndef DEFAULT_DNS_TIMEOUT
#define DEFAULT_DNS_TIMEOUT                 (3000)
#endif

#ifdef NX_DISABLE_PACKET_CHAIN
#define FRAGMENT_OPTION                     NX_DONT_FRAGMENT
#else
#define FRAGMENT_OPTION                     NX_FRAGMENT_OKAY
#endif

typedef struct cy_socket_ctx cy_socket_ctx_t;


/* Macro to get the cy_socket_context pointer from tcp/udp field's address */
#define CY_NETXDUO_GET_SOCKET_CONTEXT(addr,type,field) ((type*)((unsigned char*)addr - (unsigned long)&((type*)NULL)->nxd_socket.field))

struct cy_socket_ctx
{
    uint32_t              socket_magic_header;    /**< Socket context magic header to verify the context pointer */
    union
    {
        NX_TCP_SOCKET     *tcp;                   /**< NetXDuo TCP socket structure */
        NX_UDP_SOCKET     *udp;                   /**< NetXDuo UDP socket structure */
    } nxd_socket;
    cy_mutex_t            socket_mutex;           /**<* Protects accessing the socket context */
    struct
    {
        cy_socket_opt_callback_t connect_request;
        cy_socket_opt_callback_t receive;
        cy_socket_opt_callback_t disconnect;
    } callbacks;                                          /**<* Socket callback functions */
    void*                 tls_ctx;                        /**< tls context of underlying security stack */
    const void*           tls_identity;                   /**< contains certificate/key pair */
    char*                 rootca_certificate;             /**< RootCA certificate specific to the socket */
    int                   rootca_certificate_len;         /**< Length of ca_cert */
    bool                  enforce_tls;                    /**< Enforce TLS connection */
    int                   auth_mode;                      /**< TLS authentication mode */
    unsigned char         mfl_code;                       /**< TLS maximum fragment length */
    char*                 alpn;                           /**< ALPN string */
    char**                alpn_list;                      /**< ALPN array of strings to be passed to mbedtls */
    uint32_t              alpn_count;                     /**< Number of protocols in ALPN list */
    char*                 hostname;                       /**< Server hostname used with SNI extension */
    uint32_t              status;                         /**< socket status */
    NX_PACKET*            packet;                         /**< Receive data \c NX_PACKET structure */
    uint16_t              offset;                         /**< Receive data \c NX_PACKET  offset */
    uint16_t              bind_port;                      /**< Port number for listen requests */
    int                   id;                             /**< Socket id used in mapping from netconn to cy socket */
    int                   role;                           /**< Used for identifying if the socket is server or client */
    int                   transport_protocol;             /**< Used for identifying transport protocol TCP, TLS or UDP */
    bool                  is_recvtimeout_set;             /**< Used to check whether receive timeout is set by the application */
    uint32_t              recv_timeout;                   /**< Socket receive timeout in milliseconds */
    uint32_t              send_timeout;                   /**< Socket send timeout in milliseconds */
    bool                  nonblocking;                    /**< Socket is nonblocking */
    bool                  is_authmode_set;                /**< Used to check whether TLS authentication mode is set by the application */
    cy_socket_interface_t iface_type;                     /**< Network interface to be used with the socket */
#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT
    bool                  load_rootca_from_ram;           /**< Flag for setting the RootCA certificate location.  */
    bool                  load_device_cert_key_from_ram;  /**< Flag for setting the device cert & key location. */
#endif
    uint32_t              socket_magic_footer;            /**< Socket context magic footer to verify the context pointer */
};

/*
 * This structure is queued to worker thread to process all the receive events occurred before socket context is created for accepted client socket.
 */
typedef struct cy_socket_recv_events
{
    uint32_t recv_count;
    cy_socket_ctx_t *socket;
} cy_socket_recv_event_t;

typedef struct cy_socket_multicast_pair
{
    cy_socket_ctx_t       *socket;         /**< Socket */
    cy_socket_ip_address_t if_addr;        /**< Interface IP address */
    cy_socket_ip_address_t multi_addr;     /**< multicast group address */
} cy_socket_multicast_pair_t;

/* Used to keep track of the registered multicast members */
typedef struct cy_socket_multicast_info
{
    cy_socket_multicast_pair_t *multicast_member_list;      /**< List of multicast addresses registered */
    uint32_t                    multicast_member_status;    /**< Each bit in this member indicates status of corresponding entry in multicast_member_list. */
    uint8_t                     multicast_member_count;     /**< Number of multicast addresses registered */
} cy_socket_multicast_info_t;

static cy_socket_multicast_info_t multicast_info;

/* Mutex to protect the multicast info */
static cy_mutex_t multicast_join_leave_mutex;

typedef enum
{
    SOCKET_STATUS_FLAG_CONNECTED = 0x1, /* TCP socket connection is established with peer */
    SOCKET_STATUS_FLAG_SECURED   = 0x2, /* Secure TLS connection is establisher with peer */
    SOCKET_STATUS_FLAG_LISTENING = 0x4  /* Server socket is ready for client connections */
} cy_socket_status_flag_t;

#define SECURE_SOCKETS_MAX_FRAG_LEN_NONE    0
#define SECURE_SOCKETS_MAX_FRAG_LEN_512     1
#define SECURE_SOCKETS_MAX_FRAG_LEN_1024    2
#define SECURE_SOCKETS_MAX_FRAG_LEN_2048    3
#define SECURE_SOCKETS_MAX_FRAG_LEN_4096    4
#define SECURE_SOCKETS_MAX_FRAG_LEN_INVALID 5

/* Secure sockets thread stack size */
#ifndef SECURE_SOCKETS_THREAD_STACKSIZE
#define SECURE_SOCKETS_THREAD_STACKSIZE    (6 * 1024)
#endif

/* Sleep time in each loop while waiting for ARP resolution */
#define ARP_CACHE_CHECK_INTERVAL_IN_MSEC    5

#define NUM_SOCKETS                         (16)

#define SECURE_SOCKETS_MAGIC_HEADER         0xbaefdcbd
#define SECURE_SOCKETS_MAGIC_FOOTER         0xefbcabfe

/* Maximum number of multicast groups supported. */
#define SECURE_SOCKETS_MAX_MULTICAST_GROUPS 10

#define UNUSED_ARG(arg)                     (void)(arg)

#define NXD_TO_CY_SECURE_SOCKETS_ERR( nxd_err )     ( nxd_to_secure_socket_error(nxd_err) )
#define TLS_TO_CY_SECURE_SOCKETS_ERR( tls_err )     ( tls_to_secure_socket_error(tls_err) )

#ifdef ENABLE_SECURE_SOCKETS_LOGS
#define ss_cy_log_msg cy_log_msg
#else
#define ss_cy_log_msg(a,b,c,...)
#endif

#define IN_RANGE(x, min, max)               ((uint8_t)(x) >= (min) && (uint8_t)(x) <= (max))
#define IS_LOWERCASE(x)                     IN_RANGE((x), 'a', 'z')
#define IS_DIGIT(x)                         IN_RANGE((x), '0', '9')
#define IS_HEX_DIGIT(x)                     (IS_DIGIT(x) || IN_RANGE((x), 'a', 'f') || IN_RANGE((x), 'A', 'F'))
#define IPV6_SEGMENT_CNT                    8

typedef struct cy_nxd_sock
{
    int used;
    cy_socket_ctx_t *ctx;
} cy_nxd_sock_t;

/** mutex to protect socket array */
static cy_mutex_t socket_list_mutex;

/* mutex to protect the counter maintained for receive events occurred
 * on an accepted socket, prior to socket context is created for it.
 */
static cy_mutex_t accept_recv_event_mutex;
static cy_worker_thread_info_t socket_worker;

/* Socket library usage count */
static int init_ref_count = 0;

/** The global array of available sockets */
static cy_nxd_sock_t socket_list[NUM_SOCKETS];

static bool is_socket_valid(cy_socket_ctx_t *socket)
{
    if ((socket->socket_magic_header == SECURE_SOCKETS_MAGIC_HEADER) && (socket->socket_magic_footer == SECURE_SOCKETS_MAGIC_FOOTER))
    {
        return true;
    }
    return false;
}

static cy_rslt_t convert_nxd_to_secure_socket_ip_addr(cy_socket_ip_address_t *dest, const NXD_ADDRESS *src)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    memset(dest, 0, sizeof(cy_socket_ip_address_t));

#if defined(NX_DISABLE_IPV6) && defined(NX_DISABLE_IPV4)

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NX_DISABLE_IPV6 and NX_DISABLE_IPV4 are both defined\n");
    result = CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION;

#else

    /*
     * NetXDuo APIs use host byte order for addresses. cy_socket_ip_address_t structures
     * use network byte order so we need to do the proper conversion.
     */

    if (src->nxd_ip_version == NX_IP_VERSION_V4)
    {
#ifndef NX_DISABLE_IPV4
        dest->version = CY_SOCKET_IP_VER_V4;
        dest->ip.v4   = htonl(src->nxd_ip_address.v4);
#endif
    }
    else if (src->nxd_ip_version == NX_IP_VERSION_V6)
    {
#ifndef NX_DISABLE_IPV6
        dest->version  = CY_SOCKET_IP_VER_V6;
        dest->ip.v6[0] = htonl(src->nxd_ip_address.v6[0]);
        dest->ip.v6[1] = htonl(src->nxd_ip_address.v6[1]);
        dest->ip.v6[2] = htonl(src->nxd_ip_address.v6[2]);
        dest->ip.v6[3] = htonl(src->nxd_ip_address.v6[3]);
#endif
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid IP version type\n");
        result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

#endif

    return result;
}

static cy_rslt_t convert_secure_socket_to_nxd_ip_addr(NXD_ADDRESS *dest, const cy_socket_ip_address_t *src)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    memset(dest, 0, sizeof(NXD_ADDRESS));

#if defined(NX_DISABLE_IPV6) && defined(NX_DISABLE_IPV4)

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NX_DISABLE_IPV6 and NX_DISABLE_IPV4 are both defined\n");
    result = CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION;

#else

    /*
     * NetXDuo APIs use host byte order for addresses. cy_socket_ip_address_t structures
     * use network byte order so we need to do the proper conversion.
     */

    if (src->version == CY_SOCKET_IP_VER_V4)
    {
#ifndef NX_DISABLE_IPV4
        dest->nxd_ip_version    = NX_IP_VERSION_V4;
        dest->nxd_ip_address.v4 = ntohl(src->ip.v4);
#endif
    }
    else if (src->version == CY_SOCKET_IP_VER_V6)
    {
#ifndef NX_DISABLE_IPV6
        dest->nxd_ip_version       = NX_IP_VERSION_V6;
        dest->nxd_ip_address.v6[0] = ntohl(src->ip.v6[0]);
        dest->nxd_ip_address.v6[1] = ntohl(src->ip.v6[1]);
        dest->nxd_ip_address.v6[2] = ntohl(src->ip.v6[2]);
        dest->nxd_ip_address.v6[3] = ntohl(src->ip.v6[3]);
#endif
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid IP version type\n");
        result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

#endif

    return result;
}

static bool ip_addr_ismulticast(NXD_ADDRESS *addr)
{
    if (addr == NULL)
    {
        return false;
    }

    if (addr->nxd_ip_version == NX_IP_VERSION_V4)
    {
        /*
         * IPv4 multicast addresses are Class D.
         */

        if ((addr->nxd_ip_address.v4 & 0xF0000000UL) == 0xE0000000UL)
        {
            return true;
        }
    }
    else if (addr->nxd_ip_version == NX_IP_VERSION_V6)
    {
        /*
         * IPv6 multicast addresses have the top 8 bits set.
         */

        if ((addr->nxd_ip_address.v6[0] & 0xFF000000UL) == 0xFF000000UL)
        {
            return true;
        }
    }

    return false;
}

static bool ip_addrs_same(NXD_ADDRESS *addr1, NXD_ADDRESS *addr2)
{
    if (addr1 == NULL || addr2 == NULL)
    {
        return false;
    }

    if (addr1->nxd_ip_version != addr2->nxd_ip_version)
    {
        return false;
    }

    if (addr1->nxd_ip_version == NX_IP_VERSION_V4 && addr1->nxd_ip_address.v4 != addr2->nxd_ip_address.v4)
    {
        return false;
    }

    if (addr1->nxd_ip_version == NX_IP_VERSION_V6 &&
        (addr1->nxd_ip_address.v6[0] != addr2->nxd_ip_address.v6[0] || addr1->nxd_ip_address.v6[1] != addr2->nxd_ip_address.v6[1] ||
         addr1->nxd_ip_address.v6[2] != addr2->nxd_ip_address.v6[2] || addr1->nxd_ip_address.v6[3] != addr2->nxd_ip_address.v6[3]))
    {
        return false;
    }

    return true;
}

static bool str_to_ip(const char *str, uint32_t *addr)
{
    uint8_t byte = 0;
    uint8_t byte_cnt = 1;
    uint32_t ip_addr = 0;
    char ch;

    if (str == NULL || addr == NULL)
    {
        return false;
    }

    if (strlen(str) > 15)
    {
        return false;
    }

    while (*str != '\0' || byte_cnt <= 4)
    {
        ch = *str;
        if (ch == '.' || ch == '\0')
        {
            if (byte_cnt == 1 && ch == '.')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 0);
            }
            else if (byte_cnt == 2 && ch == '.')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 8);
            }
            else if (byte_cnt == 3 && ch == '.')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 16);
            }
            else if(byte_cnt == 4 && ch == '\0')
            {
                ip_addr = ip_addr | (((uint32_t)byte) << 24);
                break;
            }
            else
            {
                break;
            }
            byte = 0;
            byte_cnt++;
            str++;
            continue;
        }
        byte = (byte * 10) + (ch - '0');
        str++;
    }
    if (byte_cnt < 4)
    {
        return false;
    }
    *addr = ip_addr;

    return true;
}

static bool str_to_ipv6(const char *str, uint32_t *ipv6_addr)
{
    uint32_t addr_idx, zero_segment_cnt, curr_segment_idx, curr_segment_val;
    const char *ch;

    if(str == NULL || ipv6_addr == NULL)
    {
        return false;
    }

    /* To get the number of segments in a "::" sequence, count the number of colons.
       'zero_segment_cnt' can be 1 even if there are no :: sequences in the given IPv6 address string */
    zero_segment_cnt = IPV6_SEGMENT_CNT;

    for (ch = str; *ch != 0; ch++)
    {
        if(*ch == ':')
        {
            zero_segment_cnt--;
        }
        else if(!IS_HEX_DIGIT(*ch))
        {
            return false;
        }
    }

    /* parsing each segment */
    addr_idx = 0;
    curr_segment_idx = 0;
    curr_segment_val = 0;
    for (ch = str; *ch != 0; ch++)
    {
        if (*ch == ':')
      	{
            if (curr_segment_idx & 0x1)
            {
                ipv6_addr[addr_idx++] |= curr_segment_val;
            }        
            else
            {
                ipv6_addr[addr_idx] = curr_segment_val << 16; 
            }

            curr_segment_idx++;
            curr_segment_val = 0;

            if (curr_segment_idx > (IPV6_SEGMENT_CNT - 1))
            {
                /* address too long - More than 8 segments in IPv6 Address */
                return false;
            }
            if (ch[1] == ':')
            {
                if (ch[2] == ':')
                {
                    /* 3 successive colons in IPv6 address is invalid format */
                    return false;
                }
                ch++;
                /* Once "::" is found, set zeros */
                while (zero_segment_cnt > 0)
                {
                    zero_segment_cnt--;
                    if (curr_segment_idx & 0x1)
                    {
                        addr_idx++;
                    }  
                    else
                    {
                        ipv6_addr[addr_idx] = 0;        
                    }
                    
                    curr_segment_idx++;
                    if (curr_segment_idx > (IPV6_SEGMENT_CNT-1))
                    {
                        /* address too long - More than 8 segments in IPv6 Address */
                        return false;
                    }
                }
            }
        }
        else if (IS_HEX_DIGIT(*ch))
        {
            /* Add current digit */
            curr_segment_val = (curr_segment_val << 4) + (IS_DIGIT(*ch) ? (uint32_t)(*ch - '0') : (uint32_t)(10 + (IS_LOWERCASE(*ch) ? *ch - 'a' : *ch - 'A')));
        }
        else
        {
            /* Unexpected character */
            return false;
        }
    }

    if (curr_segment_idx & 0x1)
    {
        ipv6_addr[addr_idx++] |= curr_segment_val;
    }        

    if (curr_segment_idx != (IPV6_SEGMENT_CNT-1))
    {
        return false;
    }    

    return true;
}


static cy_rslt_t nxd_packet_get_data(NX_PACKET *packet, uint16_t offset, uint8_t **data, uint16_t *fragment_available_data_length, uint16_t *total_available_data_length)
{
    NX_PACKET* first_packet      = packet;
    NX_PACKET* iter              = packet;
    uint16_t   requested_offset  = offset;
    uint16_t   fragment_length;

    if (packet == NULL || data == NULL || fragment_available_data_length == NULL || total_available_data_length == NULL)
    {
        return CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR;
    }

    while (iter != NULL)
    {
        /* It is more appropriate to use the difference between nx_packet_append_ptr and nx_packet_prepend_ptr rather
         * than nx_packet_length. If the packet was fragmented the nx_packet_length will reflect the sum of the data length of all
         * fragments, not the length of the fragment alone */

        fragment_length = (uint16_t)(iter->nx_packet_append_ptr - iter->nx_packet_prepend_ptr);
        if (iter->nx_packet_length == 0)
        {
            *data                           = NULL;
            *fragment_available_data_length = 0;
            *total_available_data_length    = 0;

            return CY_RSLT_SUCCESS;
        }
        else if (offset < fragment_length)
        {
            *data = iter->nx_packet_prepend_ptr + offset;
            *fragment_available_data_length = (uint16_t)(iter->nx_packet_append_ptr - *data);

            /* This will give number of bytes available after this offset including this packet and further packets in the chain */
            *total_available_data_length    = (uint16_t)(first_packet->nx_packet_length - requested_offset);

            return CY_RSLT_SUCCESS;
        }
        else
        {
            offset = (uint16_t)(offset - (iter->nx_packet_append_ptr - iter->nx_packet_prepend_ptr));
        }
        iter = iter->nx_packet_next;
    }

    return CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR;
}

/* Find the index of the next registered slot in the multicast list. Caller should ensure to call
 * this helper function only when at-least one member is registered, else the while will become indefinite.
 * This function should be called under a mutex lock. */
static uint32_t next_registered_multicast_slot(uint32_t index)
{
    while (!(multicast_info.multicast_member_status & (0x0001 << index)))
    {
        index++;
    }
    return index;
}

/* Find the index of the next free slot in the multicast list. Ensure to call this function only when the
 * total registered count is not reached the maximum. This function should be called under a mutex lock. */
static uint32_t next_free_multicast_slot(uint32_t index)
{
    while (multicast_info.multicast_member_status & (0x0001 << index))
    {
        index++;
    }
    return index;
}

/* Set the status of the given input slot in the multicast list. This function should be called under a mutex lock. */
static void set_multicast_slot_status_bit(uint32_t index)
{
    multicast_info.multicast_member_status |= (0x0001 << index);
}

/* Clear the status of the given input slot in the multicast list. This function should be called under a mutex lock. */
static void clear_multicast_slot_status_bit(uint32_t index)
{
    multicast_info.multicast_member_status &= ~(0x0001 << index);
}

/* Find the index of the given input member in the multicast list. */
static int find_multicast_member_index(cy_socket_ctx_t *socket, const cy_socket_ip_mreq_t *imr)
{
    uint32_t count = 0;
    uint32_t index = 0;

    while (count < multicast_info.multicast_member_count)
    {
        index = next_registered_multicast_slot(index);

        if( memcmp(&multicast_info.multicast_member_list[index].multi_addr, &imr->multi_addr, sizeof(imr->multi_addr)) == 0 &&
            memcmp(&multicast_info.multicast_member_list[index].if_addr, &imr->if_addr, sizeof(imr->if_addr)) == 0 &&
            multicast_info.multicast_member_list[index].socket == socket )
        {
            return index;
        }
        count++;
        index++;
    }

    return -1;
}

static cy_socket_ctx_t *alloc_socket(int protocol, cy_socket_interface_t iface_type)
{
    int i;
    cy_rslt_t result;

    /* allocate a new socket identifier */
    for (i = 0; i < NUM_SOCKETS; ++i)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
        if (!socket_list[i].ctx)
        {
            if (socket_list[i].used)
            {
                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
                continue;
            }
            socket_list[i].ctx = malloc(sizeof(cy_socket_ctx_t));
            if (socket_list[i].ctx == NULL)
            {
                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed %s %d\n", __FILE__, __LINE__);
                return NULL;
            }

            memset(socket_list[i].ctx, 0, sizeof(cy_socket_ctx_t));

            /*
             * Allocate the NetXDuo socket structure.
             */

            if (protocol == CY_SOCKET_IPPROTO_UDP)
            {
                socket_list[i].ctx->nxd_socket.udp = calloc(1, sizeof(NX_UDP_SOCKET));
            }
            else
            {
                socket_list[i].ctx->nxd_socket.tcp = calloc(1, sizeof(NX_TCP_SOCKET));
            }

            /*
             * Since the socket pointers are a union, we only need to check one member to see if the alloc was successful.
             */

            if (socket_list[i].ctx->nxd_socket.udp == NULL)
            {
                free(socket_list[i].ctx);
                socket_list[i].ctx = NULL;

                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);

                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "socket alloc failed at file: %s line: %d\n", __FILE__, __LINE__);
                return NULL;
            }

            result = cy_rtos_init_mutex(&socket_list[i].ctx->socket_mutex);
            if (CY_RSLT_SUCCESS != result)
            {
                free(socket_list[i].ctx->nxd_socket.udp);
                socket_list[i].ctx->nxd_socket.udp = NULL;
                free(socket_list[i].ctx);
                socket_list[i].ctx = NULL;

                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_rtos_init_mutex failed at file: %s line: %d error code: %ld\n", __FILE__, __LINE__, result);
                return NULL;
            }

            socket_list[i].used = 1;
            socket_list[i].ctx->id = i;
            socket_list[i].ctx->role = CY_TLS_ENDPOINT_CLIENT;
            socket_list[i].ctx->iface_type = iface_type;
            socket_list[i].ctx->transport_protocol  = protocol;
            socket_list[i].ctx->socket_magic_header = SECURE_SOCKETS_MAGIC_HEADER;
            socket_list[i].ctx->socket_magic_footer = SECURE_SOCKETS_MAGIC_FOOTER;

            cy_rtos_set_mutex(&socket_list_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
            return socket_list[i].ctx;
        }
        else
        {
            cy_rtos_set_mutex(&socket_list_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        }
    }

    return NULL;
}

static void free_socket(cy_socket_ctx_t *socket)
{
    int id;

    if (socket == NULL)
    {
        return;
    }

    id = socket->id;
    if (id >= 0 && id < NUM_SOCKETS)
    {
        /* update the sockets array */
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
        cy_rtos_deinit_mutex(&socket->socket_mutex);
        socket_list[id].used = 0;

        /*
         * Socket pointers are a union so we only need to check one.
         */

        if (socket_list[id].ctx->nxd_socket.tcp)
        {
            free(socket_list[id].ctx->nxd_socket.tcp);
            socket_list[id].ctx->nxd_socket.tcp = NULL;
        }

        memset(socket_list[id].ctx, 0, sizeof(cy_socket_ctx_t));
        socket_list[id].ctx = NULL;
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
    }

    /* free the socket memory */
    free(socket);
}

static unsigned char max_fragment_length_to_mfl_code(uint32_t max_fragment_length)
{
    unsigned char mfl;
    switch (max_fragment_length)
    {
        case 0:
        {
            mfl = SECURE_SOCKETS_MAX_FRAG_LEN_NONE;
            break;
        }
        case 512:
        {
            mfl = SECURE_SOCKETS_MAX_FRAG_LEN_512;
            break;
        }
        case 1024:
        {
            mfl = SECURE_SOCKETS_MAX_FRAG_LEN_1024;
            break;
        }
        case 2048:
        {
            mfl = SECURE_SOCKETS_MAX_FRAG_LEN_2048;
            break;
        }
        case 4096:
        {
            mfl = SECURE_SOCKETS_MAX_FRAG_LEN_4096;
            break;
        }
        default:
        {
            mfl = SECURE_SOCKETS_MAX_FRAG_LEN_INVALID;
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid maximum fragment length\n");
        }
    }
    return mfl;
}

static uint32_t mfl_code_to_max_fragment_length(unsigned char mfl_code)
{
    uint32_t max_fragment_length=0;;

    switch (mfl_code)
    {
        case SECURE_SOCKETS_MAX_FRAG_LEN_NONE:
        {
            max_fragment_length = 0;
            break;
        }
        case SECURE_SOCKETS_MAX_FRAG_LEN_512:
        {
            max_fragment_length = 512;
            break;
        }
        case SECURE_SOCKETS_MAX_FRAG_LEN_1024:
        {
            max_fragment_length = 1024;
            break;
        }
        case SECURE_SOCKETS_MAX_FRAG_LEN_2048:
        {
            max_fragment_length = 2048;
            break;
        }
        case SECURE_SOCKETS_MAX_FRAG_LEN_4096:
        {
            max_fragment_length = 4096;
            break;
        }
        default:
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid mflcode %d\n", mfl_code);
        }
    }
    return max_fragment_length;
}

static cy_rslt_t nxd_to_secure_socket_error(UINT error)
{
    switch (error)
    {
        case NX_SUCCESS:
            return CY_RSLT_SUCCESS;

        case NX_PTR_ERROR:
            return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;

//        case ERR_CLSD:
//            return CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED;

        case NX_NO_PACKET:
            return CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;

//        case ERR_TIMEOUT:
//            return CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;

        case NX_IN_PROGRESS:
            return CY_RSLT_MODULE_SECURE_SOCKETS_IN_PROGRESS;

//        case ERR_ISCONN:
//            return CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED;

        case NX_NOT_CONNECTED:
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;

//        case ERR_ARG:
//        case ERR_VAL:
//            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;

//        case ERR_USE:
//            return CY_RSLT_MODULE_SECURE_SOCKETS_ADDRESS_IN_USE;

//        case ERR_RTE:
//            return CY_RSLT_MODULE_SECURE_SOCKETS_ERROR_ROUTING;

//        case ERR_BUF:
//        case ERR_WOULDBLOCK:
//        case ERR_IF:
        default:
            printf("NetXDuo error: 0x%02x\n", error);
            return CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR;
    }
}

static cy_rslt_t tls_to_secure_socket_error(cy_rslt_t error)
{
    switch (error)
    {
        case CY_RSLT_SUCCESS:
            return CY_RSLT_SUCCESS;

        case CY_RSLT_MODULE_TLS_BADARG:
            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;

        case CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE:
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;

        case CY_RSLT_MODULE_TLS_CONNECTION_CLOSED:
            return CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED;

        case CY_RSLT_MODULE_TLS_SOCKET_NOT_CONNECTED:
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;

        case CY_RSLT_MODULE_TLS_TIMEOUT:
            return CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;

        case CY_RSLT_MODULE_TLS_PKCS_ERROR:
            return CY_RSLT_MODULE_SECURE_SOCKETS_PKCS_ERROR;

        case CY_RSLT_MODULE_TLS_PARSE_CERTIFICATE:
        case CY_RSLT_MODULE_TLS_PARSE_KEY:
        case CY_RSLT_MODULE_TLS_ERROR:
        default:
            return CY_RSLT_MODULE_SECURE_SOCKETS_TLS_ERROR;
    }
}

static cy_rslt_t secure_socket_to_tls_error(cy_rslt_t error)
{
    switch (error)
    {
        case CY_RSLT_SUCCESS:
            return CY_RSLT_SUCCESS;

        case CY_RSLT_MODULE_SECURE_SOCKETS_BADARG:
            return CY_RSLT_MODULE_TLS_BADARG;

        case CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM:
            return CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE;

        case CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED:
            return CY_RSLT_MODULE_TLS_CONNECTION_CLOSED;

        case CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT:
            return CY_RSLT_MODULE_TLS_TIMEOUT;

        case CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED:
            return CY_RSLT_MODULE_TLS_SOCKET_NOT_CONNECTED;

        default:
            return CY_RSLT_MODULE_TLS_ERROR;
    }
}

/* 'nx_tcp_socket_bytes_available' does not indicate bytes available when socket is not in connected state.
 * It is possible that the peer device sends data and immediately closes the connection. In this case, bytes
 * are available to be processed even though the socket is no longer in a connected state.
 * Hence using a modified implementation to check if bytes are available to be received.
 */

static bool is_nx_tcp_bytes_available(NX_TCP_SOCKET *socket_ptr)
{
    NX_IP         *ip_ptr;
    NX_PACKET     *packet_ptr;
    bool is_bytes_available = false;

    if(socket_ptr == NULL || socket_ptr->nx_tcp_socket_state <= NX_TCP_LISTEN_STATE)
    {
        return false;
    }

    /* Setup IP pointer. */
    ip_ptr = socket_ptr->nx_tcp_socket_ip_ptr;
    if(ip_ptr == NULL)
    {
        return false;
    }

    /* Obtain the IP mutex so we can examine the bound port.  */
    tx_mutex_get(&(ip_ptr->nx_ip_protection), TX_WAIT_FOREVER);

    packet_ptr = socket_ptr->nx_tcp_socket_receive_queue_head;

    if (packet_ptr != NX_NULL)
    {
        /* The receive queue is not empty.
         * There are bytes in the rx queue that are available to the applciation.
         */
        if (packet_ptr->nx_packet_queue_next == ((NX_PACKET *)NX_PACKET_READY))
        {
            is_bytes_available = true;
        }
    }
    tx_mutex_put(&(ip_ptr -> nx_ip_protection));

    return is_bytes_available;
}

/* Check if a given TCP socket's TX queue is available for sending data */
static bool is_nx_tcp_send_available(NX_TCP_SOCKET *socket_ptr)
{
    NX_IP         *ip_ptr;
    bool          is_tx_queue_free = false;

    if(socket_ptr == NULL)
    {
        return false;
    }

    /* Setup IP pointer. */
    ip_ptr = socket_ptr->nx_tcp_socket_ip_ptr;
    if(ip_ptr == NULL)
    {
        return false;
    }

    /* Obtain the IP mutex so we can examine the bound port.  */
    tx_mutex_get(&(ip_ptr->nx_ip_protection), TX_WAIT_FOREVER);

    /* Make sure the TCP socket is in a connected state. */
    if ((socket_ptr -> nx_tcp_socket_state <= NX_TCP_LISTEN_STATE) ||
        (socket_ptr -> nx_tcp_socket_state > NX_TCP_ESTABLISHED))
    {
        /* Release protection.  */
        tx_mutex_put(&(ip_ptr -> nx_ip_protection));

        return false;
    }

    /* Check if there is room in tcp transmit queue to send more data. */
    if(socket_ptr->nx_tcp_socket_transmit_sent_count < socket_ptr->nx_tcp_socket_transmit_queue_maximum)
    {
        is_tx_queue_free = true;
    }
    else
    {
        is_tx_queue_free = false;
    }

    /* Release protection.  */
    tx_mutex_put(&(ip_ptr -> nx_ip_protection));

    return is_tx_queue_free;
}

/* Check if a given UDP socket is available for sending data. */ 
static bool is_nx_udp_send_available(NX_UDP_SOCKET *socket_ptr)
{
    /* There is no queuing of data in UDP sockets. Data can be sent if the socket is bound to a port. */
    if (!socket_ptr->nx_udp_socket_bound_next)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static void cy_process_receive_event_with_valid_context(cy_socket_ctx_t *ctx)
{
    NX_PACKET *packet;
    ULONG bytes_available;
    bool is_bytes_available;

    /*
     * Note this routine assumes the socket_list_mutex has been grabbed.
     */

    bytes_available = 0;
    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        if (ctx->callbacks.receive.callback)
        {
            /* For UDP transport protocol, check to find if receive data is available. */
            nx_udp_socket_bytes_available(ctx->nxd_socket.udp, &bytes_available);

            if (bytes_available > 0)
            {
                ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
            }
        }
    }
    else if( ( !ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED ) ||
             ( ( ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) )
    {
        /* If the transport protocol is TCP or TLS invoke application's receive callback function only if the connection is established.
         * The above check is needed for TLS connection so that we do not invoke app's receive callback for TLS handshake messages.
         */
        if (ctx->callbacks.receive.callback)
        {
            is_bytes_available = is_nx_tcp_bytes_available(ctx->nxd_socket.tcp);
            if (is_bytes_available)
            {
                ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
            }
            else
            {
                /* NetXDuo receive events are packet based, but secure socket APIs are stream based.
                 * If bytes_available is zero, still there can be received data left in the packet that is stored in
                 * the socket context. In that case even though bytes_available is zero application callback need be be invoked.
                 */

                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
                cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

                packet = ctx->packet;

                cy_rtos_set_mutex(&ctx->socket_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                if (packet)
                {
                    ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
                }
            }
        }
    }
}

static void cy_process_disconnect_event(void *arg)
{
    NX_TCP_SOCKET *tcp = (NX_TCP_SOCKET *)arg;
    cy_socket_ctx_t *ctx;
    uint32_t id;

    if (tcp == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NULL TCP socket %s %$d\n", __FILE__, __LINE__);
        return;
    }
    id = (uint32_t)tcp->nx_tcp_socket_reserved_ptr;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);

    ctx = (cy_socket_ctx_t *)socket_list[id].ctx;

    if (ctx == NULL || !is_socket_valid(ctx))
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NULL or invalid context in cy_process_disconnect_event\n");
        return;
    }

    if (ctx->callbacks.disconnect.callback)
    {
        ctx->callbacks.disconnect.callback((cy_socket_t)ctx, ctx->callbacks.disconnect.arg);
    }

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
}

static void cy_process_tcp_receive_event(void *arg)
{
    NX_TCP_SOCKET *tcp = (NX_TCP_SOCKET *)arg;
    cy_socket_ctx_t *ctx;
    uint32_t id;

    if (tcp == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NULL TCP socket %s %d\n", __FILE__, __LINE__);
        return;
    }
    id = (uint32_t)tcp->nx_tcp_socket_reserved_ptr;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);

    ctx = (cy_socket_ctx_t *)socket_list[id].ctx;

    if (ctx == NULL || !is_socket_valid(ctx))
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NULL or invalid context in cy_process_tcp_receive_event\n");
        return;
    }

    cy_process_receive_event_with_valid_context(ctx);

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
}

static void cy_process_connect_event(void *arg)
{
    NX_TCP_SOCKET *tcp = (NX_TCP_SOCKET *)arg;
    cy_socket_ctx_t *ctx;
    uint32_t id;

    if (tcp == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NULL TCP socket %s %d\n", __FILE__, __LINE__);
        return;
    }
    id = (uint32_t)tcp->nx_tcp_socket_reserved_ptr;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);

    ctx = (cy_socket_ctx_t *)socket_list[id].ctx;

    if (ctx == NULL || !is_socket_valid(ctx))
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NULL or invalid context in cy_process_connect_event\n");
        return;
    }

    if (ctx->callbacks.connect_request.callback)
    {
        ctx->callbacks.connect_request.callback((cy_socket_t)ctx, ctx->callbacks.connect_request.arg);
    }

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
}

static void cy_process_udp_receive_event(void *arg)
{
    NX_UDP_SOCKET *udp = (NX_UDP_SOCKET *)arg;
    cy_socket_ctx_t *ctx;
    uint32_t id;

    if (udp == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NULL UDP socket %s %d\n", __FILE__, __LINE__);
        return;
    }
    id = (uint32_t)udp->nx_udp_socket_reserved_ptr;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);

    ctx = (cy_socket_ctx_t *)socket_list[id].ctx;

    if (ctx == NULL || !is_socket_valid(ctx))
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NULL or invalid context in cy_process_udp_receive_event\n");
        return;
    }

    cy_process_receive_event_with_valid_context(ctx);

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
}

static void cy_tcp_disconnect_callback(NX_TCP_SOCKET *socket)
{
    cy_rslt_t result;

    result = cy_worker_thread_enqueue(&socket_worker, cy_process_disconnect_event, (void *)socket);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_worker_thread_enqueue failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
    }
}

static void cy_tcp_receive_callback(NX_TCP_SOCKET *socket)
{
    cy_rslt_t result;

    result = cy_worker_thread_enqueue(&socket_worker, cy_process_tcp_receive_event, (void *)socket);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_worker_thread_enqueue failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
    }
}

static void cy_tcp_connect_callback(NX_TCP_SOCKET *socket, UINT port)
{
    cy_rslt_t result;

    result = cy_worker_thread_enqueue(&socket_worker, cy_process_connect_event, (void *)socket);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_worker_thread_enqueue failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
    }
}

static void cy_udp_receive_callback(NX_UDP_SOCKET *socket)
{
    cy_rslt_t result;

    result = cy_worker_thread_enqueue(&socket_worker, cy_process_udp_receive_event, (void *)socket);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_worker_thread_enqueue failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
    }
}

/*-----------------------------------------------------------*/
/*
 * @brief Network send helper function. Function call to this function should be protected by socket_mutex.
 */
static cy_rslt_t network_send(void *context, const unsigned char *data_buffer, uint32_t data_buffer_length, uint32_t *bytes_sent)
{
    UINT ret = NX_SUCCESS;
    cy_socket_ctx_t *ctx = (cy_socket_ctx_t *)context;
    NX_PACKET *packet;
    cy_rslt_t result;
    uint32_t size;

    *bytes_sent = 0;

    /*
     * This routine should never be called for UDP sockets.
     */

    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        return CY_RSLT_MODULE_SECURE_SOCKETS_PROTOCOL_NOT_SUPPORTED;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    while (data_buffer_length - *bytes_sent > 0)
    {
        result = cy_network_get_packet(CY_NETWORK_TCP_PACKET, DEFAULT_PACKET_ALLOCATE_TIMEOUT, (void *)&packet);
        if (result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_netxduo_get_packet failed with error %d\n", result);
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        /*
         * Make sure we aren't trying to send more data than we have room for in the packet.
         */

        size = data_buffer_length - *bytes_sent;
        if (size > (uint32_t)(packet->nx_packet_data_end - packet->nx_packet_prepend_ptr))
        {
            size = (uint32_t)(packet->nx_packet_data_end - packet->nx_packet_prepend_ptr);
        }

        memcpy(packet->nx_packet_prepend_ptr, &data_buffer[*bytes_sent], size);
        packet->nx_packet_append_ptr = packet->nx_packet_prepend_ptr + size;
        packet->nx_packet_length     = (ULONG)(packet->nx_packet_append_ptr - packet->nx_packet_prepend_ptr);

        ret = nx_tcp_socket_send(ctx->nxd_socket.tcp, packet, ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->send_timeout));
        if (ret != NX_SUCCESS)
        {
            /*
             * Don't leak the packet.
             */
            nx_packet_release(packet);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "nx_tcp_socket_send failed with error 0x%02x\n", ret);
            return NXD_TO_CY_SECURE_SOCKETS_ERR(ret);
        }

        *bytes_sent += size;
    }

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
/*
 * @brief TLS network send callback.
 */
static cy_rslt_t tls_network_send_callback(void *context, const unsigned char *data_buffer, uint32_t data_buffer_length, uint32_t *bytes_sent)
{
    cy_rslt_t result;
    cy_socket_ctx_t *ctx;

    /* NetXSecure requires TCP socket, so We pass &ctx->nxd_socket.tcp in TLS parameters to CY TLS.
     * This send callback is invoked only for mbedTLS. Retrieve cy_socket_ctx_t pointer from  &ctx->nxd_socket.tcp.
     */

    ctx = CY_NETXDUO_GET_SOCKET_CONTEXT(context, cy_socket_ctx_t, tcp);

    result = network_send(ctx, data_buffer, data_buffer_length, bytes_sent);

    return secure_socket_to_tls_error(result);
}

/*-----------------------------------------------------------*/
/*
 * @brief Network receive helper function. Function call to this function should be protected by socket_mutex.
 */
static cy_rslt_t network_receive(void *context, unsigned char *buffer, uint32_t len, uint32_t *bytes_received)
{
    UINT ret = NX_SUCCESS;
    ULONG timeout;
    size_t total_received = 0;
    size_t toread = 0;
    size_t outoffset = 0;
    NX_PACKET *packet;
    uint8_t *data;
    uint16_t avail_data_length;
    uint16_t total_data_length;
    cy_rslt_t result;
    cy_socket_ctx_t *ctx = (cy_socket_ctx_t *)context;

    *bytes_received = 0;

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    do
    {
        if (!ctx->packet)
        {
            timeout = ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->recv_timeout);
            if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
            {
                ret = nx_udp_socket_receive(ctx->nxd_socket.udp, &packet, timeout);
            }
            else
            {
                ret = nx_tcp_socket_receive(ctx->nxd_socket.tcp, &packet, timeout);
            }
            if (ret != NX_SUCCESS)
            {
                /* If some amount of data already received, return success with amount of bytes received, else return error. */
                if (total_received)
                {
                    break;
                }
                else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "nx_socket_receive returned 0x%02x\n", ret);
                    return NXD_TO_CY_SECURE_SOCKETS_ERR(ret);
                }
            }

            if (packet->nx_packet_length == 0)
            {
                nx_packet_release(packet);
                ctx->packet = NULL;
                continue;
            }
            ctx->packet = packet;
            ctx->offset = 0;
        }

        do
        {
            result = nxd_packet_get_data(ctx->packet, ctx->offset, &data, &avail_data_length, &total_data_length);
            if (result != CY_RSLT_SUCCESS)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Error getting packet data (%d)\n", result);
                return CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR;
            }

            /*
             * This is the data left to read
             */

            toread = len - total_received;
            if (toread > avail_data_length)
            {
                toread = avail_data_length;
            }

            /*
             * Copy the data out
             */

            memcpy(buffer + outoffset, data, toread);
            ctx->offset += toread;

            /*
             * Keep track of the total received
             */

            total_received += toread;

            /*
             * Move the output pointer for the output buffer
             */

            outoffset += toread;

        } while (total_received < len && total_data_length > avail_data_length);

        /*
         * If we used up the current packet, we need to release it.
         * This will force another network read the next time through the loop.
         */

        if (ctx->offset >= ctx->packet->nx_packet_length)
        {
            nx_packet_release(ctx->packet);
            ctx->packet = NULL;
            ctx->offset = 0;
        }

    } while (total_received < len);

    *bytes_received = total_received;

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
/*
 * @brief TLS receive callback.
 */
static cy_rslt_t tls_network_receive_callback(void *context, unsigned char *buffer, uint32_t len, uint32_t *bytes_received)
{
    cy_rslt_t result;
    cy_socket_ctx_t *ctx;

    /* NetXSecure requires TCP socket, so we pass &ctx->nxd_socket.tcp in TLS parameters to CY TLS.
     * This send callback is invoked only for mbedTLS. Retrieve cy_socket_ctx_t pointer from  &ctx->nxd_socket.tcp.
     */
    ctx = CY_NETXDUO_GET_SOCKET_CONTEXT(context, cy_socket_ctx_t, tcp);

    result = network_receive(ctx, buffer, len, bytes_received);

    return secure_socket_to_tls_error(result);
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_worker_thread_params_t params;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_init Start\n");

    if (!init_ref_count)
    {
        result = cy_rtos_init_mutex(&socket_list_mutex);
        if (CY_RSLT_SUCCESS != result)
        {
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        result = cy_rtos_init_mutex(&accept_recv_event_mutex);
        if (CY_RSLT_SUCCESS != result)
        {
            cy_rtos_deinit_mutex(&socket_list_mutex);
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        memset(&multicast_info, 0 , sizeof(multicast_info));

        result = cy_rtos_init_mutex(&multicast_join_leave_mutex);
        if (CY_RSLT_SUCCESS != result)
        {
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        /* Allocate memory for multicast member list. */
        multicast_info.multicast_member_list = (cy_socket_multicast_pair_t *)malloc(sizeof(cy_socket_multicast_pair_t) * SECURE_SOCKETS_MAX_MULTICAST_GROUPS);
        if (multicast_info.multicast_member_list == NULL)
        {
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for multicast member list\n");
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&multicast_join_leave_mutex);
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        memset(&params, 0, sizeof(params));
        params.name        = "Socket";
        params.priority    = CY_RTOS_PRIORITY_ABOVENORMAL;
        params.stack       = NULL;
        params.stack_size  = SECURE_SOCKETS_THREAD_STACKSIZE;
        params.num_entries = 0;

        /* create a worker thread */
        result = cy_worker_thread_create(&socket_worker, &params);
        if (result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Create Worker Thread returned:0x%lx\n", result);
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&multicast_join_leave_mutex);
            if (multicast_info.multicast_member_list)
            {
                free(multicast_info.multicast_member_list);
                multicast_info.multicast_member_list = NULL;
            }
            return result;
        }
        /* Initialized the TLS library */
        result = cy_tls_init();
        if (result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_init returned:0x%lx\n", result);
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&multicast_join_leave_mutex);
            if (multicast_info.multicast_member_list)
            {
                free(multicast_info.multicast_member_list);
                multicast_info.multicast_member_list = NULL;
            }
            cy_worker_thread_delete(&socket_worker);
            return result;
        }
        init_ref_count++;
    }
    else
    {
        init_ref_count++;
    }
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_init End\n");

    return result;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_create(int domain, int type, int protocol, cy_socket_t *handle)
{
    cy_socket_ctx_t *ctx;
    NX_IP *netif;
    UINT status;
    cy_socket_interface_t iface_type = CY_SOCKET_STA_INTERFACE;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_create Start\n");

    if (handle == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NULL handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

#if !defined(NX_DISABLE_IPV6) && !defined(NX_DISABLE_IPV4)
    if ( (domain != CY_SOCKET_DOMAIN_AF_INET) && (domain != CY_SOCKET_DOMAIN_AF_INET6) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid domain\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#else
#if !defined(NX_DISABLE_IPV4)
    if (domain != CY_SOCKET_DOMAIN_AF_INET)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid domain\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#elif !defined(NX_DISABLE_IPV6)
    if (domain != CY_SOCKET_DOMAIN_AF_INET6)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid domain\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NX_DISABLE_IPV6 and NX_DISABLE_IPV4 are both defined\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION;
    }
#endif
#endif /* !defined(NX_DISABLE_IPV6) && !defined(NX_DISABLE_IPV4) */

    if ( !( ( (type == CY_SOCKET_TYPE_STREAM) && (protocol == CY_SOCKET_IPPROTO_TCP || protocol == CY_SOCKET_IPPROTO_TLS) ) ||
            ( (type == CY_SOCKET_TYPE_DGRAM)  && (protocol == CY_SOCKET_IPPROTO_UDP) ) ) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid type/protocol combination\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    netif = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);
    if (netif == NULL)
    {
        /* STA interface is not created, it can be AP only mode check if AP netif exists */
        netif = cy_network_get_nw_interface(CY_NETWORK_WIFI_AP_INTERFACE, 0);
        if (netif == NULL)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid network interface\n");
            *handle = CY_SOCKET_INVALID_HANDLE;
            return CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST;
        }
        iface_type = CY_SOCKET_AP_INTERFACE;
    }

    ctx = alloc_socket(protocol, iface_type);
    if (ctx == NULL)
    {
        *handle = CY_SOCKET_INVALID_HANDLE;
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "alloc_socket failed\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    if (protocol == CY_SOCKET_IPPROTO_TCP || protocol == CY_SOCKET_IPPROTO_TLS)
    {
        ctx->is_authmode_set = false;
        if (protocol == CY_SOCKET_IPPROTO_TLS)
        {
            ctx->enforce_tls = true;
            /* Set the default TLS authentication mode to verify required. */
            ctx->auth_mode = CY_SOCKET_TLS_VERIFY_REQUIRED;
        }

        status = nx_tcp_socket_create(netif, ctx->nxd_socket.tcp, (CHAR*)"", NX_IP_NORMAL, FRAGMENT_OPTION, NX_IP_TIME_TO_LIVE, DEFAULT_TCP_WINDOW_SIZE, NX_NULL, cy_tcp_disconnect_callback);
        if (status == NX_SUCCESS)
        {
            ctx->nxd_socket.tcp->nx_tcp_socket_reserved_ptr = (void *)ctx->id;
            nx_tcp_socket_receive_notify(ctx->nxd_socket.tcp, cy_tcp_receive_callback);
        }
    }
    else
    {
        status = nx_udp_socket_create(netif, ctx->nxd_socket.udp, (char*)"", 0, FRAGMENT_OPTION, 255, DEFAULT_UDP_QUEUE_SIZE);
        if (status == NX_SUCCESS)
        {
            ctx->nxd_socket.udp->nx_udp_socket_reserved_ptr = (void *)ctx->id;
            nx_udp_socket_receive_notify(ctx->nxd_socket.udp, cy_udp_receive_callback);
        }
    }

    if (status != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_socket_create failed\n");
        free_socket(ctx);
        return CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR;
    }

    ctx->recv_timeout = DEFAULT_RECV_TIMEOUT_IN_MSEC;
    ctx->send_timeout = DEFAULT_SEND_TIMEOUT_IN_MSEC;

    *handle = ctx;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_create End\n");
    return CY_RSLT_SUCCESS;
}


/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_setsockopt(cy_socket_t handle, int level, int optname, const void *optval, uint32_t optlen)
{
    cy_socket_ctx_t *ctx = NULL;
    cy_socket_opt_callback_t *callback_opt;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    if ( ((level < CY_SOCKET_SOL_SOCKET) || (level > CY_SOCKET_SOL_IP)) ||
         ((optval == NULL) && (optname != CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK) && (optname != CY_SOCKET_SO_RECEIVE_CALLBACK) && (optname != CY_SOCKET_SO_DISCONNECT_CALLBACK)) ||
         ((optval != NULL) && (optlen == 0)) )
    {
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    switch (level)
    {
        case CY_SOCKET_SOL_TLS:
            /* All TLS socket options are used in TLS handshake. So don't allow setting these
             * socket options when socket is in connected state.
             */
            if (ctx->status & SOCKET_STATUS_FLAG_CONNECTED)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket connected\n");

                cy_rtos_set_mutex(&ctx->socket_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                return CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED;
            }

            switch (optname)
            {
                case CY_SOCKET_SO_TLS_IDENTITY:
                {
                    ctx->tls_identity = optval;
                    break;
                }
                case CY_SOCKET_SO_TLS_AUTH_MODE:
                {
                    ctx->auth_mode = *((cy_socket_tls_auth_mode_t *)optval);
                    ctx->is_authmode_set = true;
                    break;
                }
                case CY_SOCKET_SO_SERVER_NAME_INDICATION:
                {
                    if (ctx->hostname)
                    {
                        free(ctx->hostname);
                    }
                    ctx->hostname = (char *)malloc(optlen + 1);
                    if (NULL == ctx->hostname)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for hostname\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }
                    memcpy(ctx->hostname, optval, optlen);
                    ctx->hostname[optlen] = '\0';
                    break;
                }

                case CY_SOCKET_SO_TLS_MFL:
                {
                    uint32_t mfl = *((uint32_t *)optval);
                    ctx->mfl_code = max_fragment_length_to_mfl_code(mfl);
                    if (SECURE_SOCKETS_MAX_FRAG_LEN_INVALID == ctx->mfl_code)
                    {
                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
                    break;
                }
                case CY_SOCKET_SO_TRUSTED_ROOTCA_CERTIFICATE:
                {
                    if (ctx->rootca_certificate)
                    {
                        /* free the previous configured root_ca if it exists */
                        free(ctx->rootca_certificate);
                    }

                    ctx->rootca_certificate = malloc(optlen + 1);
                    if (NULL == ctx->rootca_certificate)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for ca_cert\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }

                    memset(ctx->rootca_certificate, 0, optlen + 1);
                    memcpy(ctx->rootca_certificate, optval, optlen);
                    ctx->rootca_certificate_len = optlen;
                    break;
                }
                case CY_SOCKET_SO_ALPN_PROTOCOLS:
                {
                    char *ptr = NULL;
                    int count = 0;
                    int i = 0;

                    ctx->alpn = malloc(optlen+1);
                    if (NULL == ctx->alpn)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for alpn\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }
                    memcpy(ctx->alpn, optval, optlen);

                    ctx->alpn[optlen] = '\0';

                    /* find the number of protocols in the alpn list */
                    ptr = (char *)optval;
                    while (*ptr != '\0')
                    {
                        if (*ptr == ',')
                        {
                            count++;
                        }
                        ptr++;
                    }

                    ctx->alpn_count = count + 1;

                    /* mbedtls expects array of strings. Allocate memory for the array. */
                    ctx->alpn_list = (char **)malloc((ctx->alpn_count + 1) * sizeof(char *));
                    if (NULL == ctx->alpn_list)
                    {
                        free(ctx->alpn);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for alpn_list\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }

                    /* Convert the input alpn string to array of strings */
                    ptr = ctx->alpn;
                    while (*ptr != '\0')
                    {
                        ctx->alpn_list[i++] = (char*)ptr;

                        while (*ptr != ',' && *ptr != '\0')
                        {
                            ptr++;
                            if (*ptr == ',')
                            {
                                *ptr++ = '\0';
                                break;
                            }
                        }
                    }
                    ctx->alpn_list[i] = NULL;
                    break;
                }
#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT
                case CY_SOCKET_SO_ROOTCA_CERTIFICATE_LOCATION:
                {
                    uint8_t load_rootca_location = *((uint8_t *)optval);

                    if (load_rootca_location != CY_SOCKET_ROOTCA_SECURE_STORAGE && load_rootca_location != CY_SOCKET_ROOTCA_RAM)
                    {
                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    ctx->load_rootca_from_ram = load_rootca_location;
                    break;
                }
                case CY_SOCKET_SO_DEVICE_CERT_KEY_LOCATION:
                {
                    uint8_t load_device_cert_key_location = *((uint8_t *)optval);

                    if (load_device_cert_key_location != CY_SOCKET_DEVICE_CERT_KEY_SECURE_STORAGE && load_device_cert_key_location != CY_SOCKET_DEVICE_CERT_KEY_RAM)
                    {
                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    ctx->load_device_cert_key_from_ram = load_device_cert_key_location;
                    break;
                }
#endif
                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
            break;

        case CY_SOCKET_SOL_SOCKET:
            switch (optname)
            {
                case CY_SOCKET_SO_RCVTIMEO:
                {
                    ctx->recv_timeout = *((uint32_t *)optval);
                    ctx->is_recvtimeout_set = true;
                    break;
                }

                case CY_SOCKET_SO_SNDTIMEO:
                {
                    ctx->send_timeout = *((uint32_t *)optval);
                    break;
                }

                case CY_SOCKET_SO_NONBLOCK:
                {
                    ctx->nonblocking = *((uint8_t *)optval);
                    break;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE:
                {
                    int keep_alive = *((int *)optval);

                    if (keep_alive == 0 || keep_alive == 1)
                    {
                        if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
                        {
                            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid socket type\n");

                            cy_rtos_set_mutex(&ctx->socket_mutex);
                            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                        }
#ifdef NX_ENABLE_TCP_KEEPALIVE
                        if (keep_alive)
                        {
                            ctx->nxd_socket.tcp->nx_tcp_socket_keepalive_enabled = NX_TRUE;
                        }
                        else
                        {
                            ctx->nxd_socket.tcp->nx_tcp_socket_keepalive_enabled = NX_FALSE;
                        }
#else
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "TCP keepalive not enabled\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
#endif
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid option value\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
                    break;
                }

                case CY_SOCKET_SO_BROADCAST:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Incompatible Socket option\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_RECEIVE_CALLBACK:
                {
                    if (optval != NULL)
                    {
                        callback_opt = (cy_socket_opt_callback_t *) optval;

                        /*
                         * cy_process_receive_event function accesses ctx->callbacks.receive.callback, so we should protect initialization of
                         * ctx->callbacks.receive.callback pointer with mutex.
                         */
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\n", __FILE__, __LINE__);
                        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

                        ctx->callbacks.receive.callback = callback_opt->callback;
                        ctx->callbacks.receive.arg      = callback_opt->arg;

                        cy_rtos_set_mutex(&accept_recv_event_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\n", __FILE__, __LINE__);
                        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

                        ctx->callbacks.receive.callback = NULL;

                        cy_rtos_set_mutex(&accept_recv_event_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);
                    }
                    break;
                }

                case CY_SOCKET_SO_DISCONNECT_CALLBACK:
                {
                    if (optval != NULL)
                    {
                        callback_opt = (cy_socket_opt_callback_t *) optval;
                        ctx->callbacks.disconnect.callback = callback_opt->callback;
                        ctx->callbacks.disconnect.arg      = callback_opt->arg;
                    }
                    else
                    {
                        ctx->callbacks.disconnect.callback = NULL;
                    }
                    break;
                }
                case CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK:
                {
                    if (optval != NULL)
                    {
                        callback_opt = (cy_socket_opt_callback_t *)optval;
                        ctx->callbacks.connect_request.callback = callback_opt->callback;
                        ctx->callbacks.connect_request.arg      = callback_opt->arg;
                    }
                    else
                    {
                        ctx->callbacks.connect_request.callback = NULL;
                    }
                    break;
                }

                case CY_SOCKET_SO_BINDTODEVICE:
                {
                    cy_socket_interface_t iface_type = ( *(cy_socket_interface_t *)optval);
                    UINT status = NX_SUCCESS;

                    if ( (ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ||
                         (ctx->status & SOCKET_STATUS_FLAG_LISTENING) )
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket connected or listening\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED;
                    }

                    if (iface_type != ctx->iface_type)
                    {
                        NX_IP *netif;
                        cy_network_hw_interface_type_t hw_iface_type;

                        if (iface_type == CY_SOCKET_STA_INTERFACE)
                        {
                            hw_iface_type = CY_NETWORK_WIFI_STA_INTERFACE;
                        }
                        else
                        {
                            hw_iface_type = CY_NETWORK_WIFI_AP_INTERFACE;
                        }

                        netif = cy_network_get_nw_interface(hw_iface_type, 0);
                        if (netif == NULL)
                        {
                            cy_rtos_set_mutex(&ctx->socket_mutex);
                            return CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST;
                        }
                        if (ctx->transport_protocol == CY_SOCKET_IPPROTO_TCP)
                        {
                            status = nx_tcp_socket_delete(ctx->nxd_socket.tcp);
                            if (status == NX_SUCCESS)
                            {
                                status = nx_tcp_socket_create(netif, ctx->nxd_socket.tcp, (CHAR*)"", NX_IP_NORMAL, FRAGMENT_OPTION, NX_IP_TIME_TO_LIVE, DEFAULT_TCP_WINDOW_SIZE, NX_NULL, cy_tcp_disconnect_callback);
                            }
                        }
                        else
                        {
                            status = nx_udp_socket_delete(ctx->nxd_socket.udp);
                            if (status == NX_SUCCESS)
                            {
                                status = nx_udp_socket_create(netif, ctx->nxd_socket.udp, (char*)"", 0, FRAGMENT_OPTION, 255, DEFAULT_UDP_QUEUE_SIZE);
                            }
                        }
                        ctx->iface_type = iface_type;
                    }

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    if (status != NX_SUCCESS)
                    {
                        return nxd_to_secure_socket_error(status);
                    }
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);
                    break;
                }

                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
            break;

        case CY_SOCKET_SOL_TCP:
            switch (optname)
            {
                case CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL:
                {
                    /*
                     * NetXDuo uses a compile time define, NX_TCP_KEEPALIVE_RETRY, to specify
                     * the number of seconds between retries of the keepalive timer assuming
                     * the other side of the connection is not responding.
                     */
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_COUNT:
                {
                    /*
                     * NetXDuo uses a compile time define, NX_TCP_KEEPALIVE_RETRIES, to specify
                     * how many keepalive retries are allowed before the connection is deemed broken.
                     */
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME:
                {
                    /*
                     * NetXDuo uses a compile time define, NX_TCP_KEEPALIVE_INITIAL, to specify
                     * the number of seconds of inactivity before the keepalive timer activates.
                     */
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_NODELAY:
                {
                    /*
                     * NetXDuo TCP ACK behavior set at compile time.
                     */
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\r\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
            break;

        case CY_SOCKET_SOL_IP:
            switch (optname)
            {
                case CY_SOCKET_SO_JOIN_MULTICAST_GROUP:
                case CY_SOCKET_SO_LEAVE_MULTICAST_GROUP:
                {
                    NXD_ADDRESS if_addr;
                    NXD_ADDRESS multi_addr;
                    int member_index;
                    UINT err = NX_SUCCESS;
                    cy_rslt_t result = CY_RSLT_SUCCESS;
                    const cy_socket_ip_mreq_t *imr = (const cy_socket_ip_mreq_t *)optval;

                    convert_secure_socket_to_nxd_ip_addr(&if_addr, &imr->if_addr);
                    convert_secure_socket_to_nxd_ip_addr(&multi_addr, &imr->multi_addr);

                    /* Check if the address to join/leave is a valid multiast address. */
                    if (!ip_addr_ismulticast(&multi_addr))
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid multicast address\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    do
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex locked %s %d\n", __FILE__, __LINE__);
                        cy_rtos_get_mutex(&multicast_join_leave_mutex, CY_RTOS_NEVER_TIMEOUT);

                        if (imr->if_addr.version != imr->multi_addr.version)
                        {
                            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "multicast group address type and interface address type are not same\n");
                            result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                            break;
                        }

                        member_index = find_multicast_member_index(ctx, imr);

                        if (optname == CY_SOCKET_SO_JOIN_MULTICAST_GROUP)
                        {
                            if (member_index != -1)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);
                                result = CY_RSLT_MODULE_SECURE_SOCKETS_ADDRESS_IN_USE;
                                break;
                            }

                            if (multicast_info.multicast_member_count == SECURE_SOCKETS_MAX_MULTICAST_GROUPS)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Registered multicast member count is already reached maximum allowed count %s %d\n", __FILE__, __LINE__);
                                result = CY_RSLT_MODULE_SECURE_SOCKETS_MAX_MEMBERSHIP_ERROR;
                                break;
                            }

                            member_index = next_free_multicast_slot(0);

                            /* Call wifi-mw-core network activity function to resume the network stack. */
                            cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

#ifndef NX_DISABLE_IPV4
                            if (if_addr.nxd_ip_version == NX_IP_VERSION_V4)
                            {
                                err = nx_igmp_multicast_join(ctx->nxd_socket.udp->nx_udp_socket_ip_ptr, multi_addr.nxd_ip_address.v4);
                                if (err != NX_SUCCESS)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_igmp_multicast_join failed with error 0x%02x\n", err);
                                    break;
                                }
                            }
#endif
#ifndef NX_DISABLE_IPV6
                            if (if_addr.nxd_ip_version == NX_IP_VERSION_V6)
                            {
                                err = nxd_ipv6_multicast_interface_join(ctx->nxd_socket.udp->nx_udp_socket_ip_ptr, &multi_addr, CY_NETWORK_PRIMARY_INTERFACE);
                                if (err != NX_SUCCESS)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nxd_ipv6_multicast_interface_join failed with error 0x%02x\n", err);
                                    break;
                                }
                            }
#endif
                            set_multicast_slot_status_bit(member_index);
                            memcpy(&multicast_info.multicast_member_list[member_index].if_addr, &imr->if_addr, sizeof(imr->if_addr));
                            memcpy(&multicast_info.multicast_member_list[member_index].multi_addr, &imr->multi_addr, sizeof(imr->multi_addr));
                            multicast_info.multicast_member_list[member_index].socket = ctx;
                            multicast_info.multicast_member_count++;
                        }
                        else
                        {
                            if (member_index == -1)
                            {
                                result = CY_RSLT_MODULE_SECURE_SOCKETS_MULTICAST_ADDRESS_NOT_REGISTERED;
                                break;
                            }

                            clear_multicast_slot_status_bit(member_index);
                            multicast_info.multicast_member_count--;

                            /* Call wifi-mw-core network activity function to resume the network stack. */
                            cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

#ifndef NX_DISABLE_IPV4
                            if (if_addr.nxd_ip_version == NX_IP_VERSION_V4)
                            {
                                err = nx_igmp_multicast_leave(ctx->nxd_socket.udp->nx_udp_socket_ip_ptr, multi_addr.nxd_ip_address.v4);
                                if (err != NX_SUCCESS)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_igmp_multicast_leave failed with error 0x%02x\n", err);
                                    break;
                                }
                            }
#endif
#ifndef NX_DISABLE_IPV6
                            if (if_addr.nxd_ip_version == NX_IP_VERSION_V6)
                            {
                                err = nxd_ipv6_multicast_interface_leave(ctx->nxd_socket.udp->nx_udp_socket_ip_ptr, &multi_addr, CY_NETWORK_PRIMARY_INTERFACE);
                                if (err != NX_SUCCESS)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nxd_ipv6_multicast_interface_leave failed with error 0x%02x\n", err);
                                    break;
                                }
                            }
#endif
                        }
                    } while(0);

                    cy_rtos_set_mutex(&multicast_join_leave_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    if (err != NX_SUCCESS)
                    {
                        result = nxd_to_secure_socket_error(err);
                    }

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return result;
                }

                case CY_SOCKET_SO_IP_MULTICAST_TTL:
                {
                    if (ctx->transport_protocol != CY_SOCKET_IPPROTO_UDP)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Multicast TTL option is supported only for UDP sockets\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                    }
                    ctx->nxd_socket.udp->nx_udp_socket_time_to_live = *(uint8_t *)optval;
                    break;
                }

                case CY_SOCKET_SO_IP_TOS:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
                default:
                {
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }

            }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt End\n");
    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_getsockopt(cy_socket_t handle, int level, int optname, void *optval, uint32_t *optlen)
{
    cy_socket_ctx_t *ctx = NULL;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (optval == NULL || optlen == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_getsockopt bad arg\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    switch (level)
    {
        case CY_SOCKET_SOL_TLS:
        {
            switch (optname)
            {
                case CY_SOCKET_SO_TLS_AUTH_MODE:
                {
                    if (*optlen < sizeof(cy_socket_tls_auth_mode_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *((cy_socket_tls_auth_mode_t *)optval) = (cy_socket_tls_auth_mode_t)ctx->auth_mode;
                    *optlen = sizeof(cy_socket_tls_auth_mode_t);
                    break;
                }
                case CY_SOCKET_SO_SERVER_NAME_INDICATION:
                {
                    if (ctx->hostname)
                    {
                        if (*optlen < strlen(ctx->hostname))
                        {
                            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                            cy_rtos_set_mutex(&ctx->socket_mutex);
                            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                        }

                        *optlen = strlen(ctx->hostname);
                        memcpy(optval, ctx->hostname, *optlen);
                    }
                    else
                    {
                        *optlen = 0;
                    }
                    break;
                }

                case CY_SOCKET_SO_TLS_MFL:
                {
                    uint32_t mfl;

                    if (*optlen < sizeof(mfl))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    mfl = mfl_code_to_max_fragment_length(ctx->mfl_code);
                    *((uint32_t *)optval) = mfl;
                    *optlen = sizeof(mfl);
                    break;
                }
                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        case CY_SOCKET_SOL_SOCKET:
        {
            switch (optname)
            {
                case CY_SOCKET_SO_RCVTIMEO:
                {
                    if (*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *((uint32_t *)optval) = ctx->recv_timeout;
                    *optlen = sizeof(uint32_t);
                    break;
                }

                case CY_SOCKET_SO_SNDTIMEO:
                {
                    if (*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *((uint32_t *)optval) = ctx->send_timeout;
                    *optlen = sizeof(uint32_t);
                    break;
                }

                case CY_SOCKET_SO_BYTES_AVAILABLE:
                {
                    ULONG recv_avail = 0;

                    if (*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    /* Get the number of bytes available */
                    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
                    {
                        nx_udp_socket_bytes_available(ctx->nxd_socket.udp, &recv_avail);
                        if (ctx->packet)
                        {
                            recv_avail += ctx->packet->nx_packet_length - ctx->offset;
                        }
                    }
                    else
                    {
                        nx_tcp_socket_bytes_available(ctx->nxd_socket.tcp, &recv_avail);
                    }

                    *(uint32_t *)optval = recv_avail;
                    *optlen = sizeof(recv_avail);
                    break;
                }

                case CY_SOCKET_SO_NONBLOCK:
                {
                    if (*optlen < sizeof(uint8_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *(uint8_t *)optval = ctx->nonblocking;
                    *optlen = sizeof(uint8_t);
                    break;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE:
                {
                    if (*optlen < sizeof(int))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid socket type\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
#ifdef NX_ENABLE_TCP_KEEPALIVE
                    if (ctx->nxd_socket.tcp->nx_tcp_socket_keepalive_enabled)
                    {
                        *(int *)optval = 1;
                    }
                    else
                    {
                        *(int *)optval = 0;
                    }
                    *optlen = sizeof(int);
#else
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "TCP keepalive not enabled\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
#endif
                    break;
                }

                case CY_SOCKET_SO_BROADCAST:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Incompatible Socket option\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_BINDTODEVICE:
                {
                    if (*optlen < sizeof(cy_socket_interface_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *((cy_socket_interface_t *)optval) = (cy_socket_interface_t)ctx->iface_type;
                    *optlen = sizeof(cy_socket_interface_t);

                    break;
                }

                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        case CY_SOCKET_SOL_TCP:
        {
            switch (optname)
            {
                case CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_COUNT:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_NODELAY:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        case CY_SOCKET_SOL_IP:
        {
            switch (optname)
            {
                case CY_SOCKET_SO_IP_MULTICAST_TTL:
                {
                    if (*optlen < sizeof(uint8_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    if (ctx->transport_protocol != CY_SOCKET_IPPROTO_UDP)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Multicast TTL option is supported only for UDP sockets\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                    }
                    *(uint8_t *)optval = ctx->nxd_socket.udp->nx_udp_socket_time_to_live;
                    *optlen = sizeof(uint8_t);

                    break;
                }

                case CY_SOCKET_SO_IP_TOS:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        default:
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid level\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
        }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt End\n");
    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_connect(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t address_length)
{
    cy_socket_ctx_t *ctx;
    NXD_ADDRESS remote;
    cy_tls_params_t tls_params = { 0 };
    UINT ret = NX_SUCCESS;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    UNUSED_ARG(address_length);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_connect Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (address == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Address passed is NULL\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    /* Connect not supported for UDP sockets */
    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_connect API is not supported for UDP sockets\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    /* Check if socket is not yet bound to a local port */
    if (!ctx->nxd_socket.tcp->nx_tcp_socket_bound_next)
    {
        ret = nx_tcp_client_socket_bind(ctx->nxd_socket.tcp, NX_ANY_PORT, NX_TIMEOUT(DEFAULT_BIND_TIMEOUT));
        if (ret != NX_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_tcp_client_socket_bind failed with error = 0x%02x\n", ret);
            result = NXD_TO_CY_SECURE_SOCKETS_ERR(ret);

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return result;
        }
    }

    memset(&remote, 0, sizeof(NXD_ADDRESS));

    /* convert IP format from secure socket to NetXDuo */
    result = convert_secure_socket_to_nxd_ip_addr(&remote, &address->ip_address);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to NetXDuo\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    ret = nxd_tcp_client_socket_connect(ctx->nxd_socket.tcp, &remote, address->port, ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->send_timeout));
    if (ret != NX_SUCCESS)
    {
        nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);

        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nxd_tcp_client_socket_connect failed with error = 0x%02x\n", ret);
        result = NXD_TO_CY_SECURE_SOCKETS_ERR(ret);
        goto exit;
    }
    ctx->status |= SOCKET_STATUS_FLAG_CONNECTED;

    if (ctx->enforce_tls)
    {
        tls_params.context = &ctx->nxd_socket.tcp;
        tls_params.network_send = tls_network_send_callback;
        tls_params.network_recv = tls_network_receive_callback;
        tls_params.tls_identity = ctx->tls_identity;
        tls_params.rootca_certificate = ctx->rootca_certificate;
        tls_params.rootca_certificate_length = ctx->rootca_certificate_len;
        tls_params.auth_mode = ctx->auth_mode;
        tls_params.mfl_code  = ctx->mfl_code;
        tls_params.hostname  = ctx->hostname;
        tls_params.alpn_list = (const char**)ctx->alpn_list;
#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT
        tls_params.load_rootca_from_ram = ctx->load_rootca_from_ram;
        tls_params.load_device_cert_key_from_ram = ctx->load_device_cert_key_from_ram;
#endif
        result = cy_tls_create_context(&ctx->tls_ctx, &tls_params);
        if (result != CY_RSLT_SUCCESS)
        {
            nx_tcp_socket_disconnect(ctx->nxd_socket.tcp, ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->send_timeout));
            nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
            ctx->status &= ~(SOCKET_STATUS_FLAG_CONNECTED);

            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_create_context failed with error %lu\n", result);
            result = TLS_TO_CY_SECURE_SOCKETS_ERR(result);
            goto exit;
        }
        result = cy_tls_connect(ctx->tls_ctx, CY_TLS_ENDPOINT_CLIENT, ctx->send_timeout);
        if (result != CY_RSLT_SUCCESS)
        {
            nx_tcp_socket_disconnect(ctx->nxd_socket.tcp, ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->send_timeout));
            nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
            ctx->status &= ~(SOCKET_STATUS_FLAG_CONNECTED);

            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_connect failed with error %lu\n", result);
            result = TLS_TO_CY_SECURE_SOCKETS_ERR(result);
            goto exit;
        }
        ctx->status |= SOCKET_STATUS_FLAG_SECURED;
    }

exit:
    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_connect End\n");

    return result;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_disconnect(cy_socket_t handle, uint32_t timeout)
{
    cy_socket_ctx_t *ctx;
    UINT error;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_disconnect Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_disconnect API is not supported for UDP sockets\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    if (ctx->role == CY_TLS_ENDPOINT_CLIENT)
    {
        if ((ctx->status & SOCKET_STATUS_FLAG_CONNECTED) != SOCKET_STATUS_FLAG_CONNECTED)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Socket not connected\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
        }

        if (true == ctx->enforce_tls && (ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED)
        {
            cy_tls_delete_context(ctx->tls_ctx);

            /* Free the memory allocated for RootCA certificate for the socket.*/
            if (ctx->rootca_certificate)
            {
                free(ctx->rootca_certificate);
                ctx->rootca_certificate = NULL;
            }
            ctx->status &= ~(SOCKET_STATUS_FLAG_SECURED);
        }
        ctx->status &= ~(SOCKET_STATUS_FLAG_CONNECTED);
    }
    else
    {
        if ((ctx->status & SOCKET_STATUS_FLAG_LISTENING) != SOCKET_STATUS_FLAG_LISTENING)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Socket not listening\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_LISTENING;
        }

        /* If disconnect is called on server socket and it is in listening state, stop listening on the socket. */
        ctx->status &= ~(SOCKET_STATUS_FLAG_LISTENING);
        nx_tcp_server_socket_unlisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->nxd_socket.tcp->nx_tcp_socket_port);
    }

    /* Free the packet that is stored when partial data is copied from it */
    if (ctx->packet)
    {
        nx_packet_release(ctx->packet);
        ctx->packet = NULL;
    }

    /*
     * Disconnect the socket.
     */

    error = nx_tcp_socket_disconnect(ctx->nxd_socket.tcp, NX_TIMEOUT(timeout));
    if (ctx->nxd_socket.tcp->nx_tcp_socket_client_type == NX_TRUE)
    {
        /*
         * Un-bind the socket, so the TCP port becomes available for other sockets which are suspended on bind requests.
         * This will also flush the receive queue of the socket
         */

        nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
    }
    else
    {
        nx_tcp_server_socket_unaccept(ctx->nxd_socket.tcp);
    }

    if (error != NX_SUCCESS)
    {
        if (error == NX_NOT_CONNECTED)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "nx_tcp_socket_disconnect failed with error 0x%02x\n", error);
        }
        else
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_tcp_socket_disconnect failed with error 0x%02x (timeout: %d)\n", error, timeout);
        }
    }

    /* Release the mutex as connection status is already updated and done with NetXDuo close/delete */
    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_disconnect End\r\n");
    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_send(cy_socket_t handle, const void *data, uint32_t size, int flags, uint32_t *bytes_sent)
{
    cy_socket_ctx_t *ctx;
    cy_rslt_t ret;

    UNUSED_ARG(flags);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_send Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (bytes_sent == NULL || data == NULL || size == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "bytes_sent or data pointer is NULL or size is zero\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_send API is not supported for UDP sockets\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    /* Check if the socket is not in connected state. */
    if ( !( (!ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED) ||
            ( (ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) ) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket not connected\n");
        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
    }

    if (ctx->enforce_tls)
    {
        /* Send through TLS pipe. */
        ret = cy_tls_send(ctx->tls_ctx, data, size, ctx->send_timeout, bytes_sent);
        if (ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_tls_send failed with error code %lu\n", ret);
            ret = TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
    }
    else
    {
        ret = network_send((void *)ctx, data, size, bytes_sent);
        if (ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_network_send failed with error code %lu\n", ret);
        }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_send End\n");
    return ret;
}

/*-----------------------------------------------------------*/

cy_rslt_t cy_socket_sendto(cy_socket_t handle, const void *buffer, uint32_t length, int flags, const cy_socket_sockaddr_t *dest_addr, uint32_t address_length, uint32_t *bytes_sent)
{
    cy_socket_ctx_t *ctx;
    NX_PACKET *packet = NULL;
    NX_PACKET_POOL *packet_pool;
    UINT err;
    NXD_ADDRESS remote;
    cy_rslt_t result;

    UNUSED_ARG(flags);
    UNUSED_ARG(address_length);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_sendto Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (dest_addr == NULL || buffer == NULL || bytes_sent == NULL || length == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "dest_addr or buffer or bytes_sent pointers are NULL or length is zero\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    *bytes_sent = 0;

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_TCP || ctx->transport_protocol == CY_SOCKET_IPPROTO_TLS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_sendto API is not supported for TCP/TLS sockets\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    memset(&remote, 0, sizeof(remote));

    /* convert IP format from secure socket to LWIP */
    result = convert_secure_socket_to_nxd_ip_addr(&remote, &dest_addr->ip_address);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to NetXDuo\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    /* Create a packet buffer */
    result = cy_network_get_packet(CY_NETWORK_UDP_PACKET, DEFAULT_PACKET_ALLOCATE_TIMEOUT, (void *)&packet);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_netxduo_get_packet failed with error %d\n", result);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    /* Get packet pool to allocate a packet */
    result = cy_network_get_packet_pool(CY_NETWORK_PACKET_TX, (void *)&packet_pool);
    if (result != CY_RSLT_SUCCESS)
    {
        nx_packet_release(packet);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_network_get_packet_pool failed: 0x%x\n", result);
        return CY_RSLT_MODULE_TLS_OUT_OF_HEAP_SPACE;
    }

    /* Add data to the packet's payload. 
     * The below API will automatically perform packet chaining, if enabled. If there isn't enough
     * packets available in the packet pool for the amount of data to be sent, it will immediately
     * return with error.
     */
    err = nx_packet_data_append(packet, (void *)buffer, length, packet_pool, ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->send_timeout));
    if(err != NX_SUCCESS)
    {
        nx_packet_release(packet);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_packet_data_append failed with error 0x%02x\n", err);
        return NXD_TO_CY_SECURE_SOCKETS_ERR(err);
    }

     /*
     * Check if socket is bound. NetXDuo requires that the socket be bound to a port.
     */
    if (ctx->nxd_socket.udp->nx_udp_socket_bound_next == NULL)
    {
        /*
         * Not bound. We'll be nice and bind to a port automatically since LwIP
         * doesn't require that the UDP be bound.
         */
        nx_udp_socket_bind(ctx->nxd_socket.udp, NX_ANY_PORT, NX_TIMEOUT(DEFAULT_BIND_TIMEOUT));
    }

    /* Send data */
    err = nxd_udp_socket_send(ctx->nxd_socket.udp, packet, &remote, dest_addr->port);
    if (err != NX_SUCCESS)
    {
        nx_packet_release(packet);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nxd_udp_socket_send failed with error 0x%02x\n", err);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return NXD_TO_CY_SECURE_SOCKETS_ERR(err);
    }

    *bytes_sent = length;

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_sendto End\n");

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_recv(cy_socket_t handle, void *data, uint32_t size, int flags, uint32_t *bytes_received)
{
    cy_socket_ctx_t *ctx;
    cy_rslt_t ret;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recv Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* buffer cannot be null or buffer length cannot be 0 */
    if (data == NULL || size == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Buffer passed is NULL or buffer length is passed as 0\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if (bytes_received == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "bytes_received passed as NULL\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_recv API is not support for UDP sockets\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    /* Check if the socket is not in connected state. */
    if( !( (!ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED) ||
           ( (ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) ) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket not connected\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
    }

    if (ctx->enforce_tls)
    {
        /* Send through TLS pipe, if negotiated. */
        ret = cy_tls_recv(ctx->tls_ctx, data, size, ctx->recv_timeout, bytes_received);
        if (ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_tls_recv failed with error %ld\n", ret);
            ret = TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
    }
    else
    {
        ret = network_receive((void *)ctx, data, size, bytes_received);
        if (ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "network_recv_callback failed with error [0x%lX]\n", ret);
        }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recv End\n");
    return ret;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_recvfrom(cy_socket_t handle, void *buffer, uint32_t length, int flags, cy_socket_sockaddr_t *src_addr, uint32_t *src_addr_length, uint32_t *bytes_received)
{
    cy_socket_ctx_t *ctx = NULL;
    NXD_ADDRESS addr;
    NXD_ADDRESS remote;
    NX_PACKET *packet = NULL;
    UINT port;
    UINT err;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    bool src_filter_set = false;
    cy_socket_sockaddr_t peer_addr;

    UNUSED_ARG(src_addr_length);

    memset(&peer_addr, 0, sizeof(cy_socket_sockaddr_t));

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recvfrom Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* buffer cannot be null or buffer length cannot be 0 */
    if (buffer == NULL || length == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Buffer passed is NULL or buffer length is passed as 0\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if (bytes_received == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "bytes_received passed as NULL\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_TCP || ctx->transport_protocol == CY_SOCKET_IPPROTO_TLS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_recvfrom API is not supported for TCP/TLS sockets\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    *bytes_received = 0;

    /* src_addr cannot be null when flag passed as CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER */
    if ((flags & CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER) == CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER)
    {
        if (src_addr == NULL)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Flag CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER is set. Hence src_addr can not be null\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
        }
        else
        {
            src_filter_set = true;
        }
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    if (src_filter_set == true)
    {
        memset(&remote, 0, sizeof(remote));

        /* convert IP format from secure socket to NetXDuo */
        result = convert_secure_socket_to_nxd_ip_addr(&remote, &src_addr->ip_address);
        if (result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to NetXDuo\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return result;
        }
    }

    /*
     * The main receive operation is in a loop in case we have a source filter.
     */
    do
    {
        err = nx_udp_socket_receive(ctx->nxd_socket.udp, &packet, ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->recv_timeout));
        if (err != NX_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_udp_socket_receive failed with error 0x%02x\n", err);
            result = NXD_TO_CY_SECURE_SOCKETS_ERR(err);
            goto exit;
        }

        /*
         * Extract the sender's address.
         */

        nxd_udp_source_extract(packet, &addr, &port);

        if (src_filter_set == true)
        {
            /*
             * Make sure the sender is the one we want.
             */

            if (!ip_addrs_same(&addr, &remote) || port != src_addr->port)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Sender addr doesn't match src_filter\n");
                continue;
            }
        }
    } while (0);

    /* convert IP format from LWIP to secure socket */
    result = convert_nxd_to_secure_socket_ip_addr(&peer_addr.ip_address, &addr);
    if(result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from NetXDuo to secure socket\n");
        goto exit;
    }
    peer_addr.port = port;

    /* Copy the data from the packet to the caller's buffer */
    *bytes_received = (uint32_t)(packet->nx_packet_length); // nx_packet_length indicates total number of bytes received to be read
    if (*bytes_received > length)
    {
        *bytes_received = length;
    }

    memcpy(buffer, packet->nx_packet_prepend_ptr, *bytes_received);

    /* Copy source address */
    if (src_addr != NULL)
    {
        memcpy(src_addr, &peer_addr, sizeof(cy_socket_sockaddr_t));
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recvfrom End\n");

exit:
    if (packet != NULL)
    {
        nx_packet_release(packet);
        packet = NULL;
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recvfrom End\n");

    return result;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_gethostbyname(const char *hostname, cy_socket_ip_version_t ip_ver, cy_socket_ip_address_t *addr)
{
    cy_rslt_t result;
    NXD_ADDRESS ip_address;
    ULONG lookup_type;
    bool ip_found = false;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_gethostbyname Start\n");
    if ((NULL == hostname) || (NULL == addr))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid arg\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    lookup_type = (ip_ver == CY_SOCKET_IP_VER_V4 ? NX_IP_VERSION_V4 : NX_IP_VERSION_V6);

    if (ip_ver == CY_SOCKET_IP_VER_V6)
    {
#ifndef NX_DISABLE_IPV6
        lookup_type = NX_IP_VERSION_V6;
#else
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV6 not enabled\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
#endif
    }
    else if (ip_ver == CY_SOCKET_IP_VER_V4)
    {
#ifndef NX_DISABLE_IPV4
        lookup_type = NX_IP_VERSION_V4;
#else
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "IPV4 not enabled\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
#endif
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid IP version type\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if (lookup_type == NX_IP_VERSION_V4)
    {
        uint32_t ip_addr = 0;

        /*
         * Check to see if we've been given an IP address string for the hostname.
         */

        ip_found = str_to_ip(hostname, &ip_addr);
        if (ip_found)
        {
            /*
             * NetXDuo addresses need to be in host byte order.
             */

            ip_address.nxd_ip_version    = NX_IP_VERSION_V4;
            ip_address.nxd_ip_address.v4 = ntohl(ip_addr);
        }
    }
    if (lookup_type == NX_IP_VERSION_V6)
    {
        uint32_t ipv6_addr[4]={0,0,0,0};
        ip_found = str_to_ipv6(hostname, ipv6_addr);

        if (ip_found)
        {
            ip_address.nxd_ip_version    = NX_IP_VERSION_V6;

            ip_address.nxd_ip_address.v6[0] = ipv6_addr[0];
            ip_address.nxd_ip_address.v6[1] = ipv6_addr[1];
            ip_address.nxd_ip_address.v6[2] = ipv6_addr[2];
            ip_address.nxd_ip_address.v6[3] = ipv6_addr[3];
        }
    }

    if (!ip_found)
    {
        /* Call wifi-mw-core network activity function to resume the network stack. */
        cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

        result = cy_network_get_hostbyname(CY_NETWORK_WIFI_STA_INTERFACE, (UCHAR *)hostname, lookup_type, DEFAULT_DNS_TIMEOUT, &ip_address);
        if (result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_netxduo_get_hostbyname failed with error code %d\n", result);
            return CY_RSLT_MODULE_SECURE_SOCKETS_HOST_NOT_FOUND;
        }
    }

    /* convert IP format from NXD to secure socket */
    result = convert_nxd_to_secure_socket_ip_addr(addr, &ip_address);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from NetXDuo to secure socket\n");
        return result;
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_gethostbyname End\n");

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_poll(cy_socket_t handle, uint32_t *rwflags, uint32_t timeout)
{
    cy_socket_ctx_t *ctx;
    uint32 flags;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    ULONG recv_avail = 0;
    bool send_avail = false;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_poll Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (rwflags == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "rwflags pointer passed is NULL\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    flags = *rwflags;
    *rwflags = 0;

    if ( ( ctx->transport_protocol != CY_SOCKET_IPPROTO_UDP ) &&
         !( ( !ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED ) ||
         ( ( ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) ) )
    {
        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
    }

    if (flags & CY_SOCKET_POLL_READ)
    {
        if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
        {
            nx_udp_socket_bytes_available(ctx->nxd_socket.udp, &recv_avail);
        }
        else
        {
            nx_tcp_socket_bytes_available(ctx->nxd_socket.tcp, &recv_avail);
        }
        if (recv_avail > 0 || ctx->packet)
        {
            *rwflags |= CY_SOCKET_POLL_READ;
        }
        else
        {
            while (timeout > 0)
            {
                if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
                {
                    nx_udp_socket_bytes_available(ctx->nxd_socket.udp, &recv_avail);
                }
                else
                {
                    nx_tcp_socket_bytes_available(ctx->nxd_socket.tcp, &recv_avail);
                }
                if (recv_avail > 0 || ctx->packet)
                {
                    *rwflags |= CY_SOCKET_POLL_READ;
                    break ;
                }

                cy_rtos_delay_milliseconds(1);
                if (timeout != 0xffffffff)
                {
                    timeout--;
                }
            }
        }
    }

    if (flags & CY_SOCKET_POLL_WRITE)
    {
        if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
        {
            send_avail = is_nx_udp_send_available(ctx->nxd_socket.udp);
        }
        else
        {
            send_avail = is_nx_tcp_send_available(ctx->nxd_socket.tcp);
        }
        if (send_avail)
        {
            *rwflags |= CY_SOCKET_POLL_WRITE;
        }
        else
        {
            while (timeout > 0)
            {
                if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
                {
                    send_avail = is_nx_udp_send_available(ctx->nxd_socket.udp);
                }
                else
                {
                    send_avail = is_nx_tcp_send_available(ctx->nxd_socket.tcp);
                }
                if (send_avail > 0)
                {
                    *rwflags |= CY_SOCKET_POLL_WRITE;
                    break ;
                }

                cy_rtos_delay_milliseconds(1);
                if (timeout != 0xffffffff)
                {
                    timeout--;
                }
            }
        }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_poll End\r\n");
    return result;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_delete(cy_socket_t handle)
{
    cy_socket_ctx_t *ctx;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_delete Start\r\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (ctx->transport_protocol != CY_SOCKET_IPPROTO_UDP)
    {
        /* disconnect the socket */
        cy_socket_disconnect(ctx, 0);

        /*
         * Make sure we free up any resources which may have been allocated for the socket.
         * We don't check the return status since there's nothing we can do about it.
         */

        nx_tcp_server_socket_unaccept(ctx->nxd_socket.tcp);

        if (ctx->status & SOCKET_STATUS_FLAG_LISTENING)
        {
            nx_tcp_server_socket_unlisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->nxd_socket.tcp->nx_tcp_socket_port);
        }
        nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
        nx_tcp_socket_delete(ctx->nxd_socket.tcp);

        if (ctx->hostname)
        {
            free(ctx->hostname);
            ctx->hostname = NULL;
        }

        /* free alpn string */
        if (ctx->alpn)
        {
            free(ctx->alpn);
            ctx->alpn = NULL;
        }

        /* free alpn array of strings */
        if (ctx->alpn_list)
        {
            free(ctx->alpn_list);
            ctx->alpn_list = NULL;
        }
    }
    else
    {
        int member_index = 0;
        int count = 0;
        int num_multicast_groups = 0;
        NXD_ADDRESS if_addr;
        NXD_ADDRESS multi_addr;
        UINT err = NX_SUCCESS;

        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&multicast_join_leave_mutex, CY_RTOS_NEVER_TIMEOUT);

        /* Call wifi-mw-core network activity function to resume the network stack. */
        cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

        num_multicast_groups = multicast_info.multicast_member_count;
        while (count < num_multicast_groups)
        {
            member_index = next_registered_multicast_slot(member_index);

            if (multicast_info.multicast_member_list[member_index].socket == ctx)
            {
                convert_secure_socket_to_nxd_ip_addr(&if_addr, &multicast_info.multicast_member_list[member_index].if_addr);
                convert_secure_socket_to_nxd_ip_addr(&multi_addr, &multicast_info.multicast_member_list[member_index].multi_addr);

#ifndef NX_DISABLE_IPV4
                if (if_addr.nxd_ip_version == NX_IP_VERSION_V4)
                {
                    err = nx_igmp_multicast_leave(ctx->nxd_socket.udp->nx_udp_socket_ip_ptr, multi_addr.nxd_ip_address.v4);
                    if (err != NX_SUCCESS)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_igmp_multicast_leave failed with error 0x%02x\n", err);
                    }
                }
#endif
#ifndef NX_DISABLE_IPV6
                if (if_addr.nxd_ip_version == NX_IP_VERSION_V6)
                {
                    err = nxd_ipv6_multicast_interface_leave(ctx->nxd_socket.udp->nx_udp_socket_ip_ptr, &multi_addr, CY_NETWORK_PRIMARY_INTERFACE);
                    if (err != NX_SUCCESS)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nxd_ipv6_multicast_interface_leave failed with error 0x%02x\n", err);
                        break;
                    }
                }
#endif
                clear_multicast_slot_status_bit(member_index);
                multicast_info.multicast_member_count--;
            }
            member_index++;
            count++;
        }

        cy_rtos_set_mutex(&multicast_join_leave_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);

        /* Check if socket is still bound */
        if (ctx->nxd_socket.udp->nx_udp_socket_bound_next != NULL)
        {
            nx_udp_socket_unbind(ctx->nxd_socket.udp);
        }

        nx_udp_socket_delete(ctx->nxd_socket.udp);
    }
    /* free the socket context */
    free_socket(ctx);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_delete End\n");

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_deinit(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_deinit Start\n");
    if (!init_ref_count)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "library not initialized\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_INITIALIZED;
    }

    init_ref_count--;
    if (!init_ref_count)
    {
        result = cy_tls_deinit();
        if (result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_deinit failed\n");
        }
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
        memset(socket_list, 0, sizeof(socket_list));
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        cy_rtos_deinit_mutex(&socket_list_mutex);
        cy_rtos_deinit_mutex(&accept_recv_event_mutex);

        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&multicast_join_leave_mutex, CY_RTOS_NEVER_TIMEOUT);
        if (multicast_info.multicast_member_list)
        {
            free(multicast_info.multicast_member_list);
            multicast_info.multicast_member_list = NULL;
        }

        multicast_info.multicast_member_status = 0;
        multicast_info.multicast_member_count  = 0;

        cy_rtos_set_mutex(&multicast_join_leave_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);

        cy_rtos_deinit_mutex(&multicast_join_leave_mutex);

        cy_worker_thread_delete(&socket_worker);
    }
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_deinit End\n");

    return result;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_bind(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t address_length)
{
    cy_socket_ctx_t * ctx;
    UINT result = NX_SUCCESS;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_bind Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if (address == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Address passed is NULL\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    /*
     * Make a note of the port used for bind. In NetXDuo, only client TCP sockets perform a
     * bind operation. Server sockets pass in the port number in the listen call and must not
     * be already bound to a port. Since we have no way of knowing whether a TCP socket will
     * be used as a server socket at this point we'll treat it as a client socket and retain
     * the port in case we have to change later on.
     */

    ctx->bind_port = address->port;
    if (ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        result = nx_udp_socket_bind(ctx->nxd_socket.udp, address->port, NX_TIMEOUT(DEFAULT_BIND_TIMEOUT));
    }
    else
    {
        result = nx_tcp_client_socket_bind(ctx->nxd_socket.tcp, address->port, NX_TIMEOUT(DEFAULT_BIND_TIMEOUT));
    }

    if (result != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "socket_bind failed with error = 0x%02x\n", result);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return NXD_TO_CY_SECURE_SOCKETS_ERR(result);
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_bind End\n");
    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_listen(cy_socket_t handle, int backlog)
{
    cy_socket_ctx_t *ctx;
    UINT result = NX_SUCCESS;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_listen Start\n");
    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *)handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    /* Check if this socket is in listening state or not */
    if ((ctx->status & SOCKET_STATUS_FLAG_LISTENING))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "Server socket is already in listening state\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_SUCCESS;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    /* Check if socket is already bound to a local port */
    if (ctx->nxd_socket.tcp->nx_tcp_socket_bound_next)
    {
        nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
    }
    result = nx_tcp_server_socket_listen(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->bind_port, ctx->nxd_socket.tcp, backlog, cy_tcp_connect_callback);
    if (result != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_tcp_server_socket_listen failed with error = 0x%02x\n", result);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return NXD_TO_CY_SECURE_SOCKETS_ERR(result);
    }

    ctx->role = CY_TLS_ENDPOINT_SERVER;
    ctx->status |= SOCKET_STATUS_FLAG_LISTENING;

    /* If user has not set the authentication mode, set the default authentication mode to NONE. */
    if (!ctx->is_authmode_set)
    {
        ctx->auth_mode = CY_SOCKET_TLS_VERIFY_NONE;
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_listen End\n");

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_accept(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t *address_length, cy_socket_t *socket)
{
    cy_socket_ctx_t *ctx;
    cy_socket_ctx_t *accept_ctx;
    NX_TCP_SOCKET *tcp;
    cy_tls_params_t tls_params = { 0 };
    UINT result = NX_SUCCESS;
    cy_rslt_t ret;
    NXD_ADDRESS peer_addr;
    ULONG port;
    ULONG timeout;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_accept Start\n");
    if (socket == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Accepted client socket pointer is NULL\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    *socket = CY_SOCKET_INVALID_HANDLE;

    if (CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid server handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if (!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid server handle\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if (address == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Address passed as NULL\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    /* Check if this socket is in listening state or not */
    if (!(ctx->status &= SOCKET_STATUS_FLAG_LISTENING))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Server socket is not in listening state\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_LISTENING;
    }

    accept_ctx = alloc_socket(ctx->transport_protocol, ctx->iface_type);
    if (accept_ctx == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to allocate socket context for accepted client socket\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    /* Create the new socket */
    result = nx_tcp_socket_create(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, accept_ctx->nxd_socket.tcp, (CHAR*)"",
                                  NX_IP_NORMAL, FRAGMENT_OPTION, NX_IP_TIME_TO_LIVE, DEFAULT_TCP_WINDOW_SIZE, NX_NULL, cy_tcp_disconnect_callback);
    if (result != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to create socket for accepted client socket: 0x%02x\n", result);

        free_socket(accept_ctx);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    timeout = ctx->nonblocking ? NX_NO_WAIT : NX_TIMEOUT(ctx->recv_timeout);

    /* If socket is nonblocking, ignore timeout */
    if (ctx->nonblocking)
    {
        result = nx_tcp_server_socket_accept(ctx->nxd_socket.tcp, NX_NO_WAIT);
    }
    else
    {
        /* If socket is blocking, and user set the timeout use the timeout set by user else wait for ever. */
        timeout = ctx->is_recvtimeout_set ? NX_TIMEOUT(ctx->recv_timeout) : NX_WAIT_FOREVER;

        result = nx_tcp_server_socket_accept(ctx->nxd_socket.tcp, timeout);
    }

    if (result != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_tcp_server_socket_accept failed with error = 0x%02x\n", result);

        /* Cleanup from the failure */
        nx_tcp_server_socket_unaccept(ctx->nxd_socket.tcp);

        /* Set the socket back to listening */
        nx_tcp_server_socket_relisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->bind_port, ctx->nxd_socket.tcp);

        nx_tcp_socket_delete(accept_ctx->nxd_socket.tcp);
        free_socket(accept_ctx);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return NXD_TO_CY_SECURE_SOCKETS_ERR(result);
    }

    /*
     * Get the peer socket information.
     */
    result = nxd_tcp_socket_peer_info_get(ctx->nxd_socket.tcp, &peer_addr, &port);
    if (result != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "err getting peer info: 0x%02x\n", result);
    }

    /* Convert IP format from NetXDuo to secure socket */
    result = convert_nxd_to_secure_socket_ip_addr(&address->ip_address, &peer_addr);
    if (result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from NetXDuo to secure socket\n");
        nx_tcp_server_socket_unaccept(ctx->nxd_socket.tcp);
        nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
        /* Set the socket back to listening */
        nx_tcp_server_socket_relisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->bind_port, ctx->nxd_socket.tcp);
        nx_tcp_socket_delete(accept_ctx->nxd_socket.tcp);
        free_socket(accept_ctx);
        *socket = CY_SOCKET_INVALID_HANDLE;

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    address->port   = port;
    *address_length = sizeof(*address);

    if (ctx->enforce_tls)
    {
        tls_params.context = &ctx->nxd_socket.tcp;
        tls_params.network_send = tls_network_send_callback;
        tls_params.network_recv = tls_network_receive_callback;
        tls_params.tls_identity = ctx->tls_identity;
        tls_params.auth_mode = ctx->auth_mode;

        accept_ctx->enforce_tls = 1;

        ret = cy_tls_create_context(&ctx->tls_ctx, &tls_params);
        if (ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_create_context failed with error %ld\n", ret);
            nx_tcp_server_socket_unaccept(ctx->nxd_socket.tcp);
            nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
            /* Set the socket back to listening */
            nx_tcp_server_socket_relisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->bind_port, ctx->nxd_socket.tcp);
            nx_tcp_socket_delete(accept_ctx->nxd_socket.tcp);
            free_socket(accept_ctx);
            *socket = CY_SOCKET_INVALID_HANDLE;

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }

        ret = cy_tls_connect(ctx->tls_ctx, CY_TLS_ENDPOINT_SERVER, ctx->send_timeout);
        if (ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_connect failed with error %lu\n", ret);
            cy_tls_delete_context(ctx->tls_ctx);
            nx_tcp_server_socket_unaccept(ctx->nxd_socket.tcp);
            nx_tcp_client_socket_unbind(ctx->nxd_socket.tcp);
            /* Set the socket back to listening */
            nx_tcp_server_socket_relisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->bind_port, ctx->nxd_socket.tcp);
            nx_tcp_socket_delete(accept_ctx->nxd_socket.tcp);
            free_socket(accept_ctx);
            *socket = CY_SOCKET_INVALID_HANDLE;

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
        accept_ctx->status |= SOCKET_STATUS_FLAG_SECURED;
        accept_ctx->tls_ctx = ctx->tls_ctx;
    }

    /*
     * Slight of hand. We want to swap the underlying sockets so that the connected socket is attached to
     * the newly created accept_ctx.
     */

    tcp = ctx->nxd_socket.tcp;
    ctx->nxd_socket.tcp = accept_ctx->nxd_socket.tcp;
    accept_ctx->nxd_socket.tcp = tcp;

    /* Set the newly created socket (not the one being returned) to listening on the port */
    result = nx_tcp_server_socket_relisten(ctx->nxd_socket.tcp->nx_tcp_socket_ip_ptr, ctx->bind_port, ctx->nxd_socket.tcp);
    if (result != NX_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "nx_tcp_server_socket_relisten failed with error = 0x%02x\n", result);
    }

    /* Clear Listening state in connected client socket and set the newly created socket to be in Listening state */
    accept_ctx->status &= ~(SOCKET_STATUS_FLAG_LISTENING);
    ctx->status |= SOCKET_STATUS_FLAG_LISTENING;

    ctx->nxd_socket.tcp->nx_tcp_socket_reserved_ptr = (void *)ctx->id;
    nx_tcp_socket_receive_notify(ctx->nxd_socket.tcp, cy_tcp_receive_callback);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "new connection accepted\n");

    accept_ctx->nxd_socket.tcp->nx_tcp_socket_reserved_ptr = (void *)accept_ctx->id;
    accept_ctx->status |= SOCKET_STATUS_FLAG_CONNECTED;
    accept_ctx->nonblocking          = ctx->nonblocking;
    accept_ctx->recv_timeout         = ctx->recv_timeout;
    accept_ctx->send_timeout         = ctx->send_timeout;
    nx_tcp_socket_receive_notify(accept_ctx->nxd_socket.tcp, cy_tcp_receive_callback);

    /*
     * Ideally, the existing callbacks should not be copied. Typically the callers have 
     * added a callback argument for the callback event. If we copy the callback
     * info to the new socket, the callback argument will be same as that passed to the
     * the listening socket, instead of it being unique to the connected client socket.
     * This copying of callbacks is done here to keep the behavior uniform between LwIP and NetXDuo.
     * The below callbacks are just the default callbacks for the new client socket. They
     * can be overriden by any other callback and argument by the application.
    */

    accept_ctx->callbacks.disconnect = ctx->callbacks.disconnect;
    accept_ctx->callbacks.receive    = ctx->callbacks.receive;

    *socket = accept_ctx;

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_accept End\n");
    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_shutdown(cy_socket_t handle, int how)
{
    (void)handle;
    (void)how;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_shutdown call not suppoert for NetXDuo\n");
    return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
}
