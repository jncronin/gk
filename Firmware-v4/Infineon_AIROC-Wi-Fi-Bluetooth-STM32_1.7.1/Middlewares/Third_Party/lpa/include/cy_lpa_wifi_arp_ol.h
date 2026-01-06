/***************************************************************************//**
* \file cy_lpa_wifi_arp_ol.h
* \version 1.0
*
* \brief
* Low Power Offload ARP Assist.
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#ifndef ARP_OL_H__
#define ARP_OL_H__  (1)

#include "cy_lpa_wifi_ol_common.h"


/* We have a non-hardware build that we need to handle - "OLM_NO_HARDWARE" defined in app/olm-sanity/Makefile */
#if defined(OLM_NO_HARDWARE)
#include <stdint.h>

/* Defines so main.c can build for building without hardware */
#ifndef ARP_MULTIHOMING_MAX
#define ARP_MULTIHOMING_MAX     (8)
#endif
#ifndef ARP_OL_AGENT
#define ARP_OL_AGENT            (0x00000001)
#endif
#ifndef ARP_OL_SNOOP
#define ARP_OL_SNOOP            (0x00000002)
#endif
#ifndef ARP_OL_HOST_AUTO_REPLY
#define ARP_OL_HOST_AUTO_REPLY  (0x00000004)
#endif
#ifndef ARP_OL_PEER_AUTO_REPLY
#define ARP_OL_PEER_AUTO_REPLY  (0x00000008)
#endif

struct arp_ol_stats_t
{
    uint32_t host_ip_entries;
    uint32_t host_ip_overflow;
    uint32_t arp_table_entries;
    uint32_t arp_table_overflow;
    uint32_t host_request;
    uint32_t host_reply;
    uint32_t host_service;
    uint32_t peer_request;
    uint32_t peer_request_drop;
    uint32_t peer_reply;
    uint32_t peer_reply_drop;
    uint32_t peer_service;
};

#else

/* We have hardware here */
#include "whd.h"
#include "whd_wlioctl.h"

#endif /* defined(OLM_NO_HARDWARE) */

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
* Public definitions
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_macros_arp *//** \{ */
/******************************************************************************/

/* For internal testing */
#if defined(MBED_CONF_APP_OLM_TEST)
/* 0 means do not use callback */
extern uint32_t arp_ol_test_enable_net_callback;
#endif

#define ARP_ID                              (0xaaccooddUL)          /**< Unique ID for ARP packet */

#define CY_ARP_OL_HOSTIP_MAX_ENTRIES        ARP_MULTIHOMING_MAX     /**< Maximum Number of Host IP addresses in WLAN Device (8) */

#define CY_ARP_OL_DEFAULT_PEERAGE_VALUE     (1200UL)                /**< Default lifetime of an ARP table entry in seconds (agent only). */

#define CY_ARP_OL_AGENT_ENABLE              ARP_OL_AGENT            /**< Flag to enable the ARP Offload          (0x00000001) */
#define CY_ARP_OL_SNOOP_ENABLE              ARP_OL_SNOOP            /**< Flag to enable Snooping Host IP address (0x00000002) */
#define CY_ARP_OL_HOST_AUTO_REPLY_ENABLE    ARP_OL_HOST_AUTO_REPLY  /**< Flag to enable Host Auto Reply          (0x00000004) */
#define CY_ARP_OL_PEER_AUTO_REPLY_ENABLE    ARP_OL_PEER_AUTO_REPLY  /**< Flag to enable Peer Auto Reply          (0x00000008) */
#define CY_ARP_OL_ALL_FEATURE_ENABLE        (CY_ARP_OL_AGENT_ENABLE | CY_ARP_OL_SNOOP_ENABLE | \
                                             CY_ARP_OL_HOST_AUTO_REPLY_ENABLE | CY_ARP_OL_PEER_AUTO_REPLY_ENABLE)  /**< All ARP OL Flags (0x0000000F) */


typedef uint32_t arp_ol_enable_mask_t;      /**< ARP OL Feature is enabled when bit is set (see CY_ARP_OL_XXXX_ENABLE flags) */

/** \} */

/*******************************************************************************
* LPA Enumerated Types
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_enums *//** \{ */
/******************************************************************************/

/** States for configuration to use */
typedef enum
{
    ARP_OL_STATE_UNINITIALIZED = 0,     /**< ARP OL is Uninitialized         */
    ARP_OL_STATE_AWAKE,                 /**< ARP OL is set for AWAKE state   */
    ARP_OL_STATE_GOING_TO_SLEEP         /**< ARP OL is set for ASLEEP state  */
} arp_ol_config_state_t;

/** \} */


/*******************************************************************************
* LPA Data Structures
*******************************************************************************/

/******************************************************************************/
/** \addtogroup group_lpa_structures *//** \{ */
/******************************************************************************/

/** ARP Offload configuration.
 * NOTE: There may be multiple configurations - one per BSS interface
 * WWD_STA_INTERFACE, WWD_AP_INTERFACE, WWD_P2P_INTERFACE, WWD_ETHERNET_INTERFACE */
typedef struct arp_ol_cfg_s
{
    arp_ol_enable_mask_t awake_enable_mask; /**< Enable mask for AWAKE state \ref ARP_OL_STATE_AWAKE */
    arp_ol_enable_mask_t sleep_enable_mask; /**< Enable mask for SLEEP state \ref ARP_OL_STATE_GOING_TO_SLEEP */
    uint32_t peerage;                       /**< ARP table entry expiration time in seconds (Device ARP cache) \ref CY_ARP_OL_DEFAULT_PEERAGE_VALUE */
} arp_ol_cfg_t;

/** ARP Offload context - this is a private structure; visible to allow for static definition. */
typedef struct arp_ol_s
{
    char name[4];                               /**< ARP */
    const arp_ol_cfg_t      *config;            /**< pointer to configuration from Configurator \ref arp_ol_cfg_t */
    ol_info_t               *ol_info_ptr;       /**< Offload Manager Info structure  \ref ol_info_t */
    uint32_t ip_address;                        /**< IP Address for this interface */
    arp_ol_config_state_t state;                /**< Currently written state in Device \ref arp_ol_config_state_t */
} arp_ol_t;

/** \} */

extern const ol_fns_t arp_ol_fns;   /**< ARP Offload function table */

/**@} */

#ifdef __cplusplus
}
#endif

#endif     /* !ARP_OL_H__ */

