/*
 * WHD TKO Offload API.
 *
 ********************************************************************************
 * \copyright
 * Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
 * You may use this file only in accordance with the license, terms, conditions,
 * disclaimers, and limitations in the end user license agreement accompanying
 * the software package with which this file was provided.
 *******************************************************************************/


#ifndef WHD_TKO_API_H__
#define WHD_TKO_API_H__  (1)

#include "cy_lpa_wifi_tko_ol.h"
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(OLM_NO_HARDWARE)
#define USE_HW
#endif

#if defined(__MBED__)
#define IP_ADDR_STATS(ipaddr)   (ipaddr.addr)
#else
#define IP_ADDR_STATS(ipaddr)   (ipaddr.u_addr.ip4.addr)
#endif

/** Get TCP socket sequence number info from network stack.  */
typedef struct sock_seq
{
    whd_mac_t src_mac; /**< local mac address */
    whd_mac_t dst_mac; /**< remote mac address */
    uint32_t srcip;    /**< local IP address */
    uint32_t dstip;    /**< destination IP address */
    uint16_t srcport;  /**< Local TCP port */
    uint16_t dstport;  /**< Remote TCP port */
    uint32_t seqnum;   /**< Current sequence number for this socket */
    uint32_t acknum;   /**< Current ack number for this socket */
    uint16_t rx_window; /**< Current TCP rx window size */
} sock_seq_t;

whd_result_t
sock_stats(sock_seq_t *seq, uint16_t local_port, uint16_t remote_port, const char *remote_ip);

whd_result_t
whd_tko_activate(whd_t *whd, uint8_t index, uint16_t local_port, uint16_t remote_port, const char *remote_ip);

whd_result_t
whd_tko_enable(whd_t *whd);

whd_result_t
whd_tko_disable(whd_t *whd);

#ifdef __cplusplus
}
#endif

#endif /* !WHD_TKO_API_H__ */

