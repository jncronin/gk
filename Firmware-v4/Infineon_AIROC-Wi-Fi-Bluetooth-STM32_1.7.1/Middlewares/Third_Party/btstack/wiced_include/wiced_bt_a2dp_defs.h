/*
 * Copyright 2016-2023, Cypress Semiconductor Corporation or
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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "wiced.h"
#include "wiced_bt_types.h"
#include "wiced_result.h"
#include "wiced_bt_a2d.h"
#include "wiced_bt_a2d_sbc.h"
#include "wiced_bt_a2d_m12.h"
#include "wiced_bt_a2d_m24.h"

/** @file:   wiced_bt_a2dp_defs.h */


/******************************************************
 *                   Enumerations
 ******************************************************/

/** Masks for supported Codecs */
typedef enum
{
    WICED_BT_A2DP_CODEC_SBC             = 0x00, /**< SBC Codec */
    WICED_BT_A2DP_CODEC_M12             = 0x01, /**< MPEG-1, 2 Codecs */
    WICED_BT_A2DP_CODEC_M24             = 0x02, /**< MPEG-2, 4 Codecs */
    WICED_BT_A2DP_CODEC_VENDOR_SPECIFIC = 0xFF, /**< Vendor specific codec */
} wiced_bt_a2dp_codec_t;


/******************************************************
 *                 Type Definitions
 ******************************************************/
/** A2DP data path callback type
 *
 *  Application implements callback of this type to receive A2DP media packets.
 *  Receives raw PCM samples in case of SBC codec and encoded audio data
 *  in case of AAC codec
 *
 *  @param p_audio_data   pointer to audio data
 *  @param a2dp_data_len  audio data length
 *
 *  @return none
 */
typedef void (*wiced_bt_a2dp_sink_data_cb_t)( uint8_t* p_a2dp_data, uint32_t a2dp_data_len );
/******************************************************
 *                    Structures
 ******************************************************/

/** Vendor Specific Codec information element type */
typedef struct
{
    uint8_t  cie_length; /**< Length of codec information element in octets */
    uint8_t* cie;        /**< Codec information element */
} wiced_bt_a2d_vendor_cie_t;

/** Codec information element structure, used to provide info of a single type of codec */
typedef struct
{
    wiced_bt_a2dp_codec_t codec_id; /**< One of WICED_BT_A2DP_CODEC_XXX, to indicate the valid element of the cie union */
    union
    {
        wiced_bt_a2d_sbc_cie_t    sbc; /**< SBC information element */
        wiced_bt_a2d_m12_cie_t    m12; /**< MPEG-1, 2 information element */
        wiced_bt_a2d_m24_cie_t    m24; /**< MPEG-2, 4 information element */
        wiced_bt_a2d_vendor_cie_t vsp; /**< Vendor Specific codec information element */
    } cie;/**< Codec information element */
} wiced_bt_a2dp_codec_info_t;

/** @} */ // end of wicedbt_a2dp

#ifdef __cplusplus
} /*extern "C" */
#endif
