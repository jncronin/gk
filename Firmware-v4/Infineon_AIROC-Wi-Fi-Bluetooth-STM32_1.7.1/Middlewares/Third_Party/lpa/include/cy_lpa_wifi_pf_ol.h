/***************************************************************************//**
* \file cy_lpa_wifi_pf_ol.h
* \version 1.0
*
* \brief
* Defines the API into PktFilter offloads from personality.
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#ifndef PF_OL_H__
#define PF_OL_H__  (1)

#include <stdint.h>
#include "cy_lpa_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* LPA Enumerated Types
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_enums *//** \{ */
/******************************************************************************/

/** Packets may be filtered according to TCP/UDP Port number, ethertype or IP type */
typedef enum cy_pf_feature
{
    CY_PF_OL_FEAT_PORTNUM    = 1,        /**< Filter based on port numbers */
    CY_PF_OL_FEAT_ETHTYPE    = 2,        /**< Filter based on ethernet_type */
    CY_PF_OL_FEAT_IPTYPE     = 3,        /**< Filter based on IP type */
                                         /**< Add new filter types here. do not alter previous types (breaks backward compatibilty) */
    CY_PF_OL_FEAT_LAST       = 4,        /**< Number of offload features (invalid feature id). */
} cy_pf_feature_t;

/** Port numbers are a feature of both TCP and UDP protocols. Each filter can only support one or the other.  */
typedef enum cy_pn_proto
{
    CY_PF_PROTOCOL_UDP        = 1,        /**< UDP protocol */
    CY_PF_PROTOCOL_TCP        = 2,        /**< TCP protocol */
} cy_pf_proto_t;

/** Source or Dest Port */
typedef enum cy_pn_direction
{
    PF_PN_PORT_DEST        = 1,        /**< Filter Destination Port*/
    PF_PN_PORT_SOURCE      = 2,        /**< Filter Source Port */
} cy_pn_direction_t;
/** \} */


/*******************************************************************************
* LPA Data Structures
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_structures *//** \{ */
/******************************************************************************/

/** Usually a single port is filtered but port ranges are also supported, where the range is from portnum thru portnum + range.
 * Port ranges are expected to be used with short lived ephemeral source port numbers.
 * Port numbers are described in numerous places such https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers
 */
typedef struct cy_pn_port
{
    uint16_t portnum;            /**< Port number.  Any 16 bit port number can be specified.
                                     For a description and list of port numbers see: https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers
                                  */
    uint16_t range;               /**< Range: Allows a block of portnumbers to be filtered.
                                   *   Range must be of the form 2^y - 1 (511, 1023, etc) and must be less than portnum.
                                   *   When using range, portnum must be of the form 2^x (256, 512, etc).
                                   *   Example: portnum == 16384, range = 1023 will filter ports 16384-17407
                                   *
                                   *   If any of these checks for using ranges fails, fallback to filtering for just 'portnum'.
                                   */

    cy_pn_direction_t direction; /**< Source or Destination port. Dest is default unless source port override is on */
} cy_pf_port_t;

/** Describes a port number filter (type CY_PF_OL_FEAT_PORTNUM) */
typedef struct cy_pf_pn_cfg
{
    cy_pf_port_t portnum;        /**< Port number info */
    cy_pf_proto_t proto;           /**< Protocol info */
} cy_pf_pn_cfg_t;

/** Describes an ethertype filter (type CY_PF_OL_FEAT_ETHTYPE) */
typedef struct cy_pf_ethtype_cfg
{
    uint16_t eth_type;        /**< 16 bit value in bytes 13 & 14 of Ethernet header. For description and list of ethertypes see https://en.wikipedia.org/wiki/EtherType */
} cy_pf_ethtype_cfg_t;

/** Describes an IP type filter (type CY_PF_OL_FEAT_IPTYPE) */
typedef struct cy_pf_ip_cfg
{
    uint8_t ip_type;           /**< 8 bit value in byte 10 of ipv4 header. For a list of IP protocol numbers see https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers */
} cy_pf_ip_cfg_t;

/** Single union to describe all packet filters */
typedef struct cy_pf_ol_cfg
{
    cy_pf_feature_t feature;                /**< Type of filter */

#define CY_PF_ACTIVE_SLEEP       (1 << 0)     /**< Filter is active only when Host is asleep */
#define CY_PF_ACTIVE_WAKE        (1 << 1)     /**< Filter is active only when Host is awake */
#define CY_PF_ACTION_DISCARD     (1 << 2)     /**< Packets that match filter are dropped and NOT passed to host, else they are passed up. */

    uint32_t bits;                  /**< Various on/off options applicable to all types of packet filters */
    uint8_t id;                     /**< Each filter is fiven a unique 8 bit identifier. */

    union
    {
        cy_pf_pn_cfg_t pf;          /**< Port filter */
        cy_pf_ethtype_cfg_t eth;    /**< Eth_type filter */
        cy_pf_ip_cfg_t ip;          /**< IP type filter */
    } u;                            /**< Individual filter types  */
} cy_pf_ol_cfg_t;


/** Keep pointers to config space, system handle, etc */
typedef struct pf_ol
{
    cy_pf_ol_cfg_t   *cfg;   /**< Pointer to config space */
    void             *whd;   /**< Pointer to system handle */
} pf_ol_t;

/** \} */

extern const ol_fns_t pf_ol_fns;

#define IS_POWER(x) ( (x) && !(x & (x - 1) ) )
#define ETHER_ADDR_LEN      6
CYPRESS_PACKED(struct) ether_header {
    uint8_t ether_dhost[ETHER_ADDR_LEN];
    uint8_t ether_shost[ETHER_ADDR_LEN];
    uint16_t ether_type;
};

/* These fields are stored in network order */
CYPRESS_PACKED(struct) bcmudp_hdr
{
    uint16_t src_port;    /* Source Port Address */
    uint16_t dst_port;    /* Destination Port Address */
    uint16_t len;         /* Number of bytes in datagram including header */
    uint16_t chksum;      /* entire datagram checksum with pseudoheader */
};

#define IPV4_ADDR_LEN       4   /* IPV4 address length */
CYPRESS_PACKED(struct) ipv4_hdr {
    uint8_t version_ihl;      /* Version and Internet Header Length */
    uint8_t tos;              /* Type Of Service */
    uint16_t tot_len;         /* Number of bytes in packet (max 65535) */
    uint16_t id;
    uint16_t frag;            /* 3 flag bits and fragment offset */
    uint8_t ttl;              /* Time To Live */
    uint8_t prot;             /* Protocol */
    uint16_t hdr_chksum;      /* IP header checksum */
    uint8_t src_ip[IPV4_ADDR_LEN];    /* Source IP Address */
    uint8_t dst_ip[IPV4_ADDR_LEN];    /* Destination IP Address */
};

#ifdef __cplusplus
}
#endif

#endif /* !PF_OL_H__ */

