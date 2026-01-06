/***************************************************************************//**
* \file cy_whd_stubs.c
* \version 1.0
*
* \brief
* WHD ARP Offload API
*
********************************************************************************
* \copyright
* Copyright 2020, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cy_lpa_compat.h"
#include "cy_lpa_wifi_ol_common.h" /* for whd */
#include "cy_lpa_wifi_ol.h"
#include "cy_lpa_wifi_arp_ol.h"
#if defined(OLM_NO_HARDWARE)
#include "cy_whd_stubs.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
 *
 *  Macros
 *
 *********************************************************************************************/

/*********************************************************************************************
*
*  Data
*
*********************************************************************************************/

/*********************************************************************************************
*
*  Functions
*
*********************************************************************************************/

CYPRESS_WEAK whd_result_t whd_arp_version(whd_t *whd, uint32_t *value)
{
    (void)whd;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_peerage_get(whd_t *whd, uint32_t *value)
{
    (void)whd;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_peerage_set(whd_t *whd, uint32_t value)
{
    (void)whd;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_arpoe_get(whd_t *whd, uint32_t *value)
{
    (void)whd;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_arpoe_set(whd_t *whd, uint32_t value)
{
    (void)whd;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_cache_clear(whd_t *whd)
{
    (void)whd;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_features_get(whd_t *whd, uint32_t *features)
{
    (void)whd;
    (void)features;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_features_set(whd_t *whd, uint32_t features)
{
    (void)whd;
    (void)features;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_features_print(uint32_t features, const char *title)
{
    (void)features;
    (void)title;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_hostip_list_add(whd_t *whd, uint32_t *host_ipv4_list, uint32_t count)
{
    (void)whd;
    (void)host_ipv4_list;
    (void)count;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_hostip_list_add_string(whd_t *whd, const char *ip_addr)
{
    (void)whd;
    (void)ip_addr;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_hostip_list_clear_id(whd_t *whd, uint32_t ipv4_addr)
{
    (void)whd;
    (void)ipv4_addr;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_hostip_list_clear_id_string(whd_t *whd, const char *ip_addr)
{
    (void)whd;
    (void)ip_addr;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_hostip_list_clear(whd_t *whd)
{
    (void)whd;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_hostip_list_get(whd_t *whd, uint32_t count, uint32_t *host_ipv4_list,
                                                  uint32_t *filled)
{
    (void)whd;
    (void)count;
    (void)host_ipv4_list;
    (void)filled;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_stats_clear(whd_t *whd)
{
    (void)whd;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_stats_get(whd_t *whd, whd_arp_stats_t *arp_stats)
{
    (void)whd;
    (void)arp_stats;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_arp_stats_print(whd_arp_stats_t *arp_stats, const char *title)
{
    (void)arp_stats;
    (void)title;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_pf_enable_packet_filter(whd_t *whd, uint8_t filter_id)
{
    (void)filter_id;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_pf_disable_packet_filter(whd_t *whd, uint8_t filter_id)
{
    (void)filter_id;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_pf_add_packet_filter(whd_t *whd, const whd_packet_filter_t *settings)
{
    (void)settings;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_pf_remove_packet_filter(whd_t *whd, uint8_t filter_id)
{
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_pf_get_packet_filter_mask_and_pattern(whd_t *whd, uint8_t filter_id, uint32_t max_size,
                                                                    uint8_t *mask, uint8_t *pattern, uint32_t *size_out)
{
    (void)filter_id;
    (void)max_size;
    (void)mask;
    (void)pattern;
    (void)size_out;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_pf_get_packet_filter_stats(whd_t *whd, uint8_t filter_id, whd_pkt_filter_stats_t *stats)
{
    (void)filter_id;
    (void)stats;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_wifi_get_iovar_value(whd_t *whd, const char *iovar, uint32_t *value)
{
    (void)iovar;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_wifi_set_iovar_value(whd_t *whd, const char *iovar, uint32_t value)
{
    (void)iovar;
    (void)value;
    return WHD_IOCTL_FAIL;
}

CYPRESS_WEAK whd_result_t whd_wifi_enable_powersave_with_throughput(whd_t *whd, uint16_t return_to_sleep_delay_ms)
{
    (void)return_to_sleep_delay_ms;
    return WHD_IOCTL_FAIL;
}

#ifdef __cplusplus
}
#endif

