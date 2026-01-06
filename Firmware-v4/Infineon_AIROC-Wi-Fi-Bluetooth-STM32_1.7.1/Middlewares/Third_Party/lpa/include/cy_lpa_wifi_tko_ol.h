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


#ifndef PF_TKO_H__
#define PF_TKO_H__  (1)

#include <stdint.h>
#include "cy_lpa_compat.h"

#if !defined(OLM_NO_HARDWARE)
#include "whd.h"
#include "whd_wlioctl.h"
#define MAX_TKO MAX_TKO_CONN
#else
#define MAX_TKO 4
#endif

#ifdef __cplusplus
extern "C" {
#endif


/** User uses configurator to set these */
typedef struct cy_tko_connect
{
    uint16_t    local_port;         /**< Local port num for this socket */
    uint16_t    remote_port;        /**< Remote port num for this socket */
    char        remote_ip[16];      /**< IP address of remote side */
} cy_tko_ol_connect_t;

/** User uses configurator to set these */
typedef struct cy_tko_cfg
{
    uint16_t interval;                  /**< Interval (in seconds) between keepalives */
    uint16_t retry_interval;            /**< If a keepalive is not Acked, retry after this many seconds */
    uint16_t retry_count;               /**< Retry up to this many times */
    cy_tko_ol_connect_t ports[MAX_TKO]; /**< Port and IP address ofr each connection */
} cy_tko_ol_cfg_t;

/** Keep pointers to config space, system handle, etc */
typedef struct tko
{
    cy_tko_ol_cfg_t   *cfg;   /**< Pointer to config space */
    void             *whd;   /**< Pointer to system handle */
} tko_ol_t;

/** \} */

extern const ol_fns_t tko_ol_fns;
int cylpa_tko_ol_update_config(const char *remote_ip, uint16_t remote_port, uint16_t local_port, cy_tko_ol_cfg_t *cfg );

#ifdef __cplusplus
}
#endif

#endif /* !TKO_OL_H__ */

