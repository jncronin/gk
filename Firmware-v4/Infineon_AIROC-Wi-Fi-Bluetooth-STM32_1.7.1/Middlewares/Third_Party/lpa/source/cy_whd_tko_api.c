/***************************************************************************//**
* \file cy_whd_tko_api.c
* \version 1.0
*
* \brief
* WHD TCP Keepalive Offload API
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#include "whd_types.h"
#include "cy_lpa_wifi_ol_common.h"
#include "cy_lpa_wifi_ol.h"
#include "cy_lpa_wifi_tko_ol.h"
#include "cy_whd_tko_api.h"
#include "whd_cdc_bdc.h"
#include "whd_network_types.h"
#include "whd_buffer_api.h"
#include "cy_nw_helper.h"
#include "whd_wlioctl.h"
#ifndef MBED_CONF_APP_OLM_NO_HW
#include "lwip/tcp.h"
#endif
#include "ip4string.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*
*  Defines
*
********************************************************************/
#define IPV4_ADDR_LEN                4      /* IPV4 address length   */

/* IPV4 field decodes */
#define IPV4_VER_SHIFT               4      /* IPV4 version shift */
#define IP_VER_4                     4      /* version number for IPV4 */
#define IPV4_MIN_HEADER_LEN          20     /* Minimum size for an IP header (no options) */
#define IPV4_VER_HL_OFFSET           0      /* version and ihl byte offset */
#define IPV4_HLEN_MASK               0x0f   /* IPV4 header length mask */
#ifndef IPV4_HLEN
#define IPV4_HLEN(ipv4_body)        (4 * ( ( (uint8_t *)(ipv4_body) )[IPV4_VER_HL_OFFSET] & IPV4_HLEN_MASK ) )
#endif
#define IP_PROTO_TCP                 6      /* TCP PROTOCOL */

/* 8bit TCP flag field */
#define TCP_FLAG_ACK                0x10
#define TCP_FLAG_PSH                0x08
#define TCP_FLAG_RST                0x04
#define TCP_FLAG_SYN                0x02
#define TCP_FLAG_FIN                0x01

#define TCP_MIN_HEADER_LEN          20
#define TCP_HDRLEN_SHIFT            4

/* Default TCP Keepalive retry parameters.  */
#define TCP_KEEPALIVE_OFFLOAD_INTERVAL_SEC       (20)
#define TCP_KEEPALIVE_OFFLOAD_RETRY_COUNT        (3)
#define TCP_KEEPALIVE_OFFLOAD_RETRY_INTERVAL_SEC (3)


/********************************************************************
*
*  Macros
*
********************************************************************/
#define IFP_TO_DRIVER(ifp)  ( ( (whd_interface_t)ifp )->whd_driver )

#define TKO_OL_DEBUG
#ifdef TKO_OL_DEBUG
#define TKO_ERROR_PRINTF(x) printf x
#define TKO_DEBUG_PRINTF(x) //printf x
#else
#define TKO_ERROR_PRINTF(x)
#define TKO_DEBUG_PRINTF(x)
#endif

#define OFFSETOF(type, member)  ( (uintptr_t)&( (type *)0 )->member )
#define RX_EVENT_FLAG (1UL << 0)
#define TX_EVENT_FLAG (1UL << 1)

#define TKO_ERROR   0xfffe

#define print_ip4(ipaddr)  \
    printf("%lu.%lu.%lu.%lu\n", (ipaddr) & 0xff, (ipaddr) >> 8 & 0xff, (ipaddr) >> 16 & 0xff, (ipaddr) >> 24 & 0xff)

/********************************************************************
 *
 *  Structures
 *
 *********************************************************************/
static const whd_event_num_t tko_events[]   = { WLC_E_TKO, WLC_E_NONE };
static uint16_t tko_offload_update_entry = 0xFF;
static uint16_t tko_state_enabled = false;
void cylpa_on_emac_activity(bool is_tx_activity);    //RX_EVENT_FLAG
extern cy_mutex_t cy_lp_mutex;

/* 10Mb/s Ethernet header  */
typedef struct
{
    uint8_t ether_dhost[ETHER_ADDR_LEN];
    uint8_t ether_shost[ETHER_ADDR_LEN];
    uint16_t ether_type;
} whd_ether_header_t;

/* IP V4 Header */
typedef struct
{
    uint8_t version_ihl;          /* Version and Internet Header Length */
    uint8_t tos;              /* Type Of Service */
    uint16_t tot_len;         /* Number of bytes in packet (max 65535) */
    uint16_t id;
    uint16_t frag;            /* 3 flag bits and fragment offset */
    uint8_t ttl;              /* Time To Live */
    uint8_t prot;             /* Protocol */
    uint16_t hdr_chksum;      /* IP header checksum */
    uint8_t src_ip[IPV4_ADDR_LEN];    /* Source IP Address */
    uint8_t dst_ip[IPV4_ADDR_LEN];    /* Destination IP Address */
} ipv4_hdr_t;

/* TCP Header... used for checksumming */
typedef struct tcp_pseudo_hdr
{
    uint8_t src_ip[IPV4_ADDR_LEN]; /* Source IP Address */
    uint8_t dst_ip[IPV4_ADDR_LEN]; /* Destination IP Address */
    uint8_t zero;
    uint8_t prot;
    uint16_t tcp_size;
} tcp_pseudo_hdr_t;

/* These fields are stored in network order */
typedef struct bcmtcp_hdr
{
    uint16_t src_port;        /* Source Port Address */
    uint16_t dst_port;        /* Destination Port Address */
    uint32_t seq_num;         /* TCP Sequence Number */
    uint32_t ack_num;         /* TCP Sequence Number */
    uint16_t hdrlen_rsvd_flags;       /* Header length, reserved bits and flags */
    uint16_t tcpwin;          /* TCP window */
    uint16_t chksum;          /* Segment checksum with pseudoheader */
    uint16_t urg_ptr;         /* Points to seq-num of byte following urg data */
}bcmtcp_hdr_t;

/*********************************************************************************************
*
*  Forward Declarations
*
*********************************************************************************************/
#if LWIP_TCP
static uint16_t ipv4_hdr_cksum(uint8_t *ip, int ip_len);
static uint32_t ip_cksum_partial(uint32_t sum, uint8_t *val8, uint32_t count);
static uint16_t ip_cksum(uint32_t sum, uint8_t *val8, uint32_t count);
static uint16_t ipv4_tcp_hdr_cksum(uint8_t *ip, uint8_t *tcp, uint16_t tcp_len);
static uint16_t tcp_hdr_chksum(uint32_t sum, uint8_t *tcp_hdr, uint16_t tcp_len);
static int tko_connect_init(wl_tko_connect_t *connect, sock_seq_t *keep_alive_offload, uint8_t index);
ssize_t etharp_find_addr(struct netif *netif, const ip4_addr_t *ipaddr, wl_ether_addr_t **eth_ret,
                         const ip4_addr_t **ip_ret);
static int prep_packet(sock_seq_t *seq, int index, uint8_t *buf);
#endif
void *whd_callback_handler(whd_interface_t ifp, const whd_event_header_t *event_header, const uint8_t *event_data,
                           /*@null@*/ void *handler_user_data);

whd_result_t
whd_tko_enable(whd_t *whd)
{
    whd_result_t ret = WHD_SUCCESS;
    whd_event_handler_t handler = whd_callback_handler;

    ret =  whd_tko_toggle(whd, WHD_TRUE);

    /* Enable WLE_E_KTO Event */
    whd_management_set_event_handler(whd, tko_events, handler, (void *)whd, &tko_offload_update_entry);
    tko_state_enabled = WHD_TRUE;
    return ret;
}

whd_result_t
whd_tko_disable(whd_t *whd)
{
    if (tko_state_enabled)
    {
        tko_state_enabled = WHD_FALSE;

        /* Disable WLC_E_TKO Event */
        whd_wifi_deregister_event_handler(whd, tko_offload_update_entry);
    }

    return whd_tko_toggle(whd, WHD_FALSE);
}

/*
 * Host is going into sleep mode, activate TKO in FW.
 *
 * For each connection {
 *   1 Query the host TCP stack to get current sequence numbers for each connection.
 *   2 Use this info to fill in FW ioctl struct.
 *   3 XXX How to get local and remote mac addresses???
 *   4 Call ioctl to configure.
 * }
 * Enable TKO feature
 *
 */
whd_result_t
whd_tko_activate(whd_t *whd, uint8_t index, uint16_t local_port, uint16_t remote_port, const char *remote_ip)
{
    whd_result_t result = WHD_SUCCESS;
#if LWIP_TCP
    uint32_t len = 0;
    uint8_t *data = NULL;
    wl_tko_t *tko = NULL;
    wl_tko_connect_t *connect = NULL;
    sock_seq_t seq;
    const ip4_addr_t *ip_ret;
    wl_ether_addr_t *eth_ret;

    int tko_len;
    whd_buffer_t buffer;

    /* Get required Sequence and Ack numbers from TCP stack */
    memset(&seq, 0, sizeof(seq) );
    if (sock_stats(&seq, local_port, remote_port, remote_ip) != WHD_SUCCESS)
    {
        TKO_DEBUG_PRINTF( ("%s: Can't find socket: index[%d] %s Local Port %d, Remote Port %d\n", __func__, index,
                           remote_ip, local_port, remote_port) );
        return WHD_BADARG;
    }

    /* Local mac address */
    result =  whd_wifi_get_mac_address(whd, &seq.src_mac);
    if (result != WHD_SUCCESS)
    {
        TKO_ERROR_PRINTF( ("get_mac_address failed\n") );
        return WHD_BADARG;
    }

    /* Remote mac address */
    if (etharp_find_addr(NULL, (const ip4_addr_t * )&seq.dstip, &eth_ret, &ip_ret) >= 0)
    {
        if (ip_ret->addr != seq.dstip)
        {
            TKO_ERROR_PRINTF( ("%s: Hey orig IP and LWIP IP don't match!\n", __func__) );
        }
        memcpy(seq.dst_mac.octet, eth_ret, sizeof(seq.dst_mac.octet) );
    }
    else
    {
        TKO_DEBUG_PRINTF( ("%s: Remote mac addr not found, using bssid\n", __func__) );
        result = whd_wifi_get_bssid(whd, &seq.dst_mac);
        if (result != WHD_SUCCESS)
        {
            TKO_ERROR_PRINTF( ("%s: get_bssid failed\n", __func__) );
            return WHD_BADARG;
        }
    }

    TKO_DEBUG_PRINTF( ("Local Mac Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       seq.src_mac.octet[0], seq.src_mac.octet[1], seq.src_mac.octet[2],
                       seq.src_mac.octet[3], seq.src_mac.octet[4], seq.src_mac.octet[5]) );
    TKO_DEBUG_PRINTF( ("Remote Mac addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       seq.dst_mac.octet[0], seq.dst_mac.octet[1], seq.dst_mac.octet[2],
                       seq.dst_mac.octet[3], seq.dst_mac.octet[4], seq.dst_mac.octet[5]) );

    len = (int)(WHD_PAYLOAD_MTU - strlen(IOVAR_STR_TKO) - 1);
    data = (uint8_t * )whd_cdc_get_iovar_buffer(IFP_TO_DRIVER(whd), &buffer, (uint16_t)len, IOVAR_STR_TKO);

    tko = (wl_tko_t *)data;

    tko->subcmd_id = WL_TKO_SUBCMD_CONNECT;
    tko->len = offsetof(wl_tko_t, data);
    connect = (wl_tko_connect_t *)tko->data;

    tko_len = tko_connect_init(connect, &seq, index);
    if (tko_len == TKO_ERROR)
    {
        TKO_ERROR_PRINTF( ("%s: tko_connect_init() failed\n", __func__) );
        return tko_len;
    }

    tko->len += tko_len;
    len = offsetof(wl_tko_t, data) + tko->len;

    tko->subcmd_id = htod16(tko->subcmd_id);
    tko->len = htod16(tko->len);

    /* invoke SET iovar */
    result = whd_cdc_send_iovar(whd, CDC_SET, buffer, NULL);
    if (result != WHD_SUCCESS)
    {
        TKO_ERROR_PRINTF( ("%s: tko CONNECT subcmd IOVAR failed. Result: %u\n", __func__, (unsigned int)result) );
    }
#endif /* LWIP_TCP */
    return result;
}

/* Given a pair of port and ip addresses, return sequence numbers
 * and other low level info about a connection from network stack.
 */
whd_result_t
sock_stats(sock_seq_t *seq, uint16_t local_port, uint16_t remote_port, const char *remote_ip)
{
#if LWIP_TCP
    extern struct tcp_pcb *tcp_bound_pcbs;
    extern struct tcp_pcb *tcp_active_pcbs;
    extern struct tcp_pcb *tcp_tw_pcbs;
    struct tcp_pcb **const my_tcp_pcb_lists[] = {&tcp_bound_pcbs, &tcp_active_pcbs, &tcp_tw_pcbs };
    int i;
    struct tcp_pcb *list;

    memset(seq, 0, sizeof(sock_seq_t) );
    for  (i = 0; i < (int)(sizeof(my_tcp_pcb_lists) / sizeof(my_tcp_pcb_lists[0]) ); i++)
    {
        list = *my_tcp_pcb_lists[i];
        while (list)
        {
            uint32_t tmp_ip;
            stoip4(remote_ip, strlen(remote_ip), &tmp_ip);
            if ((list->local_port == local_port) && (list->remote_port == remote_port) && (IP_ADDR_STATS(list->remote_ip) == tmp_ip) )
            {
                seq->srcip = IP_ADDR_STATS(list->local_ip);
                seq->dstip = IP_ADDR_STATS(list->remote_ip);
                seq->srcport = list->local_port;
                seq->dstport = list->remote_port;
                seq->seqnum = list->snd_nxt;
                seq->acknum = list->rcv_nxt;
                seq->rx_window = list->rcv_wnd;
                return WHD_SUCCESS;
            }
            list = list->next;
        }
    }
#endif /* LWIP_TCP */
    return WHD_BADARG;
}

void *whd_callback_handler(whd_interface_t ifp, const whd_event_header_t *event_header, const uint8_t *event_data,
                           /*@null@*/ void *handler_user_data)
{
    cy_rtos_get_mutex(&cy_lp_mutex, CY_RTOS_NEVER_TIMEOUT);
    TKO_ERROR_PRINTF( ("%s: Entered \n", __func__) );
    if (event_header != NULL)
    {
        switch (event_header->event_type)
        {
            case WLC_E_TKO:
                if (event_data != NULL)
                {
                    tko_state_enabled = WHD_FALSE;
                    cylpa_on_emac_activity(false);
                }
                break;
            default:
                break;
        } /* end of switch */
    }

    TKO_ERROR_PRINTF( ("%s: Exit \n", __func__) );
    cy_rtos_set_mutex(&cy_lp_mutex);
    return handler_user_data;
}

#ifdef LWIP_TCP
/* Prepare request packet */
static int
prep_packet(sock_seq_t *seq, int index, uint8_t *buf)
{
    whd_ether_header_t *eth;
    ipv4_hdr_t *ip;
    bcmtcp_hdr_t *tcp;

    if (buf == NULL)
    {
        TKO_ERROR_PRINTF( ("%s: ERROR: NULL buf\n", __func__) );
        return TKO_ERROR;
    }

    eth = (whd_ether_header_t *)buf;

    /* Copy mac HW addresses */
    memcpy(eth->ether_dhost, seq->dst_mac.octet, ETHER_ADDR_LEN);
    memcpy(eth->ether_shost, seq->src_mac.octet, ETHER_ADDR_LEN);

    TKO_DEBUG_PRINTF( ("dhost : %02X:%02X:%02X:%02X:%02X:%02X\n", eth->ether_dhost[0], eth->ether_dhost[1],
                       eth->ether_dhost[2], eth->ether_dhost[3], eth->ether_dhost[4], eth->ether_dhost[5]) );
    TKO_DEBUG_PRINTF( ("shost : %02X:%02X:%02X:%02X:%02X:%02X\n", eth->ether_shost[0], eth->ether_shost[1],
                       eth->ether_shost[2], eth->ether_shost[3], eth->ether_shost[4], eth->ether_shost[5]) );

    eth->ether_type = hton16(ETHER_TYPE_IP);
    ip = (ipv4_hdr_t *)(buf + sizeof(whd_ether_header_t) );

    memcpy(ip->dst_ip, &seq->dstip, IPV4_ADDR_LEN);
    memcpy(ip->src_ip, &seq->srcip, IPV4_ADDR_LEN);

    ip->version_ihl = (IP_VER_4 << IPV4_VER_SHIFT) | (IPV4_MIN_HEADER_LEN / 4);
    ip->tos = 0;
    ip->tot_len = hton16(sizeof(ipv4_hdr_t) + sizeof(bcmtcp_hdr_t) );
    ip->id = hton16(0);
    ip->frag = 0;
    ip->ttl = 32;
    ip->prot = IP_PROTO_TCP;
    ip->hdr_chksum = 0;

    ip->hdr_chksum = ipv4_hdr_cksum( (uint8_t *)ip, IPV4_HLEN(ip) );

    tcp = (bcmtcp_hdr_t *)(buf + sizeof(whd_ether_header_t) +
                           sizeof(ipv4_hdr_t) );
    tcp->src_port = hton16(seq->srcport);
    tcp->dst_port = hton16(seq->dstport);
    tcp->seq_num  = hton32(seq->seqnum);
    tcp->ack_num  = hton32(seq->acknum);
    tcp->tcpwin   = hton16(seq->rx_window);
    tcp->chksum   = 0;
    tcp->urg_ptr  = 0;
    tcp->hdrlen_rsvd_flags = (TCP_FLAG_ACK << 8) |
                             ( (sizeof(bcmtcp_hdr_t) >> 2) << TCP_HDRLEN_SHIFT );

    /* calculate TCP header checksum */
    tcp->chksum = ipv4_tcp_hdr_cksum( (uint8_t *)ip, (uint8_t *)tcp, sizeof(*tcp) );
    if (tcp->chksum == TKO_ERROR)
    {
        TKO_ERROR_PRINTF( ("%s ERROR, Bad checksum 0x%x\n", __func__, tcp->chksum) );
        return TKO_ERROR;
    }

    TKO_DEBUG_PRINTF( ("%s Checksum Success 0x%x\n", __func__, tcp->chksum) );
    return (sizeof(whd_ether_header_t) + sizeof(ipv4_hdr_t) + sizeof(bcmtcp_hdr_t) );
}

static int
tko_connect_init(wl_tko_connect_t *connect, sock_seq_t *keep_alive_offload, uint8_t index)
{
    int datalen = 0;
    uint32_t srcip;
    uint32_t destip;

    connect->index = index;
    connect->ip_addr_type = 0;
    connect->local_port  = htod16(keep_alive_offload->srcport);
    connect->remote_port = htod16(keep_alive_offload->dstport);
    connect->local_seq   = htod32(keep_alive_offload->seqnum);
    connect->remote_seq  = htod32(keep_alive_offload->acknum);

    destip = keep_alive_offload->dstip;
    srcip = keep_alive_offload->srcip;
    memcpy(&connect->data[datalen], &srcip, sizeof(keep_alive_offload->srcip) );
    datalen += sizeof(keep_alive_offload->srcip);
    memcpy(&connect->data[datalen], &destip, sizeof(keep_alive_offload->dstip) );
    datalen += sizeof(keep_alive_offload->dstip);

    /* Keepalive uses previous sequence number so its not confused with a real packet */
    keep_alive_offload->seqnum = keep_alive_offload->seqnum - 1;

    //TKO_DEBUG_PRINTF(("_init: keep alive Seq num:%lu\n",    keep_alive_offload->seqnum ));
    //TKO_DEBUG_PRINTF(("_init: keep alive Ack num:%lu\n",    keep_alive_offload->acknum ));
    //TKO_DEBUG_PRINTF(("_init: keep alive Rcv Window:%u\n", keep_alive_offload->rx_window ));

    /* Create Request Packet */
    connect->request_len = prep_packet(keep_alive_offload, index, &connect->data[datalen]);
    if (connect->request_len == TKO_ERROR)
    {
        TKO_ERROR_PRINTF( ("%s: Creating Request FAILED\n", __func__) );
        return TKO_ERROR;
    }
    datalen += connect->request_len;
    keep_alive_offload->seqnum = keep_alive_offload->seqnum + 1;

    /* Create Response Packet */
    connect->response_len = prep_packet(keep_alive_offload, index, &connect->data[datalen]);
    if (connect->request_len == TKO_ERROR)
    {
        TKO_ERROR_PRINTF( ("%s: Creating Response FAILED\n", __func__) );
        return TKO_ERROR;
    }
    datalen += connect->response_len;

    return (offsetof(wl_tko_connect_t, data) + datalen);
}

/* calculate IPv4 header checksum
 * - input ip points to IP header in network order
 * - output cksum is in network order
 */
static uint16_t ipv4_hdr_cksum(uint8_t *ip, int ip_len)
{
    uint32_t sum = 0;
    uint8_t *ptr = ip;

    if ( (ip == NULL) || (ip_len < IPV4_MIN_HEADER_LEN) )
    {
        TKO_ERROR_PRINTF( ("%s: Error\n", __func__) );
        return TKO_ERROR;
    }

    /* partial cksum skipping the hdr_chksum field */
    sum = ip_cksum_partial(sum, ptr, OFFSETOF(ipv4_hdr_t, hdr_chksum) );
    ptr += OFFSETOF(ipv4_hdr_t, hdr_chksum) + 2;

    /* return calculated chksum */
    return ip_cksum(sum, ptr, ip_len - OFFSETOF(ipv4_hdr_t, src_ip) );
}

/* calculate partial checksum */
static uint32_t ip_cksum_partial(uint32_t sum, uint8_t *val8, uint32_t count)
{
    uint32_t i;
    uint16_t *val16 = (uint16_t *)val8;

    if ( (val8 == NULL) || ( (count % 2) != 0 ) )
    {
        TKO_ERROR_PRINTF( ("%s: ERROR!\n", __func__) );
        return TKO_ERROR;
    }

    count /= 2;

    for (i = 0; i < count; i++)
    {
        sum += *val16++;
    }
    return sum;
}

/* calculate IP checksum */
static uint16_t ip_cksum(uint32_t sum, uint8_t *val8, uint32_t count)
{
    uint16_t *val16 = (uint16_t * )val8;

    if (val8 == NULL)
    {
        TKO_ERROR_PRINTF( ("%s: ERROR!\n", __func__) );
        return TKO_ERROR;
    }

    while (count > 1)
    {
        sum += *val16++;
        count -= 2;
    }

    /*  add left-over byte, if any */
    if (count > 0)
    {
        sum += (*(uint8_t *)val16);
    }

    /*  fold 32-bit sum to 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ( (uint16_t) ~sum );
}

/* calculate IPv4 TCP header checksum
 * - input ip and tcp points to IP and TCP header in network order
 * - output cksum is in network order
 */
static uint16_t ipv4_tcp_hdr_cksum(uint8_t *ip, uint8_t *tcp, uint16_t tcp_len)
{
    ipv4_hdr_t *ip_hdr = (ipv4_hdr_t *)ip;
    tcp_pseudo_hdr_t tcp_ps;
    uint32_t sum = 0;

    if (tcp_len < TCP_MIN_HEADER_LEN)
    {
        TKO_ERROR_PRINTF( ("%s: ERROR!\n", __func__) );
        return TKO_ERROR;
    }

    /* pseudo header cksum */
    memset(&tcp_ps, 0, sizeof(tcp_ps) );
    memcpy(&tcp_ps.dst_ip, ip_hdr->dst_ip, IPV4_ADDR_LEN);
    memcpy(&tcp_ps.src_ip, ip_hdr->src_ip, IPV4_ADDR_LEN);
    tcp_ps.zero = 0;
    tcp_ps.prot = ip_hdr->prot;
    tcp_ps.tcp_size = hton16(tcp_len);
    sum = ip_cksum_partial(sum, (uint8_t *)&tcp_ps, sizeof(tcp_ps) );

    /* return calculated TCP header chksum */
    return tcp_hdr_chksum(sum, tcp, tcp_len);
}

/* calculate TCP header checksum using partial sum */
static uint16_t tcp_hdr_chksum(uint32_t sum, uint8_t *tcp_hdr, uint16_t tcp_len)
{
    uint8_t *ptr = tcp_hdr;

    if (tcp_len < TCP_MIN_HEADER_LEN)
    {
        TKO_ERROR_PRINTF( ("%s: ERROR!\n", __func__) );
        return TKO_ERROR;
    }

    /* partial TCP cksum skipping the chksum field */
    sum = ip_cksum_partial(sum, ptr, offsetof(bcmtcp_hdr_t, chksum) );
    ptr += offsetof(bcmtcp_hdr_t, chksum) + 2;

    /* return calculated chksum */
    return ip_cksum(sum, ptr, tcp_len - offsetof(bcmtcp_hdr_t, urg_ptr) );
}
#endif /* LWIP_TCP */


#ifdef __cplusplus
}
#endif

