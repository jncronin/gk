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
 *  Implementation of Secure Sockets Interface for LwIP.
 *
 */

#include "cy_secure_sockets.h"
#include "cy_tls.h"
#include "cyabs_rtos.h"
#include "cy_worker_thread.h"
#include "cyabs_rtos_impl.h"
#include "cy_network_mw_core.h"
#include "cy_log.h"
#include <lwip/api.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/etharp.h>
#include <ethernet.h>
#include <string.h>

#if LWIP_IPV4 && LWIP_IGMP
#include <lwip/igmp.h>
#endif

#if LWIP_IPV6_MLD && LWIP_IPV6
#include <lwip/mld6.h>
#endif

typedef struct cy_socket_ctx cy_socket_ctx_t;

struct cy_socket_ctx
{
    uint32_t                   socket_magic_header;            /**< Socket context magic header to verify the context pointer */
    struct netconn*            conn_handler;                   /**< netconn handler returned with netconn_new */
    cy_mutex_t                 netconn_mutex;                  /**<* Serializes netconn API calls for a socket */
    cy_mutex_t                 socket_mutex;                   /**<* Protects accessing the socket context */
    struct
    {
        cy_socket_opt_callback_t connect_request;
        cy_socket_opt_callback_t receive;
        cy_socket_opt_callback_t disconnect;
    } callbacks;                                               /**<* Socket callback functions */
    void*                      tls_ctx;                        /**< tls context of underlying security stack */
    const void*                tls_identity;                   /**< contains certificate/key pair */
    char*                      rootca_certificate;             /**< RootCA certificate specific to the socket */
    int                        rootca_certificate_len;         /**< Length of ca_cert */
    bool                       enforce_tls;                    /**< Enforce TLS connection */
    int                        auth_mode;                      /**< TLS authentication mode */
    unsigned char              mfl_code;                       /**< TLS maximum fragment length */
    char*                      alpn;                           /**< ALPN string */
    char**                     alpn_list;                      /**< ALPN array of strings to be passed to mbedtls */
    uint32_t                   alpn_count;                     /**< Number of protocols in ALPN list */
    char*                      hostname;                       /**< Server hostname used with SNI extension */
    uint32_t                   status;                         /**< socket status */
    struct pbuf*               buf;                            /**< Receive data \c pbuf structure */
    u16_t                      offset;                         /**< Receive data \c pbuf  offset */
    int                        id;                             /**< Socket id used in mapping from netconn to cy socket */
    int                        send_events;                    /**< Send events received from LwIP */
    int                        recv_events_cache;              /**< Holds the number receive events occurred prior to receive callback is registered. */
    bool                       disconnect_event_cache;         /**< Used to check if disconnect event occurred prior to disconnect callback is registered. */
    int                        role;                           /**< Used for identifying if the socket is server or client */
    int                        transport_protocol;             /**< Used for identifying transport protocol TCP, TLS or UDP */
#if LWIP_SO_RCVTIMEO
    bool                       is_recvtimeout_set;             /**< Used to check whether receive timeout is set by the application */
#endif

#if LWIP_SO_SNDTIMEO
    bool                       is_sendtimeout_set;             /**< Used to check whether send timeout is set by the application */
#endif
    bool                       is_authmode_set;                /**< Used to check whether TLS authentication mode is set by the application */
    cy_socket_interface_t      iface_type;                     /**< Network interface type to be used with the socket */
    uint8_t                    iface_idx;                      /**< Index of the network interface. Possible values are 0 and 1 */
#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT
    bool                       load_rootca_from_ram;           /**< Flag for setting the RootCA certificate location.  */
    bool                       load_device_cert_key_from_ram;  /**< Flag for setting the device cert & key location. */
#endif
    uint32_t                   data_received;                  /**< Used to keep track of data read by underlying security stack */
    uint32_t                   socket_magic_footer;            /**< Socket context magic footer to verify the context pointer */
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

/*
 * Following macros are used to find disconnect and receive events occurred before the socket context is created for accepted client sockets.
 */
#define SECURE_SOCKETS_DISCONNET_EVENT_BIT_MASK (1 << 30)
#define SECURE_SOCKETS_RECEIVE_EVENT_BIT_MASK   (1 << 31)

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

#define NUM_SOCKETS                         MEMP_NUM_NETCONN

#define SECURE_SOCKETS_MAGIC_HEADER         0xbaefdcbd
#define SECURE_SOCKETS_MAGIC_FOOTER         0xefbcabfe

/* Maximum number of multicast groups supported. */
#define SECURE_SOCKETS_MAX_MULTICAST_GROUPS 10

#define UNUSED_ARG(arg)                     (void)(arg)

#define LWIP_TO_CY_SECURE_SOCKETS_ERR( lwip_err )   ( lwip_to_secure_socket_error(lwip_err) )
#define TLS_TO_CY_SECURE_SOCKETS_ERR( tls_err )     ( tls_to_secure_socket_error(tls_err) )

#ifdef ENABLE_SECURE_SOCKETS_LOGS
#define ss_cy_log_msg cy_log_msg
#else
#define ss_cy_log_msg(a,b,c,...)
#endif

typedef struct cy_lwip_sock
{
    int used;
    cy_socket_ctx_t *ctx;
}cy_lwip_sock_t;

/** mutex to protect socket array */
static cy_mutex_t socket_list_mutex;

static cy_mutex_t netconn_recvfrom_mutex;

/* mutex to protect the counter maintained for receive events occurred
 * on an accepted socket, prior to socket context is created for it.
 */
static cy_mutex_t accept_recv_event_mutex;
static cy_worker_thread_info_t socket_worker;

/* Socket library usage count */
static int init_ref_count = 0;

/** The global array of available sockets */
static cy_lwip_sock_t socket_list[NUM_SOCKETS];

/* Function used to get bytes available after decryption from underlying security stack */
extern uint32_t cy_tls_get_bytes_avail(void *context);

static bool is_socket_valid(cy_socket_ctx_t* socket)
{
    if((socket->socket_magic_header == SECURE_SOCKETS_MAGIC_HEADER) && (socket->socket_magic_footer == SECURE_SOCKETS_MAGIC_FOOTER))
    {
        return true;
    }
    return false;
}
static cy_rslt_t convert_lwip_to_secure_socket_ip_addr(cy_socket_ip_address_t *dest, const ip_addr_t *src)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    memset(dest, 0, sizeof(cy_socket_ip_address_t));

/* If both LWIP_IPV6 and LWIP_IPV4 are enabled, then ip_addr_t has its own structure definition.
 * If only LWIP_IPV4 is enabled ip_addr_t is a typedef to ip4_addr_t, if only LWIP_IPV6 is enabled
 * ip_addr_t is a typedef to ip6_addr_t.
 */
#if LWIP_IPV6 && LWIP_IPV4
    if(src->type == IPADDR_TYPE_V4)
    {
        dest->ip.v4 = src->u_addr.ip4.addr;
        dest->version = CY_SOCKET_IP_VER_V4;
    }
    else if(src->type == IPADDR_TYPE_V6)
    {
        dest->version= CY_SOCKET_IP_VER_V6;
        dest->ip.v6[0] = src->u_addr.ip6.addr[0];
        dest->ip.v6[1] = src->u_addr.ip6.addr[1];
        dest->ip.v6[2] = src->u_addr.ip6.addr[2];
        dest->ip.v6[3] = src->u_addr.ip6.addr[3];
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid IP version type \r\n");
        result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#else

#if LWIP_IPV4
    dest->ip.v4 = src->addr;
    dest->version = CY_SOCKET_IP_VER_V4;
#elif LWIP_IPV6
    dest->ip.v6[0] = src->addr[0];
    dest->ip.v6[1] = src->addr[1];
    dest->ip.v6[2] = src->addr[2];
    dest->ip.v6[3] = src->addr[3];
    dest->version= CY_SOCKET_IP_VER_V6;
#else
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "LWIP_IPV6 and LWIP_IPV4 both are disabled \r\n");
    result = CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION;
#endif

#endif

    return result;
}

static cy_rslt_t convert_secure_socket_to_lwip_ip_addr(ip_addr_t *dest, const cy_socket_ip_address_t *src)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    memset(dest, 0, sizeof(ip_addr_t));

#if LWIP_IPV6 && LWIP_IPV4
    if(src->version == CY_SOCKET_IP_VER_V4)
    {
        ip_addr_set_ip4_u32(dest, src->ip.v4);
        dest->type = IPADDR_TYPE_V4;
    }
    else
    {
        dest->u_addr.ip6.addr[0] = src->ip.v6[0];
        dest->u_addr.ip6.addr[1] = src->ip.v6[1];
        dest->u_addr.ip6.addr[2] = src->ip.v6[2];
        dest->u_addr.ip6.addr[3] = src->ip.v6[3];
        dest->type = IPADDR_TYPE_V6;
    }
#else

#if LWIP_IPV4
    if(src->version == CY_SOCKET_IP_VER_V4)
    {
        ip_addr_set_ip4_u32(dest, src->ip.v4);
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "not an IPv4 address \r\n");
        result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#elif LWIP_IPV6
    if(src->version == CY_SOCKET_IP_VER_V6)
    {
        dest->addr[0] = src->ip.v6[0];
        dest->addr[1] = src->ip.v6[1];
        dest->addr[2] = src->ip.v6[2];
        dest->addr[3] = src->ip.v6[3];
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "not an IPv6 address \r\n");
        result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#else
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "LWIP_IPV6 and LWIP_IPV4 both are disabled \r\n");
    result = CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION;
#endif /* LWIP_IPV6 */

#endif /* LWIP_IPV6 && LWIP_IPV4 */
    return result;
}

/* Find the index of the next registered slot in the multicast list. Caller should ensure to call
 * this helper function only when at-least one member is registered, else the while will become indefinite.
 * This function should be called under a mutex lock. */
static uint32_t next_registered_multicast_slot(uint32_t index)
{
    while(!(multicast_info.multicast_member_status & (0x0001 << index)))
    {
        index++;
    }
    return index;
}

/* Find the index of the next free slot in the multicast list. Ensure to call this function only when the
 * total registered count is not reached the maximum. This function should be called under a mutex lock. */
static uint32_t next_free_multicast_slot(uint32_t index)
{
    while(multicast_info.multicast_member_status & (0x0001 << index))
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

    while(count < multicast_info.multicast_member_count)
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

static cy_socket_ctx_t* alloc_socket()
{
    int i;
    cy_rslt_t result;

    /* allocate a new socket identifier */
    for(i = 0; i < NUM_SOCKETS; ++i)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
        if(!socket_list[i].ctx)
        {
            if(socket_list[i].used)
            {
                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
                continue;
            }
            socket_list[i].ctx = malloc(sizeof(cy_socket_ctx_t));
            if( socket_list[i].ctx == NULL)
            {
                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed %s %d\r\n", __FILE__, __LINE__);
                return NULL;
            }

            memset(socket_list[i].ctx, 0, sizeof(cy_socket_ctx_t));

            result = cy_rtos_init_mutex(&socket_list[i].ctx->netconn_mutex);
            if(CY_RSLT_SUCCESS != result)
            {
                free(socket_list[i].ctx);
                socket_list[i].ctx = NULL;

                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_rtos_init_mutex failed at file: %s line: %d error code: %ld\r\n", __FILE__, __LINE__, result);
                return NULL;
            }

            result = cy_rtos_init_mutex(&socket_list[i].ctx->socket_mutex);
            if(CY_RSLT_SUCCESS != result)
            {
                cy_rtos_deinit_mutex(&socket_list[i].ctx->netconn_mutex);

                free(socket_list[i].ctx);
                socket_list[i].ctx = NULL;

                cy_rtos_set_mutex(&socket_list_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_rtos_init_mutex failed at file: %s line: %d error code: %ld\r\n", __FILE__, __LINE__, result);
                return NULL;
            }

            socket_list[i].used = 1;
            socket_list[i].ctx->id = i;
#if defined(CYBSP_ETHERNET_CAPABLE)
            socket_list[i].ctx->iface_type = CY_SOCKET_ETH1_INTERFACE;
            socket_list[i].ctx->iface_idx = 1;
#else
            /* Default interface for Wi-Fi will be STA interface */
            socket_list[i].ctx->iface_type = CY_SOCKET_STA_INTERFACE;
            socket_list[i].ctx->iface_idx = 0;
#endif
            socket_list[i].ctx->socket_magic_header = SECURE_SOCKETS_MAGIC_HEADER;
            socket_list[i].ctx->socket_magic_footer = SECURE_SOCKETS_MAGIC_FOOTER;

            cy_rtos_set_mutex(&socket_list_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
            return socket_list[i].ctx;
        }
        else
        {
            cy_rtos_set_mutex(&socket_list_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
        }
    }
    return NULL;
}

static void free_socket(cy_socket_ctx_t* socket)
{
    int id;

    if(socket == NULL)
    {
        return;
    }

    id = socket->id;
    if(id >= 0 && id < NUM_SOCKETS)
    {
        /* update the sockets array */
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
        cy_rtos_deinit_mutex(&socket->netconn_mutex);
        cy_rtos_deinit_mutex(&socket->socket_mutex);
        socket_list[id].used = 0;
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
    switch(max_fragment_length)
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
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid maximum fragment length \r\n");
        }
    }
    return mfl;
}

static uint32_t mfl_code_to_max_fragment_length(unsigned char mfl_code)
{
    uint32_t max_fragment_length=0;;

    switch(mfl_code)
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
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid mflcode %d \r\n", mfl_code);
        }
    }
    return max_fragment_length;
}

static cy_rslt_t lwip_to_secure_socket_error(err_t error)
{
    switch(error)
    {
        case ERR_OK:
            return CY_RSLT_SUCCESS;

        case ERR_CLSD:
            return CY_RSLT_MODULE_SECURE_SOCKETS_CLOSED;

        case ERR_MEM:
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;

        case ERR_TIMEOUT:
            return CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;

        case ERR_INPROGRESS:
        case ERR_ALREADY:
            return CY_RSLT_MODULE_SECURE_SOCKETS_IN_PROGRESS;

        case ERR_ISCONN:
            return CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED;

        case ERR_CONN:
        case ERR_ABRT:
        case ERR_RST:
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;

        case ERR_ARG:
        case ERR_VAL:
            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;

        case ERR_USE:
            return CY_RSLT_MODULE_SECURE_SOCKETS_ADDRESS_IN_USE;

        case ERR_RTE:
            return CY_RSLT_MODULE_SECURE_SOCKETS_ERROR_ROUTING;

        case ERR_WOULDBLOCK:
            return CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK;

        case ERR_BUF:
        case ERR_IF:
        default:
            return CY_RSLT_MODULE_SECURE_SOCKETS_TCPIP_ERROR;
    }
}

static cy_rslt_t tls_to_secure_socket_error(cy_rslt_t error)
{
    switch(error)
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
    switch(error)
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
#if LWIP_IPV4
/* This function checks if the MAC address is present in ARP cache table for the given IP. If not, it triggers
 * ARP request and wait for the MAC address to be resolved for the given IP address. If it's resolved within
 * the timeout value it returns success and cy_socket_sendto function sends the datagram packet. If not, the function returns
 * error and cy_socket_sendto function also returns error to the caller.
 *
 * NOTE: The LwIP stack drops the datagram packet if the entry doesn't exist in ARP Cache table for the given IP address.
 */
/*-----------------------------------------------------------*/
static cy_rslt_t eth_arp_resolve(ip4_addr_t *dest_addr, cy_socket_interface_t iface_type)
{
    ssize_t arp_index = -1;
    struct netif *netif;
    const ip4_addr_t *ipv4addr;
    struct eth_addr *eth_ret = NULL;
    const ip4_addr_t *ip_ret = NULL;
    int32_t arp_waittime = ARP_WAIT_TIME_IN_MSEC;
    err_t err;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "eth_arp_resolve Start\r\n");

    if(iface_type != CY_SOCKET_STA_INTERFACE  && iface_type != CY_SOCKET_AP_INTERFACE && iface_type != CY_SOCKET_ETH0_INTERFACE && iface_type != CY_SOCKET_ETH1_INTERFACE)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid interface type \r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(iface_type == CY_SOCKET_STA_INTERFACE)
    {
        netif = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);
    }
    else if(iface_type == CY_SOCKET_AP_INTERFACE)
    {
        netif = cy_network_get_nw_interface(CY_NETWORK_WIFI_AP_INTERFACE, 0);
    }
    else if(iface_type == CY_SOCKET_ETH0_INTERFACE)
    {
        netif = cy_network_get_nw_interface(CY_NETWORK_ETH_INTERFACE, 0);
    }
    else
    {
        netif = cy_network_get_nw_interface(CY_NETWORK_ETH_INTERFACE, 1);
    }

    ipv4addr = dest_addr;
    if(netif == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_network_get_nw_interface returned NULL \r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST;
    }

    /* No need to resolve the ARP for broadcasts and multicasts addresses.  */
    if( (ip4_addr_isbroadcast(ipv4addr, netif)) || (ip4_addr_ismulticast(ipv4addr)) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "eth_arp_resolve broadcast or multicast packet\r\n");
        return CY_RSLT_SUCCESS;
    }

    /* Check if the destination address is outside subnet. */
    if(!ip4_addr_netcmp(ipv4addr, netif_ip4_addr(netif), netif_ip4_netmask(netif)) && !ip4_addr_islinklocal(ipv4addr))
    {
        /* Check if default gateway address is available */
        if (!ip4_addr_isany_val(*netif_ip4_gw(netif)))
        {
            /* send to hardware address of default gateway IP address */
            ipv4addr = netif_ip4_gw(netif);
        }
        else
        {
            /* no default gateway available */
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "default gateway missing \r\n");
            return lwip_to_secure_socket_error(ERR_RTE);
        }
    }

    /* Check if entry for the address is already present in the ARP cache. */
    arp_index = etharp_find_addr(netif, ipv4addr, &eth_ret, &ip_ret);
    if(arp_index == -1)
    {
        /* Entry for the address is not present in the ARP cache. Sent ARP request.*/
        err = etharp_request(netif, ipv4addr);
        if(err != ERR_OK)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "etharp_request failed with error %d\n", err);
            return  LWIP_TO_CY_SECURE_SOCKETS_ERR(err);
        }

        do
        {
            arp_index = etharp_find_addr(netif, ipv4addr, &eth_ret, (const ip4_addr_t **) &ip_ret);
            if(arp_index != -1)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "arp entry found \r\n");
                break;
            }
            cy_rtos_delay_milliseconds(ARP_CACHE_CHECK_INTERVAL_IN_MSEC);
            arp_waittime -= ARP_CACHE_CHECK_INTERVAL_IN_MSEC;
            if(arp_waittime <= 0)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Could not resolve MAC address for the given destination address \r\n");
                return CY_RSLT_MODULE_SECURE_SOCKETS_ARP_TIMEOUT;
            }
        } while(1);
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "eth_arp_resolve End\r\n");
    return CY_RSLT_SUCCESS;
}
#endif

/*-----------------------------------------------------------*/
static void cy_process_receive_event_with_valid_context(cy_socket_ctx_t *ctx)
{
    int recv_avail = 0;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_receive_event_with_valid_context Start \r\n");

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        if(ctx->callbacks.receive.callback)
        {
            /* For UDP transport protocol, check  conn_handler->recv_avail to find if receive data is available. */
            SYS_ARCH_GET(ctx->conn_handler->recv_avail, recv_avail);

            if(recv_avail > 0)
            {
                ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
            }
        }
        else
        {
            /* Receive Callback is not registered yet. Increment the receive count so that it will processed as soon the receive callback is registered. */
            ctx->recv_events_cache++;
        }
    }
    else if( ( !ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED ) ||
             ( ( ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) )
    {
        /* If the transport protocol is TCP or TLS invoke application's receive callback function only if the connection is established.
         * The above check is needed for TLS connection so that we do not invoke app's receive callback for TLS handshake messages.
         */
        if(ctx->callbacks.receive.callback)
        {
            SYS_ARCH_GET(ctx->conn_handler->recv_avail, recv_avail);

            if(recv_avail > 0)
            {
                ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
            }
            else
            {
                /* LwIP receive event and conn->recv_avail are packet based, but secure socket APIs are stream based.
                 * If conn->recv_avail is zero, still there can be received data left in the pbuf that is stored in
                 * the socket context. In that case even though conn->rec_avail is zero application callback need be be invoked.
                 */
                struct pbuf *pbuf = NULL;
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
                cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);
                pbuf = ctx->buf;
                cy_rtos_set_mutex(&ctx->netconn_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);
                if(pbuf)
                {
                    ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
                }
                /* Problem: Currently, LWIP sends event to application/library for each packet received from the network, if data from the packet is not yet consumed. 
                 * In secure connection, when a TLS record is fragmented over multiple LWIP packets, if the first read call from application to security stack, reads all 
                 * the LWIP packets for that TLS record then data is consumed from the packet. Hence LWIP doesn't send any more event to application and due to which the 
                 * application doesnt receive full data.
                 *
                 * Solution: Added ctx->data_received to keep track of data read in security stack. If there are no more packets with LWIP, then check if the security stack
                 * has received/read some data. If either one of layer has the packet then send an event to application/library which will eventually read the data.
                 */
                else if( ctx->enforce_tls )
                {
                    if( ctx->data_received )
                    {
                        ctx->callbacks.receive.callback((cy_socket_t)ctx, ctx->callbacks.receive.arg);
                    }
                }
            }
        }
        else
        {
            /* Receive Callback is not registered yet. Increment the receive count so that it will processed as soon the receive callback is registered. */
            ctx->recv_events_cache++;
        }
    }
    cy_rtos_set_mutex(&accept_recv_event_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_receive_event_with_valid_context End \r\n");
}
/*-----------------------------------------------------------*/
static void cy_process_receive_event(void *arg)
{
    struct netconn *conn = (struct netconn *)arg;
    int id;
    cy_socket_ctx_t *ctx = NULL;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_receive_event Start \r\n");
    if(NETCONNTYPE_GROUP(netconn_type(conn)) == NETCONN_TCP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

        /*
         * 1. On netconn creation lwIP initializes conn->socket value to -1. cy_socket_accept function changes conn->socket value to socket ID once it creates context for the accepted client socket.
         * 2. To keep-track of receive events occurred prior to socket context is created, conn->socket variable's bit fields are utilized in following way.
         *
         *    +------------------------------+------+-------
              |bit-32| bit-31  |           bits1-30        |
              +------------------------------+------+-------

              bit  32 : This bit is set to one on the first receive event that occurred prior to socket context is created.
              bit  31 : This bit is set to on on the disconnect event occurred prior to socket context is created.
              bits 1-30 : Holds number of receive events occurred before socket context is created.
         */
        /* Check if socket context is created for the accepted client socket. */
        if(conn->socket == -1)
        {
            /* Set the receive event bit */
            conn->socket = SECURE_SOCKETS_RECEIVE_EVENT_BIT_MASK;

            /* Increment the receive event count. */
            conn->socket++;

            cy_rtos_set_mutex(&accept_recv_event_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

            return;
        }
        else if(conn->socket & SECURE_SOCKETS_RECEIVE_EVENT_BIT_MASK)
        {
            /* Increment the receive event count. */
            conn->socket++;

            cy_rtos_set_mutex(&accept_recv_event_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
            return;
        }
        else
        {
            /*
             * This else condition hits when conn->socket value is not -1 and receive event bit is not set, that means socket context is allocated for accepted client socket.
             * So we can get the socket id from conn->socket.
             */
            id = conn->socket;
        }

        cy_rtos_set_mutex(&accept_recv_event_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
    }
    else
    {
        id = conn->socket;
    }
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
    ctx = (cy_socket_ctx_t *)socket_list[id].ctx;

    if(ctx == NULL)
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "context is NULL in cy_process_receive_event \r\n");
        return;
    }

    cy_process_receive_event_with_valid_context(ctx);

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_receive_event End \r\n");
}

/*-----------------------------------------------------------*/
/*
 * This function processes the receive events occurred before socket context is created for the accepted client socket.
 */
static void cy_process_pending_receive_events(void *arg)
{
    cy_socket_recv_event_t *recv_event = (cy_socket_recv_event_t *)arg;
    cy_socket_ctx_t *ctx;
    uint32_t count;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_pending_receive_events Start\r\n");
    if(recv_event == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Argument to the worker thread callback function is NULL\r\n");
        return;
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);

    ctx = recv_event->socket;
    count = recv_event->recv_count;

    if(ctx == NULL || count == 0)
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_pending_receive_events End\r\n");
        free(recv_event);
        return;
    }

    /* Application might have deleted the socket while we are processing the pending receive events. So check if the socket is valid. */
    if(is_socket_valid(ctx))
    {
        if(ctx->callbacks.receive.callback == NULL)
        {
            cy_rtos_set_mutex(&socket_list_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

            /* This if condition should never be true, as this event is pushed to queue only after the receive callback is registered. */
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_pending_receive_events receive callback is not registered\r\n");
            free(recv_event);
            return;
        }

        while(count)
        {
            cy_process_receive_event_with_valid_context(ctx);
            count--;
        }
    }

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    free(recv_event);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_pending_receive_events End\r\n");
    return;
}

/*-----------------------------------------------------------*/
static void cy_process_connect_event(cy_socket_ctx_t *ctx)
{
    if(ctx == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "context is NULL in cy_process_connect_event \r\n");
        return;
    }

    if(ctx->callbacks.connect_request.callback)
    {
        ctx->callbacks.connect_request.callback((cy_socket_t)ctx, ctx->callbacks.connect_request.arg);
    }
}
/*-----------------------------------------------------------*/
static void cy_process_disconnect_event(cy_socket_ctx_t *ctx)
{
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_disconnect_event Start\r\n");
    if(ctx == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "context is NULL in cy_process_disconnect_event \r\n");
        return;
    }

    if(ctx->callbacks.disconnect.callback)
    {
        ctx->callbacks.disconnect.callback((cy_socket_t)ctx, ctx->callbacks.disconnect.arg);
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_disconnect_event End\r\n");
}
/*-----------------------------------------------------------*/
static void cy_process_connect_disconnect_notification_event(void *arg)
{
    struct netconn *conn = (struct netconn *)arg;
    cy_socket_ctx_t *socket_ctx;
    int socket_id;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_connect_disconnect_notification_event Start\r\n");

    cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

    socket_id = conn->socket;


    /*
     * 1. Connect event occurs on server socket and not on accepted client socket. So for connect event conn->socket value always have the valid socket ID.
     * 2. conn->socket == -1 means, disconnect event occurred before socket context is created for accepted client socket. This can happen if the remote client does connect and disconnect immediately.
     * 3. To keep track of the disconnect event and the receive events occcurred before socket context is created, conn->socket variable's bit fields are used in following way.
     *
     *        +------------------------------+------+-------
              |bit-32| bit-31  |           bits1-30        |
              +------------------------------+------+-------

              bit  32 : This bit is set to one on the first receive event that occurred prior to socket context is created.
              bit  31 : This bit is set to on on the disconnect event occurred prior to socket context is created.
              bits 1-30 : Holds number of receive events occurred before socket context is created.
     */

    /* Check if socket context is created for accepted client socket. */
    if(socket_id == -1)
    {
        conn->socket = SECURE_SOCKETS_DISCONNET_EVENT_BIT_MASK;

        cy_rtos_set_mutex(&accept_recv_event_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return;
    }
    /* Check if any receive events occurred before socket context is created for accepted client socket.*/
    else if(socket_id & SECURE_SOCKETS_RECEIVE_EVENT_BIT_MASK)
    {
        /*
         * Receive events occurred before socket context is created, so set the disconnect event bit without clearing receive event bit.
         */
        conn->socket |= SECURE_SOCKETS_DISCONNET_EVENT_BIT_MASK;

        cy_rtos_set_mutex(&accept_recv_event_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return;
    }

    cy_rtos_set_mutex(&accept_recv_event_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
    socket_ctx = (cy_socket_ctx_t *)socket_list[socket_id].ctx;

    /* By the time worker function is scheduled for disconnect event, application might have deleted the socket.
     * In that case socket_ctx will be NULL. So do NULL check for socket_ctx before accessing it.
     */
    if( socket_ctx == NULL )
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Callback function invoked on a deleted socket \r\n");
        return;
    }

    /* Connect event occurs on server socket's conn handler */
    if((socket_ctx->status & SOCKET_STATUS_FLAG_LISTENING) ==  SOCKET_STATUS_FLAG_LISTENING)
    {
        cy_process_connect_event(socket_ctx);
    }
    else if((socket_ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED)
    {
        /* Disconnect event occurs on client or accepted socket's conn handler */
        cy_process_disconnect_event(socket_ctx);
    }

    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_connect_disconnect_notification_event End\r\n");
}
/*-----------------------------------------------------------*/
static void cy_process_send_plus_event(void *arg)
{

    cy_socket_ctx_t *socket_ctx;
    int socket_id = (int)arg;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_send_plus_event Start\r\n");
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
    socket_ctx = (cy_socket_ctx_t *)socket_list[socket_id].ctx;
    if(socket_ctx == NULL)
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "context is NULL in cy_process_disconnect_event \r\n");
        return;
    }
    socket_ctx->send_events = 1;
    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_send_plus_event End\r\n");
}
/*-----------------------------------------------------------*/
static void cy_process_send_minus_event(void *arg)
{
    struct netconn *conn = (struct netconn *)arg;
    cy_socket_ctx_t *socket_ctx;
    int socket_id;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_send_minus_event Start\r\n");
    socket_id = conn->socket;
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
    socket_ctx = (cy_socket_ctx_t *)socket_list[socket_id].ctx;
    if(socket_ctx == NULL)
    {
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "context is NULL in cy_process_send_minus_event \r\n");
        return;
    }

    socket_ctx->send_events = 0;
    cy_rtos_set_mutex(&socket_list_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\r\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_process_send_minus_event End\r\n");
}
/*-----------------------------------------------------------*/
static void internal_netconn_event_callback(struct netconn *conn, enum netconn_evt event, u16_t length)
{
    cy_rslt_t result;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "internal_netconn_event_callback conn %p event %d length %d\r\n",conn, event, length);
    if(conn)
    {
        switch(event)
        {
            case NETCONN_EVT_RCVPLUS:
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NETCONN_EVT_RCVPLUS %s %d\r\n", __FILE__, __LINE__);
                if(length > 0)
                {
                    result = cy_worker_thread_enqueue(&socket_worker, cy_process_receive_event, (void *)conn);
                    if(result != CY_RSLT_SUCCESS)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
                    }
                }
                else
                {
                    /* NETCONN_EVT_RCVPLUS with length zero is  connect/disconnect notification */
                    result = cy_worker_thread_enqueue(&socket_worker, cy_process_connect_disconnect_notification_event, (void *)conn);
                    if(result != CY_RSLT_SUCCESS)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
                    }
                }
                break;

            case NETCONN_EVT_RCVMINUS:
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NETCONN_EVT_RCVMINUS %s %d\r\n", __FILE__, __LINE__);
                break;

            case NETCONN_EVT_SENDPLUS:
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NETCONN_EVT_SENDPLUS %s %d\r\n", __FILE__, __LINE__);

                /* NETCONN_EVT_SENDPLUS with length zero indicate connection deletion and connection established
                 * Non zero length indicates data has been sent to remote peer and received by it. */
                result = cy_worker_thread_enqueue(&socket_worker, cy_process_send_plus_event, (void *)conn->socket);
                if(result != CY_RSLT_SUCCESS)
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);

                }
                break;

            case NETCONN_EVT_SENDMINUS:
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "NETCONN_EVT_SENDMINUS %s %d\r\n", __FILE__, __LINE__);
                result = cy_worker_thread_enqueue(&socket_worker, cy_process_send_minus_event, (void *)conn);
                if(result != CY_RSLT_SUCCESS)
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
                }
                break;

            default:
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "unknown event %d\r\n", event);
                break;
        }
    }
    return;
}
/*-----------------------------------------------------------*/
/*
 * @brief Network send helper function. Function call to this function should be protected by netconn_mutex.
 */
static cy_rslt_t network_send(void *context, const unsigned char *data_buffer, uint32_t data_buffer_length, uint32_t *bytes_sent)
{
    err_t ret ;
    cy_socket_ctx_t *ctx = (cy_socket_ctx_t *)context;

    *bytes_sent = 0;

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    ret = netconn_write_partly(ctx->conn_handler, data_buffer, data_buffer_length, 0, (size_t *)bytes_sent);
    if(ret != ERR_OK)
    {
        *bytes_sent = 0;
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_write_partly failed with error %d\n", ret);
        return  LWIP_TO_CY_SECURE_SOCKETS_ERR(ret) ;
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

    result = network_send(context, data_buffer, data_buffer_length, bytes_sent);

    return secure_socket_to_tls_error(result);
}
/*-----------------------------------------------------------*/
/*
 * @brief Network receive helper function. Function call to this function should be protected by netconn_mutex.
 */
static cy_rslt_t network_receive(void *context, unsigned char *buffer, uint32_t len, uint32_t *bytes_received)
{
    err_t ret;
    u16_t received;
    size_t total_received = 0;
    size_t toread = 0;
    size_t outoffset = 0;
    struct pbuf *p;
    cy_socket_ctx_t *ctx = (cy_socket_ctx_t *)context;

    *bytes_received = 0;

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    do
    {
        if(!ctx->buf)
        {
            ret = netconn_recv_tcp_pbuf(ctx->conn_handler, &p);
            if(ret != ERR_OK)
            {
                /* If some amount of data already received, return success with amount of bytes received, else return error. */
                if(total_received)
                {
                    break;
                }
                else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_recv_tcp_pbuf returned %d\n", ret);
                    return LWIP_TO_CY_SECURE_SOCKETS_ERR(ret);
                }
            }

            if(p->tot_len == 0)
            {
                ctx->buf = NULL;
                continue;
            }
            ctx->buf = p;
            ctx->offset = 0;
        }

        /*
         * This is the data left to read
         */
        toread = len - total_received;
        if(toread > ctx->buf->tot_len - ctx->offset)
        {
            toread = ctx->buf->tot_len - ctx->offset;
        }

        /*
         * Copy the data out
         */
        received = pbuf_copy_partial(ctx->buf, buffer + outoffset, (u16_t)toread, ctx->offset);
        if( ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED )
        {
            ctx->data_received += received;
        }

        ctx->offset += received;

        /*
         * Keep track of the total received
         */
        total_received += received;

        /*
         * Move the output pointer for the output buffer
         */
        outoffset += received;

        /*
         * If we used up the current buffer, mark the context
         * as not having read data.  This will force another network
         * read the next time through the loop
         */
        if(ctx->offset >= ctx->buf->tot_len)
        {
            pbuf_free(ctx->buf);
            ctx->buf = 0;
        }

    }while(total_received < len);

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

    result = network_receive(context, buffer, len, bytes_received);

    return secure_socket_to_tls_error(result);
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_worker_thread_params_t params;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_init Start\r\n");

    if(!init_ref_count)
    {
        result = cy_rtos_init_mutex(&socket_list_mutex);
        if(CY_RSLT_SUCCESS != result)
        {
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        result = cy_rtos_init_mutex(&accept_recv_event_mutex);
        if(CY_RSLT_SUCCESS != result)
        {
            cy_rtos_deinit_mutex(&socket_list_mutex);
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        result = cy_rtos_init_mutex(&netconn_recvfrom_mutex);
        if(CY_RSLT_SUCCESS != result)
        {
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        memset(&multicast_info, 0 , sizeof(multicast_info));

        result = cy_rtos_init_mutex(&multicast_join_leave_mutex);
        if(CY_RSLT_SUCCESS != result)
        {
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&netconn_recvfrom_mutex);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        /* Allocate memory for multicast member list. */
        multicast_info.multicast_member_list = (cy_socket_multicast_pair_t *)malloc(sizeof(cy_socket_multicast_pair_t) * SECURE_SOCKETS_MAX_MULTICAST_GROUPS);
        if(multicast_info.multicast_member_list == NULL)
        {
            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for multicast member list\r\n");
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&netconn_recvfrom_mutex);
            cy_rtos_deinit_mutex(&multicast_join_leave_mutex);
            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        memset(&params, 0, sizeof(params));
        params.name = "Socket";
        params.priority = CY_RTOS_PRIORITY_ABOVENORMAL;
        params.stack = NULL;
        params.stack_size = SECURE_SOCKETS_THREAD_STACKSIZE;
        params.num_entries = 0;

        /* create a worker thread */
        result = cy_worker_thread_create(&socket_worker, &params);
        if(result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Create Worker Thread returned:0x%lx\r\n", result);
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&netconn_recvfrom_mutex);
            cy_rtos_deinit_mutex(&multicast_join_leave_mutex);
            if(multicast_info.multicast_member_list)
            {
                free(multicast_info.multicast_member_list);
                multicast_info.multicast_member_list = NULL;
            }
            return result;
        }
        /* Initialized the TLS library */
        result = cy_tls_init();
        if(result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Create Worker Thread returned:0x%lx\r\n", result);
            cy_rtos_deinit_mutex(&socket_list_mutex);
            cy_rtos_deinit_mutex(&accept_recv_event_mutex);
            cy_rtos_deinit_mutex(&netconn_recvfrom_mutex);
            cy_rtos_deinit_mutex(&multicast_join_leave_mutex);
            if(multicast_info.multicast_member_list)
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
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_init End\r\n");

    return result;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_create(int domain, int type, int protocol, cy_socket_t *handle)
{
    cy_socket_ctx_t *ctx = NULL;
    struct netconn  *conn = NULL;
    enum netconn_type conn_type = NETCONN_INVALID;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_create Start\r\n");

    if( handle == NULL )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "NULL handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#if LWIP_IPV4 && LWIP_IPV6
    if( (domain != CY_SOCKET_DOMAIN_AF_INET) && (domain != CY_SOCKET_DOMAIN_AF_INET6) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid domain\r\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#else
#if LWIP_IPV4
    if(domain != CY_SOCKET_DOMAIN_AF_INET)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid domain\r\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#elif LWIP_IPV6
    if(domain != CY_SOCKET_DOMAIN_AF_INET6)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid domain\r\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
#else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "LWIP_IPV6 and LWIP_IPV4 both are disabled\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BAD_NW_STACK_CONFIGURATION;
    }
#endif
#endif /* LWIP_IPV4 && LWIP_IPV6 */

    if( !( ( (type == CY_SOCKET_TYPE_STREAM) && (protocol == CY_SOCKET_IPPROTO_TCP || protocol == CY_SOCKET_IPPROTO_TLS) ) ||
           ( (type == CY_SOCKET_TYPE_DGRAM) && (protocol == CY_SOCKET_IPPROTO_UDP) ) ) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid type/protocol combination\r\n");
        *handle = CY_SOCKET_INVALID_HANDLE;
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = alloc_socket();
    if(ctx == NULL)
    {
        *handle = CY_SOCKET_INVALID_HANDLE;
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "alloc_socket failed \r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    ctx->transport_protocol = protocol;

    if(protocol == CY_SOCKET_IPPROTO_TCP || protocol == CY_SOCKET_IPPROTO_TLS)
    {
        if(domain == CY_SOCKET_DOMAIN_AF_INET)
        {
            conn_type = NETCONN_TCP;
        }
#if LWIP_IPV6
        else
        {
            conn_type = NETCONN_TCP_IPV6;
        }
#endif

        ctx->is_authmode_set = false;
        if(protocol == CY_SOCKET_IPPROTO_TLS)
        {
            ctx->enforce_tls = true;
            /* Set the default TLS authentication mode to verify required. */
            ctx->auth_mode = CY_SOCKET_TLS_VERIFY_REQUIRED;
        }
    }
    else if(protocol == CY_SOCKET_IPPROTO_UDP)
    {
        if(domain == CY_SOCKET_DOMAIN_AF_INET)
        {
            conn_type = NETCONN_UDP;
        }
#if LWIP_IPV6
        else
        {
            conn_type = NETCONN_UDP_IPV6;
        }
#endif
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    conn = netconn_new_with_callback(conn_type, internal_netconn_event_callback);
    if(conn == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_new_with_callback failed\r\n");
        free_socket(ctx);
        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    ctx->conn_handler = conn;
    ctx->conn_handler->socket = ctx->id;

#if LWIP_SO_RCVTIMEO
    ctx->is_recvtimeout_set = false;

    /**
     * Setting the receive timeout is necessary to avoid indefinite blocking while trying
     * to read more bytes than what was actually received.
     */
    netconn_set_recvtimeout(ctx->conn_handler, DEFAULT_RECV_TIMEOUT_IN_MSEC);
#endif

#if LWIP_SO_SNDTIMEO
    ctx->is_sendtimeout_set = false;

    /**
     * Setting the send timeout to default value.
     */
    netconn_set_sendtimeout(ctx->conn_handler, DEFAULT_SEND_TIMEOUT_IN_MSEC);
#endif

    ctx->data_received = 0;
    *handle = ctx;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_create End\r\n");
    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_setsockopt(cy_socket_t handle, int level, int optname, const void *optval, uint32_t optlen)
{
    cy_socket_ctx_t *ctx = NULL;
    cy_socket_opt_callback_t *callback_opt;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    if( ((level < CY_SOCKET_SOL_SOCKET) || (level > CY_SOCKET_SOL_IP)) ||
        ((optval == NULL) && (optname != CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK) && (optname != CY_SOCKET_SO_RECEIVE_CALLBACK) && (optname != CY_SOCKET_SO_DISCONNECT_CALLBACK)) ||
        ((optval != NULL) && (optlen == 0)) )
    {
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    switch(level)
    {
        case CY_SOCKET_SOL_TLS:
            /* All TLS socket options are used in TLS handshake. So don't allow setting these
             * socket options when socket is in connected state.
             */
            if(ctx->status & SOCKET_STATUS_FLAG_CONNECTED)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket connected\r\n");

                cy_rtos_set_mutex(&ctx->socket_mutex);
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                return CY_RSLT_MODULE_SECURE_SOCKETS_ALREADY_CONNECTED;
            }

            switch(optname)
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
                    if(ctx->hostname)
                    {
                        free(ctx->hostname);
                    }
                    ctx->hostname = (char *) malloc(optlen + 1);
                    if(NULL == ctx->hostname)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for hostname \n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }
                    memcpy(ctx->hostname, optval, optlen);
                    ctx->hostname[ optlen ] = '\0';
                    break;
                }

                case CY_SOCKET_SO_TLS_MFL:
                {
                    uint32_t mfl = *((uint32_t *)optval);
                    ctx->mfl_code = max_fragment_length_to_mfl_code(mfl);
                    if(SECURE_SOCKETS_MAX_FRAG_LEN_INVALID == ctx->mfl_code)
                    {
                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
                    break;
                }
                case CY_SOCKET_SO_TRUSTED_ROOTCA_CERTIFICATE:
                {
                    if(ctx->rootca_certificate)
                    {
                        /* free the previous configured root_ca if it exists */
                        free(ctx->rootca_certificate);
                    }

                    ctx->rootca_certificate = malloc(optlen + 1);
                    if(NULL == ctx->rootca_certificate)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for ca_cert \r\n");

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
                    char* ptr = NULL;
                    int count=0;
                    int i = 0;

                    ctx->alpn = malloc(optlen+1);
                    if(NULL == ctx->alpn)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for alpn \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }
                    memcpy(ctx->alpn, optval, optlen);

                    ctx->alpn[optlen] = '\0';

                    /* find the number of protcols in the alpn list */
                    ptr = (char *)optval;
                    while(*ptr != '\0')
                    {
                        if( *ptr == ',' )
                        {
                            count++;
                        }
                        ptr++;
                    }

                    ctx->alpn_count = count + 1;

                    /* mbedtls expects array of strings. Allocate memory for the array. */
                    ctx->alpn_list = (char **)malloc( (ctx->alpn_count + 1) * sizeof(char *));
                    if(NULL == ctx->alpn_list)
                    {
                        free(ctx->alpn);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "malloc failed for alpn_list \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                    }

                    /* Convert the input alpn string to array of strings */
                    ptr = ctx->alpn;
                    while( *ptr != '\0' )
                    {
                        ctx->alpn_list[i++] = (char*)ptr;

                        while( *ptr != ',' && *ptr != '\0' )
                        {
                            ptr++;
                            if( *ptr == ',' )
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

                    if(load_rootca_location != CY_SOCKET_ROOTCA_SECURE_STORAGE && load_rootca_location != CY_SOCKET_ROOTCA_RAM)
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

                    if(load_device_cert_key_location != CY_SOCKET_DEVICE_CERT_KEY_SECURE_STORAGE && load_device_cert_key_location != CY_SOCKET_DEVICE_CERT_KEY_RAM)
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
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\r\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
            break;

        case CY_SOCKET_SOL_SOCKET:
            switch(optname)
            {
                case CY_SOCKET_SO_RCVTIMEO:
#if LWIP_SO_RCVTIMEO
                {
                    uint32_t recv_timeout = *((uint32_t *)optval);
                    netconn_set_recvtimeout(ctx->conn_handler, recv_timeout);
                    ctx->is_recvtimeout_set = true;
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_SNDTIMEO:
#if LWIP_SO_SNDTIMEO
                {
                    uint32_t send_timeout = *((uint32_t *)optval);
                    netconn_set_sendtimeout(ctx->conn_handler, send_timeout);
                    ctx->is_sendtimeout_set = true;
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_NONBLOCK:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Non-block socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE:
                {
                    int keep_alive = *((int *)optval);

                    if(keep_alive ==0 || keep_alive ==1)
                    {
                        if(keep_alive)
                        {
                            ip_set_option(ctx->conn_handler->pcb.ip, SOF_KEEPALIVE);
                        }
                        else
                        {
                            ip_reset_option(ctx->conn_handler->pcb.ip, SOF_KEEPALIVE);
                        }
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid option value\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
                    break;
                }

                case CY_SOCKET_SO_BROADCAST:
#if IP_SOF_BROADCAST
                {
                    uint8_t broadcast = *((uint8_t *)optval);

                    if(broadcast == 0 || broadcast == 1)
                    {
                        if(broadcast)
                        {
                            ip_set_option(ctx->conn_handler->pcb.ip, SOF_BROADCAST);
                        }
                        else
                        {
                            ip_reset_option(ctx->conn_handler->pcb.ip, SOF_BROADCAST);
                        }
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid option value\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Incompatible Socket option\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_RECEIVE_CALLBACK:
                {
                    if(optval != NULL)
                    {
                        cy_socket_recv_event_t *pending_recv_events;
                        cy_rslt_t result = CY_RSLT_SUCCESS;

                        callback_opt = (cy_socket_opt_callback_t *) optval;

                        /*
                         * cy_process_receive_event function accesses ctx->callbacks.receive.callback, so we should protect initialization of
                         * ctx->callbacks.receive.callback pointer with mutex.
                         */
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
                        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

                        ctx->callbacks.receive.callback = callback_opt->callback;
                        ctx->callbacks.receive.arg = callback_opt->arg;

                        if(ctx->recv_events_cache)
                        {
                            /* Push an event to process the receive events occurred before the callback is registered.*/
                            pending_recv_events = calloc(1, sizeof(cy_socket_recv_event_t));
                            if(pending_recv_events == NULL)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "memory allocation failed for receive event structure\r\n");

                                cy_rtos_set_mutex(&accept_recv_event_mutex);
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);

                                cy_rtos_set_mutex(&ctx->socket_mutex);
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                                return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
                            }

                            pending_recv_events->recv_count = ctx->recv_events_cache;
                            pending_recv_events->socket = ctx;

                            /* Push event to worker thread to process all the pending receive events. */
                            result = cy_worker_thread_enqueue(&socket_worker, cy_process_pending_receive_events, (void *)pending_recv_events);
                            if(result != CY_RSLT_SUCCESS)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
                                free(pending_recv_events);
                            }

                            ctx->recv_events_cache = 0;
                        }
                        cy_rtos_set_mutex(&accept_recv_event_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
                        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

                        ctx->callbacks.receive.callback = NULL;

                        cy_rtos_set_mutex(&accept_recv_event_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);
                    }
                    break;
                }
                case CY_SOCKET_SO_DISCONNECT_CALLBACK:
                {
                    if(optval != NULL)
                    {
                        cy_rslt_t result = CY_RSLT_SUCCESS;

                        callback_opt = (cy_socket_opt_callback_t *) optval;
                        ctx->callbacks.disconnect.callback = callback_opt->callback;
                        ctx->callbacks.disconnect.arg = callback_opt->arg;

                        if(ctx->disconnect_event_cache)
                        {
                            result = cy_worker_thread_enqueue(&socket_worker, cy_process_connect_disconnect_notification_event, (void *)ctx->conn_handler);
                            if(result != CY_RSLT_SUCCESS)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
                            }
                        }
                        ctx->disconnect_event_cache = false;
                    }
                    else
                    {
                        ctx->callbacks.disconnect.callback = NULL;
                    }
                    break;
                }
                case CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK:
                {
                    if(optval != NULL)
                    {
                        callback_opt = (cy_socket_opt_callback_t *) optval;
                        ctx->callbacks.connect_request.callback = callback_opt->callback;
                        ctx->callbacks.connect_request.arg = callback_opt->arg;
                    }
                    else
                    {
                        ctx->callbacks.connect_request.callback = NULL;
                    }
                    break;
                }
                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\r\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }

                case CY_SOCKET_SO_BINDTODEVICE:
                {
                    struct netif *netif;
                    cy_socket_interface_t iface_type = ( *(cy_socket_interface_t *)optval);

                    if(iface_type == CY_SOCKET_STA_INTERFACE)
                    {
                        netif = cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0);
                    }
                    else if (iface_type == CY_SOCKET_AP_INTERFACE)
                    {
                        netif = cy_network_get_nw_interface(CY_NETWORK_WIFI_AP_INTERFACE, 0);
                    }
                    else if(iface_type == CY_SOCKET_ETH0_INTERFACE)
                    {
                        netif = cy_network_get_nw_interface(CY_NETWORK_ETH_INTERFACE, 0);
                    }
                    else if(iface_type == CY_SOCKET_ETH1_INTERFACE)
                    {
                        netif = cy_network_get_nw_interface(CY_NETWORK_ETH_INTERFACE, 1);
                    }
                    else
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid interface type \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }
                    if(netif == NULL)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_network_get_nw_interface returned NULL \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_NETIF_DOES_NOT_EXIST;
                    }

                    if(NETCONNTYPE_GROUP(netconn_type(ctx->conn_handler)) == NETCONN_TCP)
                    {
                        tcp_bind_netif(ctx->conn_handler->pcb.tcp, netif);
                    }
                    else if(NETCONNTYPE_GROUP(netconn_type(ctx->conn_handler)) == NETCONN_UDP)
                    {
                        udp_bind_netif(ctx->conn_handler->pcb.udp, netif);
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid socket error \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
                    }

                    ctx->iface_type = iface_type;
                    ctx->iface_idx = 0;

                    break;
                }
            }
            break;

        case CY_SOCKET_SOL_TCP:
            switch(optname)
            {
                case CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL:
#if LWIP_TCP_KEEPALIVE
                {
                    ctx->conn_handler->pcb.tcp->keep_intvl = (uint32_t)(*(const int *)optval);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt keep-alive interval %ld\r\n", ctx->conn_handler->pcb.tcp->keep_intvl);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_TCP_KEEPALIVE_COUNT:
#if LWIP_TCP_KEEPALIVE
                {
                    ctx->conn_handler->pcb.tcp->keep_cnt = (uint32_t)(*(const int *)optval);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt keep-alive count %ld\r\n", ctx->conn_handler->pcb.tcp->keep_cnt);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME:
#if LWIP_TCP_KEEPALIVE
                {
                    ctx->conn_handler->pcb.tcp->keep_idle = (uint32_t)(*(const int *)optval);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt keep-alive idle time %ld\r\n", ctx->conn_handler->pcb.tcp->keep_idle);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_TCP_NODELAY:
                {
                    uint8_t tcp_nodelay = *((uint8_t *)optval);

                    if(tcp_nodelay == 0 || tcp_nodelay == 1)
                    {
                        if(tcp_nodelay)
                        {
                            tcp_nagle_disable(ctx->conn_handler->pcb.tcp);
                        }
                        else
                        {
                            tcp_nagle_enable(ctx->conn_handler->pcb.tcp);
                        }
                    }
                    else
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid option value\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    break;
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
            switch(optname)
            {
                case CY_SOCKET_SO_JOIN_MULTICAST_GROUP:
                case CY_SOCKET_SO_LEAVE_MULTICAST_GROUP:
                {
                    ip_addr_t if_addr;
                    ip_addr_t multi_addr;
                    int member_index;
                    err_t err = ERR_OK;
                    cy_rslt_t result = CY_RSLT_SUCCESS;
                    const cy_socket_ip_mreq_t *imr = (const cy_socket_ip_mreq_t *)optval;

                    convert_secure_socket_to_lwip_ip_addr(&if_addr, &imr->if_addr);
                    convert_secure_socket_to_lwip_ip_addr(&multi_addr, &imr->multi_addr);

                    /* Check if the address to join/leave is a valid multiast address. */
                    if(!ip_addr_ismulticast(&multi_addr))
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid multicast address\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    do
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex locked %s %d\n", __FILE__, __LINE__);
                        cy_rtos_get_mutex(&multicast_join_leave_mutex, CY_RTOS_NEVER_TIMEOUT);

                        if(imr->if_addr.version != imr->multi_addr.version)
                        {
                            cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "multicast group address type and interface address type are not same\r\n");
                            result = CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                            break;
                        }

                        member_index = find_multicast_member_index(ctx, imr);

                        if(optname == CY_SOCKET_SO_JOIN_MULTICAST_GROUP)
                        {
                            if(member_index != -1)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);
                                result = CY_RSLT_MODULE_SECURE_SOCKETS_ADDRESS_IN_USE;
                                break;
                            }

                            if(multicast_info.multicast_member_count == SECURE_SOCKETS_MAX_MULTICAST_GROUPS)
                            {
                                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Registered multicast member count is already reached maximum allowed count %s %d\n", __FILE__, __LINE__);
                                result = CY_RSLT_MODULE_SECURE_SOCKETS_MAX_MEMBERSHIP_ERROR;
                                break;
                            }

                            member_index = next_free_multicast_slot(0);

                            /* Call wifi-mw-core network activity function to resume the network stack. */
                            cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

#if LWIP_IPV4
                            if(IP_IS_V4(&if_addr))
                            {
                                err = igmp_joingroup(ip_2_ip4(&if_addr), ip_2_ip4(&multi_addr));
                                if(err != ERR_OK)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "igmp_joingroup failed with error %d\r\n", err);
                                    break;
                                }
                            }
#endif
#if LWIP_IPV6
                            if(IP_IS_V6(&if_addr))
                            {
                                err = mld6_joingroup(ip_2_ip6(&if_addr), ip_2_ip6(&multi_addr));
                                if(err != ERR_OK)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "mld6_joingroup failed with error %d\r\n", err);
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
                            if(member_index == -1)
                            {
                                result = CY_RSLT_MODULE_SECURE_SOCKETS_MULTICAST_ADDRESS_NOT_REGISTERED;
                                break;
                            }

                            clear_multicast_slot_status_bit(member_index);
                            multicast_info.multicast_member_count--;

                            /* Call wifi-mw-core network activity function to resume the network stack. */
                            cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

#if LWIP_IPV4
                            if (IP_IS_V4(&if_addr))
                            {
                                err = igmp_leavegroup(ip_2_ip4(&if_addr), ip_2_ip4(&multi_addr));
                                if(err != ERR_OK)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "igmp_leavegroup failed with error %d\r\n", err);
                                    break;
                                }
                            }
#endif
#if LWIP_IPV6
                            if (IP_IS_V6(&if_addr))
                            {
                                err = mld6_leavegroup(ip_2_ip6(&if_addr), ip_2_ip6(&multi_addr));
                                if(err != ERR_OK)
                                {
                                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "mld6_leavegroup failed with error %d\r\n", err);
                                    break;
                                }
                            }
#endif
                        }
                    } while(0);

                    cy_rtos_set_mutex(&multicast_join_leave_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    if(err != ERR_OK)
                    {
                        result = lwip_to_secure_socket_error(err);
                    }

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return result;
                }

                case CY_SOCKET_SO_IP_MULTICAST_TTL:
                {
                    if(NETCONNTYPE_GROUP(netconn_type(ctx->conn_handler)) != NETCONN_UDP)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Multicast TTL option is supported only for UDP sockets\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                    }
                    udp_set_multicast_ttl(ctx->conn_handler->pcb.udp, *(uint8_t *)optval);
                    break;
                }

                case CY_SOCKET_SO_IP_TOS:
                {
                    if( (ctx->conn_handler == NULL) || (ctx->conn_handler->pcb.ip == NULL) )
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid connection handle or IP handle \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    ctx->conn_handler->pcb.ip->tos = (*(uint8_t *)optval);
                    break;
                }

                default:
                {
                    cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option = [%d]\r\n", optname);

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }

            }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_setsockopt End\r\n");
    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_getsockopt(cy_socket_t handle,  int level, int optname, void *optval, uint32_t *optlen)
{
    cy_socket_ctx_t *ctx = NULL;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(optval == NULL || optlen == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_getsockopt bad arg\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    switch(level)
    {
        case CY_SOCKET_SOL_TLS:
        {
            switch(optname)
            {
                case CY_SOCKET_SO_TLS_AUTH_MODE:
                {
                    if(*optlen < sizeof(cy_socket_tls_auth_mode_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

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
                    if(ctx->hostname)
                    {
                        if(*optlen < strlen(ctx->hostname))
                        {
                            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

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

                    if(*optlen < sizeof(mfl))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

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
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        case CY_SOCKET_SOL_SOCKET:
        {
            switch(optname)
            {
                case CY_SOCKET_SO_RCVTIMEO:
#if LWIP_SO_RCVTIMEO
                {
                    if(*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *((uint32_t *)optval) = netconn_get_recvtimeout(ctx->conn_handler);
                    *optlen = sizeof(uint32_t);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Receive timeout option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_SNDTIMEO:
#if LWIP_SO_SNDTIMEO
                {
                    if(*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *((uint32_t *)optval) = netconn_get_sendtimeout(ctx->conn_handler);
                    *optlen = sizeof(uint32_t);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Send timeout option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_BYTES_AVAILABLE:
                {
                    int recv_avail = 0;

                    if(*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    /* Acquire mutex, to sync available data with cy_socket_recv function. */
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
                    cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);

                    /* Get the number of bytes available with lwIP */
                    SYS_ARCH_GET(ctx->conn_handler->recv_avail, recv_avail);

                    if(NETCONNTYPE_GROUP(netconn_type(ctx->conn_handler)) == NETCONN_TCP)
                    {
                        /* If there is a partially read pbuf, then add the number of bytes available in that pbuf. */
                        if(ctx->buf)
                        {
                            recv_avail += (ctx->buf->tot_len - ctx->offset);
                        }
                    }

                    *(uint32_t *)optval = recv_avail;
                    *optlen = sizeof(recv_avail);

                    cy_rtos_set_mutex(&ctx->netconn_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    break;
                }

                case CY_SOCKET_SO_NONBLOCK:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Non-block socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }

                case CY_SOCKET_SO_TCP_KEEPALIVE_ENABLE:
                {
                    if(*optlen < sizeof(int))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    if( SOF_KEEPALIVE == ip_get_option(ctx->conn_handler->pcb.ip, SOF_KEEPALIVE))
                    {
                        *(int *)optval = 1;
                    }
                    else
                    {
                        *(int *)optval = 0;
                    }
                    *optlen = sizeof(int);
                    break;
                }

                case CY_SOCKET_SO_BROADCAST:
#if IP_SOF_BROADCAST
                {
                    if(*optlen < sizeof(uint8_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    if( SOF_BROADCAST == ip_get_option(ctx->conn_handler->pcb.ip, SOF_BROADCAST))
                    {
                        *(uint8_t *)optval = 1;
                    }
                    else
                    {
                        *(uint8_t *)optval = 0;
                    }
                    *optlen = sizeof(uint8_t);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Incompatible Socket option\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
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
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        case CY_SOCKET_SOL_TCP:
        {
            switch(optname)
            {
                case CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL:
#if LWIP_TCP_KEEPALIVE
                {
                    if(*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *(uint32_t *)optval = (uint32_t)(ctx->conn_handler->pcb.tcp->keep_intvl);
                    *optlen = sizeof(uint32_t);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt keep-alive interval %ld\r\n", *(uint32_t *)optval);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_TCP_KEEPALIVE_COUNT:
#if LWIP_TCP_KEEPALIVE
                {
                    if(*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *(uint32_t *)optval = (uint32_t)(ctx->conn_handler->pcb.tcp->keep_cnt);
                    *optlen = sizeof(uint32_t);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt keep-alive count %ld\r\n", *(uint32_t *)optval);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME:
#if LWIP_TCP_KEEPALIVE
                {
                    if(*optlen < sizeof(uint32_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *(uint32_t *)optval = (uint32_t)(ctx->conn_handler->pcb.tcp->keep_idle);
                    *optlen = sizeof(uint32_t);

                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt keep-alive idle time %ld\r\n", *(uint32_t *)optval);
                    break;
                }
#else
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket option not supported\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                }
#endif
                case CY_SOCKET_SO_TCP_NODELAY:
                {
                    if(*optlen < sizeof(uint8_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *(uint8_t *)optval = (uint8_t)tcp_nagle_disabled(ctx->conn_handler->pcb.tcp);
                    *optlen = sizeof(uint8_t);

                    break;
                }
                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;

        case CY_SOCKET_SOL_IP:
        {
            switch(optname)
            {
                case CY_SOCKET_SO_IP_MULTICAST_TTL:
                {
                    if(*optlen < sizeof(uint8_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    if(NETCONNTYPE_GROUP(netconn_type(ctx->conn_handler)) != NETCONN_UDP)
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Multicast TTL option is supported only for UDP sockets\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_OPTION_NOT_SUPPORTED;
                    }
                    *(uint8_t *)optval = udp_get_multicast_ttl(ctx->conn_handler->pcb.udp);
                    *optlen = sizeof(uint8_t);

                    break;
                }

                case CY_SOCKET_SO_IP_TOS:
                {
                    if(*optlen < sizeof(uint8_t))
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "insufficient option value buffer\r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    if( (ctx->conn_handler == NULL) || (ctx->conn_handler->pcb.ip == NULL) )
                    {
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid connection handle or IP handle \r\n");

                        cy_rtos_set_mutex(&ctx->socket_mutex);
                        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
                    }

                    *(uint8_t *)optval = ctx->conn_handler->pcb.ip->tos;
                    *optlen = sizeof(uint8_t);

                    break;
                }
                default:
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid Socket option\r\n");

                    cy_rtos_set_mutex(&ctx->socket_mutex);
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

                    return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
                }
            }
        }
        break;
        default:
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid level\r\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_OPTION;
        }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_getsockopt End\r\n");
    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_connect(cy_socket_t handle, cy_socket_sockaddr_t * address, uint32_t address_length)
{
    cy_socket_ctx_t * ctx;
    ip_addr_t remote;
    cy_tls_params_t tls_params = { 0 };
    err_t ret;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    UNUSED_ARG(address_length);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_connect Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(address == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Address passed is NULL\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }


    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_connect API is not support for UDP sockets\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    memset(&remote, 0, sizeof(ip_addr_t));

    /* convert IP format from secure socket to LWIP */
    result = convert_secure_socket_to_lwip_ip_addr(&remote, &address->ip_address);
    if(result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to LWIP \r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    /* Acquire mutex, so that socket disconnect and connect are in sync. While connect operation is in progress
     * this mutex lock will prevent disconnection. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    ret = netconn_connect(ctx->conn_handler, &remote, address->port) ;
    if(ret != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_connect failed with error = %d \r\n", ret);
        result = LWIP_TO_CY_SECURE_SOCKETS_ERR(ret);
        goto exit;
    }
    ctx->status |=  SOCKET_STATUS_FLAG_CONNECTED;

    if(ctx->enforce_tls)
    {
        tls_params.context = ctx;
        tls_params.network_send = tls_network_send_callback;
        tls_params.network_recv = tls_network_receive_callback;
        tls_params.tls_identity = ctx->tls_identity;
        tls_params.rootca_certificate = ctx->rootca_certificate;
        tls_params.rootca_certificate_length = ctx->rootca_certificate_len;
        tls_params.auth_mode = ctx->auth_mode;
        tls_params.mfl_code = ctx->mfl_code;
        tls_params.hostname = ctx->hostname;
        tls_params.alpn_list = (const char**)ctx->alpn_list;
#ifdef CY_SECURE_SOCKETS_PKCS_SUPPORT
        tls_params.load_rootca_from_ram = ctx->load_rootca_from_ram;
        tls_params.load_device_cert_key_from_ram = ctx->load_device_cert_key_from_ram;
#endif
        result = cy_tls_create_context(&ctx->tls_ctx, &tls_params);
        if(result != CY_RSLT_SUCCESS)
        {
            netconn_close(ctx->conn_handler);
            ctx->status &= ~(SOCKET_STATUS_FLAG_CONNECTED);

            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_create_context failed with error %lu\n", result);
            result = TLS_TO_CY_SECURE_SOCKETS_ERR(result);
            goto exit;
        }
        result = cy_tls_connect(ctx->tls_ctx, CY_TLS_ENDPOINT_CLIENT, 0);
        if(result != CY_RSLT_SUCCESS)
        {
            netconn_close(ctx->conn_handler);
            ctx->status &= ~(SOCKET_STATUS_FLAG_CONNECTED);

            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_connect failed with error %lu\n", result);
            result = TLS_TO_CY_SECURE_SOCKETS_ERR(result);
            goto exit;
        }
        ctx->status |=  SOCKET_STATUS_FLAG_SECURED;
        ctx->data_received = 0;
    }

exit:
    cy_rtos_set_mutex(&ctx->netconn_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_connect End\r\n");

    return result;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_disconnect(cy_socket_t handle, uint32_t timeout)
{
    cy_socket_ctx_t *ctx;
    err_t error;
    UNUSED_ARG(timeout);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_disconnect Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_disconnect API is not support for UDP sockets\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);
    if(ctx->role == CY_TLS_ENDPOINT_CLIENT)
    {
        if((ctx->status & SOCKET_STATUS_FLAG_CONNECTED) !=  SOCKET_STATUS_FLAG_CONNECTED)
        {
            /* Connection is not established. But check, netconn handler is created, if yes, delete it */
            if(ctx->conn_handler)
            {
                error = netconn_delete(ctx->conn_handler);
                if(error != ERR_OK)
                {
                    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_delete failed with error %d\n",  error);
                }
                else
                {
                    ctx->conn_handler = NULL;
                }
            }

            cy_rtos_set_mutex(&ctx->netconn_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Socket not connected\r\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
        }

        if(true == ctx->enforce_tls && (ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED)
        {
            cy_tls_delete_context(ctx->tls_ctx);

            /* Free the memory allocated for RootCA certificate for the socket.*/
            if(ctx->rootca_certificate)
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
        if((ctx->status & SOCKET_STATUS_FLAG_LISTENING) != SOCKET_STATUS_FLAG_LISTENING)
        {
            cy_rtos_set_mutex(&ctx->netconn_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Socket not listening\r\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_LISTENING;
        }
        ctx->status &= ~(SOCKET_STATUS_FLAG_LISTENING);
    }

    /* Free the pbuf that is stored when partial data is copied from it */
    if(ctx->buf)
    {
        pbuf_free(ctx->buf);
        ctx->buf = NULL;
    }

    /* Close the netconn */
    error = netconn_close(ctx->conn_handler);
    if(error != ERR_OK)
    {
        if(error == ERR_CONN || error == ERR_CLSD)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_close failed with error %d\n",  error);
        }
        else
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_close failed with error %d\n",  error);
        }
    }

    /* LwIP doesn't allow reusing netconn object once it's closed. Also for server mode sockets, the client sockets
     * needs to be managed internally without expecting the caller to invoke cy_socket_delete() for the accepted client sockets.
     * Hence deleting the netconn object in this function (i.e., cy_socket_disconnect) instead of cy_socket_delete.
     */
    if(ctx->conn_handler)
    {
        error = netconn_delete(ctx->conn_handler);
        if(error != ERR_OK)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_delete failed with error %d\n",  error);
        }
        else
        {
            ctx->conn_handler = NULL;
        }
    }
    
    /* Reset the data_received element of the socket */
    ctx->data_received = 0;

    /* Release the mutex as connection status is already updated and done with LwIP netconn close/delete */
    cy_rtos_set_mutex(&ctx->netconn_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

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

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_send Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(bytes_sent == NULL || data == NULL || size == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "bytes_sent or data pointer is NULL or size is zero \r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_send API is not support for UDP sockets\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);

    /* Check if the socket is not in connected state. */
    if( !( (!ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED) ||
           ( (ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) ) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket not connected \r\n");
        cy_rtos_set_mutex(&ctx->netconn_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
    }

    if(ctx->enforce_tls)
    {
        /* Send through TLS pipe. */
        ret = cy_tls_send(ctx->tls_ctx, data, size, 0, bytes_sent);
        if(ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_tls_send failed with error code %lu\n", ret);
            ret = TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
    }
    else
    {
        ret = network_send((void *)ctx, data, size, bytes_sent);
        if( ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_network_send failed with error code %lu\n", ret);
        }
    }

    cy_rtos_set_mutex(&ctx->netconn_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_send End\r\n");
    return ret;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_sendto(cy_socket_t handle, const void *buffer, uint32_t length, int flags, const cy_socket_sockaddr_t *dest_addr, uint32_t address_length, uint32_t *bytes_sent)
{
    cy_socket_ctx_t *ctx;
    struct netbuf *buf = NULL;
    err_t err;
    ip_addr_t remote;
    cy_rslt_t result;

    UNUSED_ARG(flags);
    UNUSED_ARG(address_length);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_sendto Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(dest_addr == NULL || buffer == NULL || bytes_sent == NULL || length == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "dest_addr or buffer or bytes_sent pointers are NULL or length is zero\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    *bytes_sent = 0;

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_TCP || ctx->transport_protocol == CY_SOCKET_IPPROTO_TLS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_sendto API is not supported for TCP/TLS sockets\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    memset(&remote, 0, sizeof(remote));

    /* convert IP format from secure socket to LWIP */
    result = convert_secure_socket_to_lwip_ip_addr(&remote, &dest_addr->ip_address);
    if( result != CY_RSLT_SUCCESS )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to LWIP \r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

#if LWIP_IPV4
    if(CY_SOCKET_IP_VER_V4 == dest_addr->ip_address.version)
    {
        /* If destination is not present in the ARP table, try to resolve it */
        result = eth_arp_resolve(ip_2_ip4(&remote), ctx->iface_type);
        if(result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "eth_arp_resolve failed\r\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return result;
        }
    }
#endif

    /* Create a netbuf */
    buf = netbuf_new();
    if(buf == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netbuf_new failed\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    /* Attach buffer pointer to netbuf */
    err = netbuf_ref(buf, buffer, (u16_t)length);
    if(err != ERR_OK)
    {
        netbuf_delete(buf);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netbuf_ref failed with error %d\n", err);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return LWIP_TO_CY_SECURE_SOCKETS_ERR(err);
    }

    /* Send data */
    err = netconn_sendto(ctx->conn_handler, buf, &remote, dest_addr->port);
    if(err != ERR_OK)
    {
        netbuf_delete(buf);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_sendto failed with error %d\n", err);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return LWIP_TO_CY_SECURE_SOCKETS_ERR(err);
    }

    netbuf_delete(buf);

    *bytes_sent = length;

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_sendto End\r\n");

    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_recv(cy_socket_t handle, void * data, uint32_t size, int flags, uint32_t *bytes_received)
{
    cy_socket_ctx_t * ctx;
    cy_rslt_t ret;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recv Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* buffer cannot be null or buffer length cannot be 0 */
    if(data == NULL || size == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Buffer passed is NULL or buffer length is passed as 0 \r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if(bytes_received == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "bytes_received passed as NULL \r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *)handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_UDP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_recv API is not support for UDP sockets\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);

    /* Check if the socket is not in connected state. */
    if( !( (!ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED) ||
           ( (ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) ) )
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Socket not connected \r\n");
        cy_rtos_set_mutex(&ctx->netconn_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
    }

    if(ctx->enforce_tls)
    {
        /* Send through TLS pipe, if negotiated. */
        ret = cy_tls_recv(ctx->tls_ctx, data, size, 0, bytes_received);
        if(ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_tls_recv failed with error %ld\n", ret);
            ret = TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
        ctx->data_received = cy_tls_get_bytes_avail(ctx->tls_ctx);
    }
    else
    {
        ret = network_receive((void *)ctx, data, size, bytes_received);
        if(ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "network_recv_callback failed with error [0x%lX]\n", ret);
        }
    }

    cy_rtos_set_mutex(&ctx->netconn_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recv End\r\n");
    return ret;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_recvfrom(cy_socket_t handle, void *buffer, uint32_t length, int flags, cy_socket_sockaddr_t *src_addr, uint32_t *src_addr_length, uint32_t *bytes_received)
{
    cy_socket_ctx_t *ctx = NULL;
    ip_addr_t *addr = NULL;
    ip_addr_t remote;
    struct netbuf *net_buf = NULL;
    err_t err;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    bool src_filter_set = false;
    cy_socket_sockaddr_t peer_addr;

    UNUSED_ARG(src_addr_length);

    memset(&peer_addr, 0, sizeof(cy_socket_sockaddr_t));

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recvfrom Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");

        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->transport_protocol == CY_SOCKET_IPPROTO_TCP || ctx->transport_protocol == CY_SOCKET_IPPROTO_TLS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_recvfrom API is not supported for TCP/TLS sockets\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_SUPPORTED;
    }

    /* buffer cannot be null or buffer length cannot be 0 */
    if(buffer == NULL || length == 0)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Buffer passed is NULL or buffer length is passed as 0 \r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if(bytes_received == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "bytes_received passed as NULL \r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    *bytes_received = 0;

    /* src_addr cannot be null when flag passed as CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER */
    if((flags & CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER) == CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER)
    {
        if(src_addr == NULL)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Flag CY_SOCKET_FLAGS_RECVFROM_SRC_FILTER is set. Hence src_addr can not be null \r\n");

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
        }
        else
        {
            src_filter_set = true;
        }
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_recvfrom_mutex locked %s %d\n", __FILE__, __LINE__);
    result = cy_rtos_get_mutex(&netconn_recvfrom_mutex, CY_RTOS_NEVER_TIMEOUT);
    if(result != CY_RSLT_SUCCESS)
    {
        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    if(src_filter_set == true)
    {
        memset(&remote, 0, sizeof(ip_addr_t));

        /* convert IP format from secure socket to LWIP */
        result = convert_secure_socket_to_lwip_ip_addr(&remote, &src_addr->ip_address);
        if( result != CY_RSLT_SUCCESS )
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to LWIP \r\n");
            cy_rtos_set_mutex(&netconn_recvfrom_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_recvfrom_mutex unlocked %s %d\n", __FILE__, __LINE__);

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return result;
        }

        err = netconn_connect(ctx->conn_handler, &remote, src_addr->port) ;
        if(err != ERR_OK)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_connect failed with error = %d \r\n", err);
            cy_rtos_set_mutex(&netconn_recvfrom_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_recvfrom_mutex unlocked %s %d\n", __FILE__, __LINE__);

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return LWIP_TO_CY_SECURE_SOCKETS_ERR(err);
        }
    }

    err = netconn_recv(ctx->conn_handler, &net_buf);
    if(err != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_recv failed with error %d\n", err);
        result = LWIP_TO_CY_SECURE_SOCKETS_ERR(err);
        goto exit;
    }

    addr = netbuf_fromaddr(net_buf);
    peer_addr.port = netbuf_fromport(net_buf);

    /* convert IP format from LWIP to secure socket */
    result = convert_lwip_to_secure_socket_ip_addr(&peer_addr.ip_address, addr);
    if(result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from LWIP to secure socket \r\n");
        goto exit;
    }

    *bytes_received = netbuf_copy(net_buf, buffer, (u16_t)length);

    /* Copy source address */
    if(src_addr != NULL)
    {
        memcpy(src_addr, &peer_addr, sizeof(cy_socket_sockaddr_t));
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_recvfrom End\r\n");

exit:
    if(src_filter_set == true)
    {
        netconn_disconnect(ctx->conn_handler);
    }

    /* TODO : Currently remaining data in netbuf is silently discarded if application doesnt pass enough buffer
     *        to copy data. Need to implement logic to buffer the data if application doesnt pass enough buffer.
     */
    if(net_buf != NULL)
    {
        netbuf_delete(net_buf);
    }

    cy_rtos_set_mutex(&netconn_recvfrom_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_recvfrom_mutex unlocked %s %d\n", __FILE__, __LINE__);

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    return result;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_gethostbyname(const char *hostname, cy_socket_ip_version_t ip_ver, cy_socket_ip_address_t *addr)
{
    err_t          result;
    ip_addr_t      ip_address ;
    uint8_t        dns_type = NETCONN_DNS_IPV4;

    /* dns_type is only used when both LWIP_IPV4 and LWIP_IPV6 is enabled in LWIP configuration file.
     * otherwise dns_type is unused and gives warning during compilation. To fix the warning, made variable
     * UNUSED but still can be used for case when both LWIP_IPV4 and LWIP_IPV6 enabled.
     */
    UNUSED_VARIABLE(dns_type);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_gethostbyname Start\r\n");
    if( (NULL == hostname) || (NULL == addr))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_gethostbyname failed invalid arg \n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if(ip_ver == CY_SOCKET_IP_VER_V6)
    {
#if LWIP_IPV6
        dns_type = NETCONN_DNS_IPV6;
#else
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_gethostbyname failed LWIP_IPV6 not enabled \n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
#endif
    }
    else if(ip_ver == CY_SOCKET_IP_VER_V4)
    {
#if LWIP_IPV4
        dns_type = NETCONN_DNS_IPV4;
#else
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_gethostbyname failed LWIP_IPV4 not enabled \n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
#endif
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Invalid IP version type \n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    result = netconn_gethostbyname_addrtype(hostname, &ip_address, dns_type);
    if(result != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_gethostbyname failed with error code %d\n", result);
        if( (result == ERR_ARG) || (result == ERR_VAL))
        {
            return CY_RSLT_MODULE_SECURE_SOCKETS_HOST_NOT_FOUND;
        }
        else
        {
            return LWIP_TO_CY_SECURE_SOCKETS_ERR(result);
        }
    }

    /* convert IP format from LWIP to secure socket */
    result = convert_lwip_to_secure_socket_ip_addr(addr, &ip_address);
    if(result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from LWIP to secure socket \r\n");
        return result;
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_gethostbyname End\r\n");
    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_poll(cy_socket_t handle, uint32_t *rwflags, uint32_t timeout)
{
    cy_socket_ctx_t * ctx;
    uint32_t flags;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int recv_avail = 0;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_poll Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(rwflags == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "rwflags pointer passed is NULL\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    flags = *rwflags;
    *rwflags = 0;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex locked %s %d\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->netconn_mutex, CY_RTOS_NEVER_TIMEOUT);

    if( ( ctx->transport_protocol != CY_SOCKET_IPPROTO_UDP ) &&
        !( ( !ctx->enforce_tls && ( ctx->status & SOCKET_STATUS_FLAG_CONNECTED) ==  SOCKET_STATUS_FLAG_CONNECTED ) ||
        ( ( ctx->status & SOCKET_STATUS_FLAG_SECURED) ==  SOCKET_STATUS_FLAG_SECURED) ) )
    {
        cy_rtos_set_mutex(&ctx->netconn_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED;
    }

    if(flags & CY_SOCKET_POLL_READ )
    {
        SYS_ARCH_GET(ctx->conn_handler->recv_avail, recv_avail);
        if(recv_avail > 0 || ctx->buf)
        {
            *rwflags |= CY_SOCKET_POLL_READ;
        }
        else
        {
            while(timeout > 0)
            {
                SYS_ARCH_GET(ctx->conn_handler->recv_avail, recv_avail);
                if(recv_avail > 0 || ctx->buf)
                {
                    *rwflags |= CY_SOCKET_POLL_READ;
                    break ;
                }

                cy_rtos_delay_milliseconds(1);
                if(timeout != 0xffffffff)
                {
                    timeout--;
                }
            }
        }
    }
    if(flags & CY_SOCKET_POLL_WRITE)
    {
        while(timeout > 0)
        {
            if(ctx->send_events)
            {
                *rwflags |= CY_SOCKET_POLL_WRITE;
                break;
            }

            cy_rtos_delay_milliseconds(1);
            if(timeout != 0xffffffff)
            {
                timeout--;
            }
        }
    }

    cy_rtos_set_mutex(&ctx->netconn_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "netconn_mutex unlocked %s %d\n", __FILE__, __LINE__);

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
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(ctx->transport_protocol != CY_SOCKET_IPPROTO_UDP)
    {
        /* disconnect the socket */
        cy_socket_disconnect(ctx, 0);

        /*
         *  FALSE-POSITIVE: CID 241171: Read from pointer after free (USE_AFTER_FREE)
         *  Reason:
         *     'ctx' passed as argument to this function can't be accepted client context, hence ctx->server_socket will be always NULL.
         *     Hence this API `cy_socket_disconnect(ctx, 0);` which is invoked above in this function will never invoke `free_socket(ctx);`
         *     Therefore when the code execution reaches this point, 'ctx' will not be NULL.
         */
        if(ctx->hostname)
        {
            free(ctx->hostname);
            ctx->hostname = NULL;
        }

        /* free alpn string */
        if(ctx->alpn)
        {
            free(ctx->alpn);
            ctx->alpn = NULL;
        }

        /* free alpn array of strings */
        if(ctx->alpn_list)
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
        ip_addr_t if_addr;
        ip_addr_t multi_addr;
        err_t err = ERR_OK;

        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&multicast_join_leave_mutex, CY_RTOS_NEVER_TIMEOUT);

        /* Call wifi-mw-core network activity function to resume the network stack. */
        cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

        num_multicast_groups = multicast_info.multicast_member_count;
        while(count < num_multicast_groups)
        {
            member_index = next_registered_multicast_slot(member_index);

            if(multicast_info.multicast_member_list[member_index].socket == ctx)
            {
                convert_secure_socket_to_lwip_ip_addr(&if_addr, &multicast_info.multicast_member_list[member_index].if_addr);
                convert_secure_socket_to_lwip_ip_addr(&multi_addr, &multicast_info.multicast_member_list[member_index].multi_addr);
#if LWIP_IPV4
                if(IP_IS_V4(&if_addr))
                {
                    err = igmp_leavegroup(ip_2_ip4(&if_addr), ip_2_ip4(&multi_addr));
                    if(err != ERR_OK)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "igmp_leavegroup failed with error %d\r\n", err);
                    }
                }
#endif
#if LWIP_IPV6
                if(IP_IS_V6(&if_addr))
                {
                    err = mld6_leavegroup(ip_2_ip6(&if_addr), ip_2_ip6(&multi_addr));
                    if(err != ERR_OK)
                    {
                        cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "mld6_leavegroup failed with error %d\r\n", err);
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

        netconn_delete(ctx->conn_handler);
        ctx->conn_handler = NULL;
    }
    /* free the socket context */
    free_socket(ctx);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_delete End\r\n");

    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_deinit(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_deinit Start\r\n");
    if(!init_ref_count)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "library not initialized\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_INITIALIZED;
    }

    init_ref_count--;
    if(!init_ref_count)
    {
        result = cy_tls_deinit();
        if(result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_deinit failed \r\n");
        }
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&socket_list_mutex, CY_RTOS_NEVER_TIMEOUT);
        memset(socket_list, 0, sizeof(socket_list));
        cy_rtos_set_mutex(&socket_list_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_list_mutex unlocked %s %d\n", __FILE__, __LINE__);
        cy_rtos_deinit_mutex(&socket_list_mutex);
        cy_rtos_deinit_mutex(&accept_recv_event_mutex);
        cy_rtos_deinit_mutex(&netconn_recvfrom_mutex);

        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex locked %s %d\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&multicast_join_leave_mutex, CY_RTOS_NEVER_TIMEOUT);
        if(multicast_info.multicast_member_list)
        {
            free(multicast_info.multicast_member_list);
            multicast_info.multicast_member_list = NULL;
        }

        multicast_info.multicast_member_status = 0;
        multicast_info.multicast_member_count = 0;

        cy_rtos_set_mutex(&multicast_join_leave_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "multicast_join_leave_mutex unlocked %s %d\n", __FILE__, __LINE__);

        cy_rtos_deinit_mutex(&multicast_join_leave_mutex);

        cy_worker_thread_delete(&socket_worker);
    }
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_deinit End\r\n");
    return result;
}

/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_bind(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t address_length)
{
    cy_socket_ctx_t * ctx;
    ip_addr_t ipaddr;
    err_t result;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_bind Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    if(address == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Address passed is NULL\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }


    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    memset(&ipaddr, 0, sizeof(ipaddr));

    /* Convert IP format from secure socket to LWIP */
    result = convert_secure_socket_to_lwip_ip_addr(&ipaddr, &address->ip_address);
    if(result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from secure socket to LWIP \r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    /*
     * Setting SOF_REUSEADDR socket option in order to be able to bind to the same ip:port again
     * without netconn_bind failing.
     */
    ip_set_option(ctx->conn_handler->pcb.ip, SOF_REUSEADDR);

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    result = netconn_bind(ctx->conn_handler, &ipaddr, address->port) ;
    if(result != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_bind failed with error = %d \r\n", result);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return LWIP_TO_CY_SECURE_SOCKETS_ERR(result);
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_bind End\r\n");
    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_listen(cy_socket_t handle, int backlog)
{
    cy_socket_ctx_t * ctx;
    err_t result;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_listen Start\r\n");
    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->conn_handler == NULL)
    {
        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    /* Check if this socket is already listening */
    if(ctx->conn_handler->state == NETCONN_LISTEN)
    {
        ctx->status |=  SOCKET_STATUS_FLAG_LISTENING;

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_SUCCESS;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

#if LWIP_SO_RCVTIMEO

    if(!ctx->is_recvtimeout_set)
    {
        /* Reset recv timeout for server socket to zero if application has not set it. */
        netconn_set_recvtimeout(ctx->conn_handler, 0);
    }

#endif

    result = netconn_listen_with_backlog(ctx->conn_handler, backlog);
    if(result != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_listen_with_backlog failed with error = %d \r\n", result);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return LWIP_TO_CY_SECURE_SOCKETS_ERR(result);
    }

    ctx->role = CY_TLS_ENDPOINT_SERVER;
    ctx->status |=  SOCKET_STATUS_FLAG_LISTENING;

    /* If user has not set the authentication mode, set the default authentication mode to NONE. */
    if(!ctx->is_authmode_set)
    {
        ctx->auth_mode = CY_SOCKET_TLS_VERIFY_NONE;
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_listen End\r\n");
    return CY_RSLT_SUCCESS;
}
/*-----------------------------------------------------------*/
cy_rslt_t cy_socket_accept(cy_socket_t handle, cy_socket_sockaddr_t *address, uint32_t *address_length, cy_socket_t *socket)
{
    cy_socket_ctx_t *ctx;
    cy_socket_ctx_t *accept_ctx;
    struct netconn *conn;
    cy_tls_params_t tls_params = { 0 };
    err_t result;
    cy_rslt_t ret;
    int recvevent = 0;
    int disconnectevent = 0;
    ip_addr_t peer_addr;
    u16_t port;
    cy_socket_recv_event_t *pending_recv_events;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_accept Start\r\n");
    if(socket == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Accepted client socket pointer is NULL\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    *socket = CY_SOCKET_INVALID_HANDLE;

    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid server handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }
    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid server handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(ctx->conn_handler == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid server handle\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if(address == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Address passed as NULL\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    /* Check if this socket is in listening state or not */
    if(!(ctx->status &=  SOCKET_STATUS_FLAG_LISTENING))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Server socket is not in listening state\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_LISTENING;
    }

    accept_ctx = alloc_socket();
    if(accept_ctx == NULL)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to allocate socket context for accepted client socket\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
    }

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    result = netconn_accept(ctx->conn_handler, &conn);
    if(result != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn_accept failed with error = %d \r\n", result);
        free_socket(accept_ctx);

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return LWIP_TO_CY_SECURE_SOCKETS_ERR(result);
    }
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "new connection accepted.\n\n");

    accept_ctx->conn_handler = conn;
    accept_ctx->status |=  SOCKET_STATUS_FLAG_CONNECTED;
    accept_ctx->callbacks.disconnect = ctx->callbacks.disconnect;
    accept_ctx->callbacks.receive = ctx->callbacks.receive;

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

    /*
     * Find if disconnect and receive events occurred prior to socket context is created for accepted client socket.
     * conn->socket is -1 if no disconnect or receive events are occurred before socket context is created for accepted client socket.
     */
    if(conn->socket != -1)
    {
        /*
         * Check the disconnect event bit.
         */
        disconnectevent = (conn->socket) & SECURE_SOCKETS_DISCONNET_EVENT_BIT_MASK;

        /*
         * Clear disconnect event bit in conn->socket
         */
        conn->socket = (conn->socket) & ~SECURE_SOCKETS_DISCONNET_EVENT_BIT_MASK;

        /*
         * Get the receive event count occurred before socket context is created.
         */
        recvevent = (conn->socket) & ~SECURE_SOCKETS_RECEIVE_EVENT_BIT_MASK;

        /*
         * Clear receive event bit in conn->socket. Clearing this bit helps in cy_process_receive_event function to identify that socket context is created.
         */
        conn->socket = (conn->socket) & ~SECURE_SOCKETS_RECEIVE_EVENT_BIT_MASK;
    }

    conn->socket = accept_ctx->id;
    cy_rtos_set_mutex(&accept_recv_event_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);

    *socket = accept_ctx;

    /*
     * If the receive callback is already registered on server socket, queue an event to process the pending receive events.
     * If the receive callback is not registered yet, all the pending receive events are processed in socket option CY_SOCKET_SO_RECEIVE_CALLBACK.
     */
    if(accept_ctx->callbacks.receive.callback)
    {

        /* Allocate memory for event to be pushed to the worker thread for processing the receive events occurred prior to socket context creation for accepted client socket.*/
        pending_recv_events = calloc(1, sizeof(cy_socket_recv_event_t));
        if(pending_recv_events == NULL)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "memory allocation failed for receive evetnt structure\r\n");
            netconn_close(accept_ctx->conn_handler);
            netconn_delete(accept_ctx->conn_handler);
            free_socket(accept_ctx);
            *socket = CY_SOCKET_INVALID_HANDLE;

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return CY_RSLT_MODULE_SECURE_SOCKETS_NOMEM;
        }

        pending_recv_events->recv_count = recvevent;
        pending_recv_events->socket = accept_ctx;

         /* Push event to worker thread to process all the pending receive events.*/
        result = cy_worker_thread_enqueue(&socket_worker, cy_process_pending_receive_events, (void *)pending_recv_events);
        if(result != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
            netconn_close(accept_ctx->conn_handler);
            netconn_delete(accept_ctx->conn_handler);
            free_socket(accept_ctx);
            *socket = CY_SOCKET_INVALID_HANDLE;
            free(pending_recv_events);

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return result;
        }
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

        accept_ctx->recv_events_cache -= recvevent;

        cy_rtos_set_mutex(&accept_recv_event_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);
    }
    else
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex locked %s %d\r\n", __FILE__, __LINE__);
        cy_rtos_get_mutex(&accept_recv_event_mutex, CY_RTOS_NEVER_TIMEOUT);

        accept_ctx->recv_events_cache += recvevent;

        cy_rtos_set_mutex(&accept_recv_event_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "accept_recv_event_mutex unlocked %s %d\n", __FILE__, __LINE__);

    }

#if LWIP_SO_RCVTIMEO

    /**
     * Set the receive timeout to the value set by the application. If not already set, use the default value.
     */

    if(ctx->is_recvtimeout_set)
    {
        netconn_set_recvtimeout(accept_ctx->conn_handler, netconn_get_recvtimeout(ctx->conn_handler));
    }
    else
    {
        netconn_set_recvtimeout(accept_ctx->conn_handler, DEFAULT_RECV_TIMEOUT_IN_MSEC);
    }

#endif

#if LWIP_SO_SNDTIMEO

    /**
     * Set the send timeout to the value set by the application. If not already set, use the default value.
     */
    if(ctx->is_sendtimeout_set)
    {
        netconn_set_sendtimeout(accept_ctx->conn_handler, netconn_get_sendtimeout(ctx->conn_handler));
    }
    else
    {
        netconn_set_sendtimeout(accept_ctx->conn_handler, DEFAULT_SEND_TIMEOUT_IN_MSEC);
    }
#endif

    result = netconn_peer(accept_ctx->conn_handler, &peer_addr, &port);

    /* Convert IP format from LWIP to secure socket */
    result = convert_lwip_to_secure_socket_ip_addr(&address->ip_address, &peer_addr);
    if(result != CY_RSLT_SUCCESS)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed conversion from LWIP to secure socket \r\n");
        netconn_close(accept_ctx->conn_handler);
        netconn_delete(accept_ctx->conn_handler);
        free_socket(accept_ctx);
        *socket = CY_SOCKET_INVALID_HANDLE;

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return result;
    }

    address->port = port;
    *address_length = sizeof(*address);

    if(ctx->enforce_tls)
    {
        tls_params.context = accept_ctx;
        tls_params.network_send = tls_network_send_callback;
        tls_params.network_recv = tls_network_receive_callback;
        tls_params.tls_identity = ctx->tls_identity;
        tls_params.auth_mode = ctx->auth_mode;

        accept_ctx->enforce_tls = 1;

        ret = cy_tls_create_context(&accept_ctx->tls_ctx, &tls_params);
        if(ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_create_context failed with error %ld\n", ret);
            netconn_close(accept_ctx->conn_handler);
            netconn_delete(accept_ctx->conn_handler);
            free_socket(accept_ctx);
            *socket = CY_SOCKET_INVALID_HANDLE;

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
        
        ret = cy_tls_connect(accept_ctx->tls_ctx, CY_TLS_ENDPOINT_SERVER, 0);
        if(ret != CY_RSLT_SUCCESS)
        {
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_tls_connect failed with error %lu\n", ret);
            cy_tls_delete_context(accept_ctx->tls_ctx);
            netconn_close(accept_ctx->conn_handler);
            netconn_delete(accept_ctx->conn_handler);
            free_socket(accept_ctx);
            *socket = CY_SOCKET_INVALID_HANDLE;

            cy_rtos_set_mutex(&ctx->socket_mutex);
            ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

            return TLS_TO_CY_SECURE_SOCKETS_ERR(ret);
        }
        accept_ctx->status |=  SOCKET_STATUS_FLAG_SECURED;
        ctx->data_received = 0;
    }

    /*
     * If disconnect event occurred and disconnect callback is registered then queue the disconnect event to worker thread.
     */
    if(disconnectevent)
    {
        if(accept_ctx->callbacks.disconnect.callback)
        {
            result = cy_worker_thread_enqueue(&socket_worker, cy_process_connect_disconnect_notification_event, (void *)accept_ctx->conn_handler);
            if(result != CY_RSLT_SUCCESS)
            {
                ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_defer_work failed at file: %s line: %d with error: 0x%lx\r\n", __FILE__, __LINE__, result);
                /*
                 * Do not fail cy_socket_accept if cy_worker_thread_enqueue fails. In-case there are receive events then returning fail from here
                 * will mislead application not to receive the data that is available to read.
                 */
            }
        }
        else
        {
            accept_ctx->disconnect_event_cache = true;
        }
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_accept End\r\n");
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_socket_shutdown(cy_socket_t handle, int how)
{
    uint8_t shut_rx = 0, shut_tx = 0;
    cy_socket_ctx_t * ctx;
    err_t result;

    if(CY_SOCKET_INVALID_HANDLE == handle)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    ctx = (cy_socket_ctx_t *) handle;

    if(!is_socket_valid(ctx))
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "invalid handle\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_INVALID_SOCKET;
    }

    /* While this function is running, application may delete the socket. Protect entire function with a mutex. */
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex locked %s %d\r\n", __FILE__, __LINE__);
    cy_rtos_get_mutex(&ctx->socket_mutex, CY_RTOS_NEVER_TIMEOUT);

    if(NETCONNTYPE_GROUP(netconn_type(ctx->conn_handler)) != NETCONN_TCP)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_socket_shutdown call not applicable for UDP \r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return CY_RSLT_MODULE_SECURE_SOCKETS_BADARG;
    }

    if((how & CY_SOCKET_SHUT_RD) == CY_SOCKET_SHUT_RD)
    {
        shut_rx = 1;
    }

    if((how & CY_SOCKET_SHUT_WR) == CY_SOCKET_SHUT_WR)
    {
        shut_tx = 1;
    }

    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_socket_shutdown API with RX: [%d] and TX: [%d] \r\n", shut_rx, shut_tx);

    /* Call wifi-mw-core network activity function to resume the network stack. */
    cy_network_activity_notify(CY_NETWORK_ACTIVITY_TX);

    result = netconn_shutdown(ctx->conn_handler, shut_rx, shut_tx);
    if(result != ERR_OK)
    {
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "netconn shutdown API failed\r\n");

        cy_rtos_set_mutex(&ctx->socket_mutex);
        ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

        return LWIP_TO_CY_SECURE_SOCKETS_ERR(result);
    }

    cy_rtos_set_mutex(&ctx->socket_mutex);
    ss_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "socket_mutex unlocked %s %d\n", __FILE__, __LINE__);

    return CY_RSLT_SUCCESS;
}
