/***************************************************************************//**
* \file cy_lpa_wifi_pf_ol.c
* \version 1.0
*
* \brief
* Low Power Offload Packet Filter Assist Implementation
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "string.h"
#include "cy_lpa_wifi_ol_debug.h"
#include "cy_lpa_wifi_ol.h"
#include "cy_lpa_wifi_ol_priv.h"
#include "cy_lpa_wifi_pf_ol.h"
#include "cy_lpa_wifi_result.h"

#if !defined(OLM_NO_HARDWARE)
#define USE_HW
#endif

#ifdef USE_HW
#include "lwip/ip.h"
#include "whd_sdpcm.h"
#include "whd_wifi_api.h"
#include "whd_wlioctl.h"
#include "whd_endian.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BITS_IN_PORTNUM 16  /* Portnum is 16 bits */

static ol_init_t cylpa_pf_ol_init;
static ol_deinit_t cylpa_pf_ol_deinit;
static ol_pm_t cylpa_pf_ol_pm;

const ol_fns_t pf_ol_fns =
{
    .init = cylpa_pf_ol_init,
    .deinit = cylpa_pf_ol_deinit,
    .pm = cylpa_pf_ol_pm,
};


/*******************************************************************************
* Function Prototypes
*******************************************************************************/

static int cylpa_create_portnum_range_mask(cy_pf_ol_cfg_t *pf_cfg, uint16_t *pattern, uint16_t *mask);
static int cylpa_create_single_portnum_mask(cy_pf_ol_cfg_t *pf_cfg, uint16_t *pattern, uint16_t *mask);
static int cylpa_create_port_filter(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg, uint16_t pattern, uint16_t mask);
static int cylpa_create_ethtype_filter(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg);
static int cylpa_create_ip_filter(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg);

static int cylpa_sign_extend(int val, unsigned width);
static int cylpa_bitwidth(int val);
static int cylpa_dump_filters_stats(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg);
#ifdef TESTING
static void cylpa_run_pf_test(unsigned int pattern, unsigned int mask);
#endif

#ifdef USE_HW
static int cylpa_print_packet_filter_stats(whd_t *whd, uint8_t filter_id);
#ifdef DEBUG
static void cylpa_print_pat_and_mask(uint8_t id, int mask_len, uint8_t *mask, uint8_t *pat);
#endif
#endif


/*******************************************************************************
 * Function Name: cylpa_pf_ol_init
 ****************************************************************************//**
 *
 * Packet filter init function.
 *
 * \param ol
 * The pointer to the ol structure.
 *
 * \param info
 * The pointer to the ol_info_t structure \ref ol_info_t.
 *
 * \param cfg
 * The pointer to the configuration.
 *
 * \return
 * Returns the execution result
 *
 ********************************************************************************/
static int cylpa_pf_ol_init(void *ol, ol_info_t *info, const void *cfg)
{
    pf_ol_t *ctxt = (pf_ol_t *)ol;
    memset(ctxt, 0, sizeof(pf_ol_t) );

    ctxt->cfg  = (cy_pf_ol_cfg_t *)cfg;
    ctxt->whd  = info->whd;

    cy_pf_ol_cfg_t *pf_cfg;

    for (pf_cfg = ctxt->cfg; pf_cfg->feature != CY_PF_OL_FEAT_LAST; pf_cfg++)
    {
        switch (pf_cfg->feature)
        {
            case CY_PF_OL_FEAT_PORTNUM:
            {
                uint16_t pattern;
                uint16_t mask;
                if ( (!IS_POWER(pf_cfg->u.pf.portnum.portnum) ) || (!IS_POWER( (pf_cfg->u.pf.portnum.range + 1) ) ) ||
                     (pf_cfg->u.pf.portnum.range == 0) )
                {
                    cylpa_create_single_portnum_mask(pf_cfg, &pattern, &mask);
                    OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Creating Single Port filter ID: %d Port: %d ",
                              pf_cfg->id, pf_cfg->u.pf.portnum.portnum);
                }
                else
                {
                    cylpa_create_portnum_range_mask(pf_cfg, &pattern, &mask);
                    OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Creating Port Range filter ID: %d, Range %d - %d ", pf_cfg->id,
                              pf_cfg->u.pf.portnum.portnum, pf_cfg->u.pf.portnum.portnum + pf_cfg->u.pf.portnum.range);
                }
                cylpa_create_port_filter(ctxt->whd, pf_cfg, pattern, mask);
#ifdef TESTING
                cylpa_run_pf_test(pattern, mask);
#endif
                break;
            }
            case CY_PF_OL_FEAT_ETHTYPE:
                cylpa_create_ethtype_filter(ctxt->whd, pf_cfg);
                break;
            case CY_PF_OL_FEAT_IPTYPE:
                cylpa_create_ip_filter(ctxt->whd, pf_cfg);
                break;
            case CY_PF_OL_FEAT_LAST:
                /* Satisfy compiler, never executed */
                break;
            default:
                OL_LOG_PF(LOG_OLA_LVL_ERR, "%s: Unknown Packet Filter feature: %d\n", __func__, pf_cfg->feature);
                break;
        }
    }

#ifdef USE_HW
    /*
     * Enable any wake filters
     */
    for (pf_cfg = ctxt->cfg; pf_cfg->feature != CY_PF_OL_FEAT_LAST; pf_cfg++)
    {
        if (pf_cfg->bits & CY_PF_ACTIVE_WAKE)
        {
            if (whd_pf_enable_packet_filter(ctxt->whd, pf_cfg->id) != WHD_SUCCESS)
            {
                OL_LOG_PF(LOG_OLA_LVL_ERR, "FAILED to enable id %d\n", pf_cfg->id);
            }
            else
            {
                OL_LOG_PF(LOG_OLA_LVL_INFO, "Activating id %d for WAKE operation\n", pf_cfg->id);
            }
        }
    }
#endif /* USE_HW */

    cylpa_dump_filters_stats(ctxt->whd, ctxt->cfg);

    return RESULT_OK;
}

/*******************************************************************************
 * Function Name: cylpa_pf_ol_deinit
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param ol
 * The pointer to the ol structure.
 *
 ********************************************************************************/
static void cylpa_pf_ol_deinit(void *ol)
{
    pf_ol_t *ctxt = (pf_ol_t *)ol;
    cy_pf_ol_cfg_t *pf_cfg;

    if ((ctxt == NULL) || (ctxt->cfg == NULL) || (ctxt->whd == NULL))
    {
        return;
    }

    for (pf_cfg = ctxt->cfg; pf_cfg->feature != CY_PF_OL_FEAT_LAST; pf_cfg++)
    {
        OL_LOG_PF(LOG_OLA_LVL_INFO, "Removing filter %d\n", pf_cfg->id);
#ifdef USE_HW
        whd_result_t res = whd_pf_remove_packet_filter(ctxt->whd, pf_cfg->id);
        if (res != WHD_SUCCESS)
        {
            OL_LOG_PF(LOG_OLA_LVL_ERR, "%s: Unable to remove filter %d, result = %d\n", __func__, pf_cfg->id, res);
        }
#endif
    }
}

/*******************************************************************************
 * Function Name: cylpa_pf_ol_pm
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param ol
 * The pointer to the ol structure.
 *
 * \param st
 * see \ref ol_pm_st_t.
 *
 ********************************************************************************/
static void cylpa_pf_ol_pm(void *ol, ol_pm_st_t st)
{
    pf_ol_t *ctxt = (pf_ol_t *)ol;
    cy_pf_ol_cfg_t *pf_cfg;

    if ((ctxt == NULL) || (ctxt->cfg == NULL) || (ctxt->whd == NULL))
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "%s : Bad Args!\n", __func__);
        return;
    }
#ifdef USE_HW
    switch (st)
    {
        case OL_PM_ST_GOING_TO_SLEEP:
            OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Entering sleep\n", __func__);
            break;
        case OL_PM_ST_AWAKE:
            OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Entering wake\n", __func__);
            break;
        default:
            OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Unknown state change\n", __func__);
            break;
    }
#endif /* USE_HW */

    for (pf_cfg = ctxt->cfg; pf_cfg->feature != CY_PF_OL_FEAT_LAST; pf_cfg++)
    {
        /* If always active, then nothing to do. */
        if ( (pf_cfg->bits & (CY_PF_ACTIVE_SLEEP | CY_PF_ACTIVE_WAKE) ) == (CY_PF_ACTIVE_SLEEP | CY_PF_ACTIVE_WAKE) )
        {
            OL_LOG_PF(LOG_OLA_LVL_INFO, "%s: Filter %d is always on, nothing to do\n", __func__, pf_cfg->id);
        }
        else
        {
#ifdef USE_HW
            switch (st)
            {
                case OL_PM_ST_GOING_TO_SLEEP:
                    if (pf_cfg->bits & CY_PF_ACTIVE_SLEEP)
                    {
                        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Enabling sleep filter %d\n", __func__, pf_cfg->id);
                        if (whd_pf_enable_packet_filter(ctxt->whd, pf_cfg->id) != WHD_SUCCESS)
                        {
                            OL_LOG_PF(LOG_OLA_LVL_ERR, "%s: whd_pf_enable_packet_filter %d FAILED\n", __func__,
                                      pf_cfg->id);
                        }
                    }
                    else
                    {
                        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Disabling wake filter %d\n", __func__, pf_cfg->id);
                        if (whd_pf_disable_packet_filter(ctxt->whd, pf_cfg->id) != WHD_SUCCESS)
                        {
                            OL_LOG_PF(LOG_OLA_LVL_ERR, "%s: whd_pf_disable_packet_filter %d FAILED\n", __func__,
                                      pf_cfg->id);
                        }
                    }
                    break;
                case OL_PM_ST_AWAKE:
                    if (pf_cfg->bits & CY_PF_ACTIVE_WAKE)
                    {
                        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Enabling wake filter %d\n", __func__, pf_cfg->id);
                        if (whd_pf_enable_packet_filter(ctxt->whd, pf_cfg->id) != WHD_SUCCESS)
                        {
                            OL_LOG_PF(LOG_OLA_LVL_ERR, "%s: whd_pf_enable_packet_filter %d FAILED\n", __func__,
                                      pf_cfg->id);
                        }
                    }
                    else
                    {
                        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "%s: Disabling sleep filter %d\n", __func__, pf_cfg->id);
                        if (whd_pf_disable_packet_filter(ctxt->whd, pf_cfg->id) != WHD_SUCCESS)
                        {
                            OL_LOG_PF(LOG_OLA_LVL_ERR, "%s: whd_pf_disable_packet_filter %d FAILED\n", __func__,
                                      pf_cfg->id);
                        }
                    }
                    break;
                default:
                    OL_LOG_PF(LOG_OLA_LVL_ERR, "Unknown PM state! %d\n", st);
                    break;
            }
#endif /* USE_HW */
        }  /* if !(SLEEP && WAKE) */
    }  /* for each configuration */
}

/* The number of bytes between the start of the 'EtherType' of the ethernet
 * header, and the end of the 'Destination port' of the UDP header is 26 (PORT_FILTER_LEN). */

/* The 'EtherType' field is 12 bytes from the start of the ethernet header. */
#define ETHTYPE_OFFSET 12

#ifdef USE_HW
static uint8_t cylpa_glob_pat_buf[32];
static uint8_t cylpa_glob_mask_buf[32];
#endif

#ifdef USE_HW
/*******************************************************************************
 * Function Name: common_filter_attrs
 ****************************************************************************//**
 * Handle common attributes of all filters
 * \param pf_cfg
 * The pointer to the cy_pf_ol_cfg_t structure \ref cy_pf_ol_cfg_t.
 * \param filter
 * Pointer to actual filter
 * \return
 * Returns the execution result
 ********************************************************************************/
static void common_filter_attrs(cy_pf_ol_cfg_t *pf_cfg, whd_packet_filter_t *filter)
{
    if ( (pf_cfg->bits & (CY_PF_ACTIVE_WAKE | CY_PF_ACTIVE_SLEEP) ) == 0 )
    {
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "ID %d: Neither Sleep nor Wake specified...will not be enabled\n", pf_cfg->id);
    }
    else
    {
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, " %s%s",
                  pf_cfg->bits & CY_PF_ACTIVE_WAKE ? "Wake" : "",
                  pf_cfg->bits & CY_PF_ACTIVE_SLEEP ? "Sleep" : "");
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, ", ");
    }

    if (pf_cfg->bits & CY_PF_ACTION_DISCARD)
    {
        filter->rule = WHD_PACKET_FILTER_RULE_NEGATIVE_MATCHING;
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Discard\n");
    }
    else
    {
        filter->rule = WHD_PACKET_FILTER_RULE_POSITIVE_MATCHING;
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Keep\n");
    }
}

#endif

/*******************************************************************************
 * Function Name: cylpa_create_ip_filter
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param whd
 * The pointer to the whd interface.
 *
 * \param pf_cfg
 * The pointer to the cy_pf_ol_cfg_t structure \ref cy_pf_ol_cfg_t.
 *
 * \return
 * Returns the execution result
 *
 ********************************************************************************/
static int cylpa_create_ip_filter(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg)
{
#ifdef USE_HW
    whd_packet_filter_t filter;

    struct ether_header ether_hdr_mask;
    struct ether_header ether_hdr_pattern;
    struct ipv4_hdr ip_hdr_mask;
    struct ipv4_hdr ip_hdr_pattern;

    uint8_t *pat_ptr = cylpa_glob_pat_buf;
    uint8_t *mask_ptr = cylpa_glob_mask_buf;

    memset(&ether_hdr_pattern, 0, sizeof(ether_hdr_pattern) );
    memset(&ether_hdr_mask, 0, sizeof(ether_hdr_mask) );
    memset(&ip_hdr_pattern, 0, sizeof(ip_hdr_pattern) );
    memset(&ip_hdr_mask, 0, sizeof(ip_hdr_mask) );

    ether_hdr_pattern.ether_type = hton16(ETHER_TYPE_IP);
    ether_hdr_mask.ether_type = hton16(0xffff);

    ip_hdr_mask.prot = 0xff;
    ip_hdr_pattern.prot = pf_cfg->u.ip.ip_type;

    filter.id = pf_cfg->id;
    filter.offset = ETHTYPE_OFFSET;
    filter.mask = (uint8_t *)cylpa_glob_mask_buf;
    filter.pattern = (uint8_t *)cylpa_glob_pat_buf;
    filter.mask_size = ETHER_TYPE_LEN + sizeof(ip_hdr_pattern);

    memcpy(mask_ptr, &ether_hdr_mask.ether_type, ETHER_TYPE_LEN);
    memcpy(mask_ptr + ETHER_TYPE_LEN, &ip_hdr_mask, sizeof(ip_hdr_mask) );

    memcpy(pat_ptr, &ether_hdr_pattern.ether_type, ETHER_TYPE_LEN);
    memcpy(pat_ptr + ETHER_TYPE_LEN, &ip_hdr_pattern, sizeof(ip_hdr_pattern) );

    OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Creating IP filter %d: ", pf_cfg->id);
    common_filter_attrs(pf_cfg, &filter);

    if (whd_pf_add_packet_filter(whd, &filter) != WHD_SUCCESS)
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "Add filter %ld Failed\n", filter.id);
    }
#endif /* USE_HW */
    return 0;
}

/*******************************************************************************
 * Function Name: cylpa_create_ethtype_filter
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param whd
 * The pointer to the whd interface.
 *
 * \param pf_cfg
 * The pointer to the cy_pf_ol_cfg_t structure \ref cy_pf_ol_cfg_t.
 *
 * \return
 * Returns the execution result
 *
 ********************************************************************************/
static int cylpa_create_ethtype_filter(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg)
{
#ifdef USE_HW
    whd_packet_filter_t filter;

    struct ether_header ether_hdr_mask;
    struct ether_header ether_hdr_pattern;

    uint8_t *pat_ptr = cylpa_glob_pat_buf;
    uint8_t *mask_ptr = cylpa_glob_mask_buf;

    uint16_t eth = pf_cfg->u.eth.eth_type;

    memset(&ether_hdr_pattern, 0, sizeof(ether_hdr_pattern) );
    memset(&ether_hdr_mask, 0, sizeof(ether_hdr_mask) );

    ether_hdr_pattern.ether_type = hton16(eth);
    ether_hdr_mask.ether_type = hton16(0xffff);

    filter.id = pf_cfg->id;
    filter.offset = ETHTYPE_OFFSET;
    filter.mask = (uint8_t *)cylpa_glob_mask_buf;
    filter.pattern = (uint8_t *)cylpa_glob_pat_buf;
    filter.mask_size = ETHER_TYPE_LEN;

    OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Creating EtherType filter: ID %d", pf_cfg->id);
    common_filter_attrs(pf_cfg, &filter);

    memcpy(mask_ptr, &ether_hdr_mask.ether_type, ETHER_TYPE_LEN);
    memcpy(pat_ptr, &ether_hdr_pattern.ether_type, ETHER_TYPE_LEN);

    if (whd_pf_add_packet_filter(whd, &filter) != WHD_SUCCESS)
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "Add filter %ld Failed\n", filter.id);
    }
#endif /* USE_HW */
    return 0;
}

/*******************************************************************************
 * Function Name: cylpa_create_port_filter
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param whd
 * The pointer to the whd interface.
 *
 * \param pf_cfg
 * The pointer to the cy_pf_ol_cfg_t structure \ref cy_pf_ol_cfg_t.
 *
 * \param pattern
 * The pointer to the ol structure.
 *
 * \param mask
 * The pointer to the ol_info_t structure.
 *
 * \return
 * Returns the execution result
 *
 ********************************************************************************/
static int cylpa_create_port_filter(whd_t *whd, cy_pf_ol_cfg_t *pf_cfg, uint16_t pattern, uint16_t mask)
{
#ifndef USE_HW
    OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Creating filter for Pattern 0x%x, Mask 0x%x, Active during: %s %s\n",
              pattern, mask,
              pf_cfg->bits & CY_PF_ACTIVE_WAKE ? "Wake " : "",
              pf_cfg->bits & CY_PF_ACTIVE_SLEEP ? "Sleep " : "");
#else
    whd_packet_filter_t filter;
    struct ether_header ether_hdr_mask;
    struct ether_header ether_hdr_pattern;
    struct bcmudp_hdr udp_hdr_mask;
    struct bcmudp_hdr udp_hdr_pattern;
    struct ipv4_hdr ip_hdr_mask;
    struct ipv4_hdr ip_hdr_pattern;
    uint8_t *pat_ptr = cylpa_glob_pat_buf;
    uint8_t *mask_ptr = cylpa_glob_mask_buf;

    /* Configure pattern to match against received packets. */
    memset(&ether_hdr_pattern, 0, sizeof(ether_hdr_pattern) );
    memset(&udp_hdr_pattern, 0, sizeof(udp_hdr_pattern) );
    memset(&ip_hdr_pattern, 0, sizeof(ip_hdr_pattern) );

    /* Configure bitmask that indicates which bits of received packets to match * against specified pattern. */
    memset(&ether_hdr_mask, 0, sizeof(ether_hdr_mask) );
    memset(&udp_hdr_mask, 0, sizeof(udp_hdr_mask) );
    memset(&ip_hdr_mask, 0, sizeof(ip_hdr_mask) );

    /* Ethernet header. Validate that 'EtherType' is IP. */
    ether_hdr_pattern.ether_type = hton16(ETHER_TYPE_IP);
    ether_hdr_mask.ether_type = hton16(0xffff);

    /* IP header. Validate that 'Protocol' is UDP. */
    ip_hdr_mask.prot = 0xff;
    ip_hdr_pattern.prot = pf_cfg->u.pf.proto;

    switch (pf_cfg->u.pf.proto)
    {
        case CY_PF_PROTOCOL_UDP:
            ip_hdr_pattern.prot = IP_PROTO_UDP;
            break;
        case CY_PF_PROTOCOL_TCP:
            ip_hdr_pattern.prot = IP_PROTO_TCP;
            break;
        default:
            ip_hdr_pattern.prot = IP_PROTO_TCP;
            break;
    }

    /* Set port number pattern and mask */
    if (pf_cfg->u.pf.portnum.direction == PF_PN_PORT_SOURCE)
    {
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Source Port, ");
        udp_hdr_pattern.src_port = hton16(pattern);
        udp_hdr_mask.src_port = hton16(mask);
    }
    else
    {
        OL_LOG_PF(LOG_OLA_LVL_DEBUG, "Dest Port, ");
        udp_hdr_pattern.dst_port = hton16(pattern);
        udp_hdr_mask.dst_port = hton16(mask);
    }

    common_filter_attrs(pf_cfg, &filter);

    filter.id = pf_cfg->id;
    filter.offset = ETHTYPE_OFFSET;
    filter.mask = (uint8_t *)cylpa_glob_mask_buf;
    filter.pattern = (uint8_t *)cylpa_glob_pat_buf;
    filter.mask_size = PORT_FILTER_LEN;

    memcpy(mask_ptr, &ether_hdr_mask.ether_type, ETHER_TYPE_LEN);
    memcpy(mask_ptr + ETHER_TYPE_LEN, &ip_hdr_mask, sizeof(ip_hdr_mask) );
    memcpy(mask_ptr + ETHER_TYPE_LEN + sizeof(ip_hdr_mask), &udp_hdr_mask, sizeof(udp_hdr_mask) );


    memcpy(pat_ptr, &ether_hdr_pattern.ether_type, ETHER_TYPE_LEN);
    memcpy(pat_ptr + ETHER_TYPE_LEN, &ip_hdr_pattern, sizeof(ip_hdr_pattern) );
    memcpy(pat_ptr + ETHER_TYPE_LEN + sizeof(ip_hdr_pattern), &udp_hdr_pattern, sizeof(udp_hdr_pattern) );

#ifdef DEBUG
    OL_LOG_PF(LOG_OLA_LVL_INFO, "Before add_packet_filter()\n");
    cylpa_print_pat_and_mask(filter.id, PORT_FILTER_LEN, cylpa_glob_mask_buf, cylpa_glob_pat_buf);
#endif

    if (whd_pf_add_packet_filter(whd, &filter) != WHD_SUCCESS)
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "Add filter %ld Failed\n", filter.id);
    }
#endif /* USE_HW */

    return 0;
}

/*******************************************************************************
 * Function Name: cylpa_create_single_portnum_mask
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param pf_cfg
 * The pointer to the cy_pf_ol_cfg_t structure \ref cy_pf_ol_cfg_t.
 *
 * \param pattern
 * TBD.
 *
 * \param mask
 * TBD.
 *
 * \return
 * Returns the execution result
 *
 ********************************************************************************/
static int cylpa_create_single_portnum_mask(cy_pf_ol_cfg_t *pf_cfg, uint16_t *pattern, uint16_t *mask)
{
    *pattern = pf_cfg->u.pf.portnum.portnum;
    *mask = 0xffff;
    return 0;
}

/*******************************************************************************
 * Function Name: cylpa_create_portnum_range_mask
 ****************************************************************************//**
 *
 * Remove all filters. No need to disable prior to removal.
 *
 * \param pf_cfg
 * The pointer to the cy_pf_ol_cfg_t structure \ref cy_pf_ol_cfg_t.
 *
 * \param pattern
 * TBD.
 *
 * \param mask
 * TBD.
 *
 * \return
 * Returns the execution result
 *
 ********************************************************************************/
static int cylpa_create_portnum_range_mask(cy_pf_ol_cfg_t *pf_cfg, uint16_t *pattern, uint16_t *mask)
{
    uint16_t port = pf_cfg->u.pf.portnum.portnum;
    uint16_t range = pf_cfg->u.pf.portnum.range;

    /* initialize filter patterns */
    *pattern = port;

    /*
     * Say using port = 64 and  range = 3. Translates into binary:  100 0000 thru 100 0011
     *   Pattern: Use port number for pattern (100 0000)
     *   Mask:    Need to invert low bits, and fill 0s between resulting 1s.
     *             To invert low bits, subtract 1.
     *             OR the port back in since it was removed by the -1.
     *       Example mask: convert 100 0011 into 111 1100
     */
    *mask = ( ( (port - 1) ^ range ) | port );

    /* Now sign extend all the way to the leftmost/highest bit
     * to prevent false hits on high bits
     */
    *mask = cylpa_sign_extend(*mask, cylpa_bitwidth(*mask) );

    return 0;
}

static int cylpa_sign_extend(int val, unsigned width)
{
    int const mask = 1U << (width - 1);

    val = val & ( (1U << width) - 1 );
    return ( (val ^ mask) - mask );
}

/* Return position of set bit using 1 based notation */
static int cylpa_bitwidth(int val)
{
    int pos;
    for (pos = MAX_BITS_IN_PORTNUM - 1; pos >= 0; pos--)
    {
        if (val & (1 << pos) )
        {
            return (pos + 1);
        }
    }
    return -1;
}

#ifdef TESTING
/* Simulate FW using the pattern and mask */
static void cylpa_run_pf_test(unsigned int pattern, unsigned int mask)
{
    int i;
    if (!pattern)
    {
        return;
    }

    for (i = 0; i < 65000; i++)
    {
        if ( (i & mask) == pattern )
        {
            OL_LOG_PF(LOG_OLA_LVL_INFO, "%d\n", i);
        }
    }
}

#endif

/* Bug in FW? doesn't allow us to retrieve filter patterns after enabling.
 * dump_patterns controls this.
 */
static int cylpa_dump_filters_stats(whd_t *whd, cy_pf_ol_cfg_t *base_pf_cfg)
{
#ifdef USE_HW
    cy_pf_ol_cfg_t *pf_cfg;
    OL_LOG_PF(LOG_OLA_LVL_ERR, "\n");
    OL_LOG_PF(LOG_OLA_LVL_ERR, "               \t Total    Total\n");
    OL_LOG_PF(LOG_OLA_LVL_ERR, "ID:   Matched  \tSent Up  Dropped\n");

    for (pf_cfg = base_pf_cfg; pf_cfg->feature != CY_PF_OL_FEAT_LAST; pf_cfg++)
    {
        cylpa_print_packet_filter_stats(whd, pf_cfg->id);
    }
#endif /* USE_HW */
    return 0;
}

#ifdef USE_HW
#ifdef DEBUG
static void cylpa_print_pat_and_mask(uint8_t id, int mask_len, uint8_t *mask, uint8_t *pat)
{
    int b;
    OL_LOG_PF(LOG_OLA_LVL_ERR, "ID: %d\n", id);
    OL_LOG_PF(LOG_OLA_LVL_ERR, "      Mask: ");
    b = mask_len;
    while (b > 0)
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "%02x ", *mask);
        mask++;
        b--;
    }

    OL_LOG_PF(LOG_OLA_LVL_ERR, "\n");
    OL_LOG_PF(LOG_OLA_LVL_ERR, "     Value: ");
    b = mask_len;
    while (b > 0)
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "%02x ", *pat);
        pat++;
        b--;
    }
    OL_LOG_PF(LOG_OLA_LVL_ERR, "\n");
}

#endif /* DEBUG */

/* Query and print stats from each filter */
static int cylpa_print_packet_filter_stats(whd_t *whd, uint8_t filter_id)
{
    whd_result_t status;
    wl_pkt_filter_stats_t stats;

    status = whd_pf_get_packet_filter_stats(whd, filter_id, &stats);
    if (status != WHD_SUCCESS)
    {
        OL_LOG_PF(LOG_OLA_LVL_ERR, "Failure Getting Packet Filter Statistics 0x%x\n", status);
        return status;
    }

    OL_LOG_PF(LOG_OLA_LVL_ERR, " %u:   %u \t%u \t%u\n",
              (unsigned int)filter_id, (unsigned int)stats.num_pkts_matched,
              (unsigned int)stats.num_pkts_forwarded, (unsigned int)stats.num_pkts_discarded);

    return status;
}

#endif /* USE_HW */

#ifdef __cplusplus
}
#endif

