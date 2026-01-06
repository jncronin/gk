/*******************************************************************************
* \file cybt_patchram_download.c
*
* \brief
* Implement the mechanism of BT firmware download during WICED BT stack
* initialization.
*
********************************************************************************
* \copyright
* Copyright 2018-2021 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cyabs_rtos.h"

#include "wiced_bt_dev.h"
#include "wiced_bt_stack_platform.h"

#include "cybt_prm.h"
#include "cybt_platform_config.h"
#include "cybt_platform_hci.h"
#include "cybt_platform_trace.h"
#include "cybt_platform_util.h"

#ifdef FW_DATBLOCK_SEPARATE_FROM_APPLICATION
#include "cy_ota_api.h"
    cy_ota_fwdb_bt_fw_t bt_fw_buffer_info;
#endif /* FW_DATBLOCK_SEPARATE_FROM_APPLICATION */

/******************************************************************************
 *                                Constants
 ******************************************************************************/
#define HCI_VSC_UPDATE_BAUDRATE_CMD                (0xFC18)
#define HCI_VSC_UPDATE_BAUD_RATE_UNENCODED_LENGTH  (6)


/*****************************************************************************
 *                           Type Definitions
 *****************************************************************************/
typedef enum
{
    BT_POST_RESET_STATE_IDLE = 0x00,
    BT_POST_RESET_STATE_UPDATE_BAUDRATE_FOR_FW_DL,
    BT_POST_RESET_STATE_FW_DOWNLOADING,
    BT_POST_RESET_STATE_FW_DOWNLOAD_COMPLETED,
    BT_POST_RESET_STATE_UPDATE_BAUDRATE_FOR_FEATURE,
    BT_POST_RESET_STATE_DONE,
    BT_POST_RESET_STATE_FAILED
} bt_post_reset_state_e;

typedef struct
{
    bt_post_reset_state_e  state;
} bt_fw_download_cb;

/******************************************************************************
 *                           Variables Definitions
 ******************************************************************************/
bt_fw_download_cb bt_fwdl_cb = {.state = BT_POST_RESET_STATE_IDLE};

extern const char    brcm_patch_version[];

#ifndef FW_DATBLOCK_SEPARATE_FROM_APPLICATION
extern const uint8_t brcm_patchram_buf[];
extern const int     brcm_patch_ram_length;
extern const uint8_t brcm_patchram_format;
#endif /* FW_DATBLOCK_SEPARATE_FROM_APPLICATION */

/*****************************************************************************
 *                           Function Declarations
 *****************************************************************************/
void bt_baudrate_updated_cback (wiced_bt_dev_vendor_specific_command_complete_params_t* p);
void bt_fw_download_complete_cback(cybt_prm_status_t status);
void bt_post_reset_cback(void);

/******************************************************************************
 *                           Function Definitions
 ******************************************************************************/
void bt_start_fw_download(void)
{
    bt_fwdl_cb.state = BT_POST_RESET_STATE_FW_DOWNLOADING;

#ifdef FW_DATBLOCK_SEPARATE_FROM_APPLICATION
    cy_rslt_t   result;
    /* We need to read the BT FW patch from external flash */
    /* Read into RAM from external flash */
    result = cy_ota_fwdb_get_bt_fw(&bt_fw_buffer_info);
    if (result == CY_RSLT_SUCCESS)
    {
        /* Check BT FW Patch version */
        if (strncmp(brcm_patch_version, (const char *)bt_fw_buffer_info.BT_FW_version, strlen(brcm_patch_version)) != 0)
        {
            MAIN_TRACE_ERROR("bt_start_fw_download():                FW ver = %.*s", 128, (const char *)bt_fw_buffer_info.BT_FW_version);
            MAIN_TRACE_ERROR("bt_start_fw_download(): DOES NOT MATCH FW ver = %.*s", 128, (const char *)brcm_patch_version);
        }

        MAIN_TRACE_WARNING("bt_start_fw_download() NOT using XIP: FW ver = %*.s", 128, (const char *)bt_fw_buffer_info.BT_FW_version);

        cybt_prm_download(bt_fw_download_complete_cback,
                          (void*)(bt_fw_buffer_info.BT_FW_buffer),
                          bt_fw_buffer_info.BT_FW_size,
                          0,
                          CYBT_PRM_FORMAT_HCD
                          );
    }
#else
    /* BT FW Patch included as part of app data */
    cybt_prm_download(bt_fw_download_complete_cback,
                      brcm_patchram_buf,
                      brcm_patch_ram_length,
                      0,
                      CYBT_PRM_FORMAT_HCD
                    );
#endif /* FW_DATBLOCK_SEPARATE_FROM_APPLICATION */
}

void bt_update_platform_baudrate(uint32_t baudrate)
{
    MAIN_TRACE_DEBUG("bt_update_platform_baudrate(): %d", baudrate);

    cy_rtos_delay_milliseconds(100);

    cybt_platform_hci_set_baudrate(baudrate);

    cy_rtos_delay_milliseconds(100);
}

void bt_update_controller_baudrate(uint32_t baudrate)
{
    uint8_t       hci_data[HCI_VSC_UPDATE_BAUD_RATE_UNENCODED_LENGTH];

    MAIN_TRACE_DEBUG("bt_update_controller_baudrate(): %d", baudrate);

    /* Baudrate is loaded LittleEndian */
    hci_data[0] = 0;
    hci_data[1] = 0;
    hci_data[2] = baudrate & 0xFF;
    hci_data[3] = (baudrate >> 8) & 0xFF;
    hci_data[4] = (baudrate >> 16) & 0xFF;
    hci_data[5] = (baudrate >> 24) & 0xFF;

    /* Send the command to the host controller */
    wiced_bt_dev_vendor_specific_command(HCI_VSC_UPDATE_BAUDRATE_CMD,
                                         HCI_VSC_UPDATE_BAUD_RATE_UNENCODED_LENGTH,
                                         hci_data,
                                         bt_baudrate_updated_cback
                                        );
}

void bt_baudrate_updated_cback (wiced_bt_dev_vendor_specific_command_complete_params_t* p)
{
    const cybt_platform_config_t *p_bt_platform_cfg = cybt_platform_get_config();

    switch(bt_fwdl_cb.state)
    {
        case BT_POST_RESET_STATE_UPDATE_BAUDRATE_FOR_FW_DL:
            {
                MAIN_TRACE_DEBUG("bt_baudrate_updated_cback(): Baudrate is updated for FW downloading");
                bt_update_platform_baudrate(p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_fw_download);

                bt_start_fw_download();
            }
            break;
        case BT_POST_RESET_STATE_UPDATE_BAUDRATE_FOR_FEATURE:
            {
                MAIN_TRACE_DEBUG("bt_baudrate_updated_cback(): Baudrate is updated for feature");
                bt_update_platform_baudrate(p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_feature);

                MAIN_TRACE_DEBUG("bt_baudrate_updated_cback(): post-reset process is done");
                bt_fwdl_cb.state = BT_POST_RESET_STATE_DONE;

                wiced_bt_continue_reset();
            }
            break;
        default:
            MAIN_TRACE_ERROR("bt_baudrate_updated_cback(): unknown state(%d)",
                             bt_fwdl_cb.state
                            );
            break;
    }
}

void bt_fw_download_complete_cback(cybt_prm_status_t status)
{
    const cybt_platform_config_t *p_bt_platform_cfg = cybt_platform_get_config();

    MAIN_TRACE_DEBUG("bt_patch_download_complete_cback(): status = %d", status);

    if(CYBT_PRM_STS_COMPLETE == status)
    {
        bt_fwdl_cb.state = BT_POST_RESET_STATE_FW_DOWNLOAD_COMPLETED;

        // After patch ram is launched, the baud rate of BT chip is reset to 115200
        if(HCI_UART_DEFAULT_BAUDRATE
           != p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_fw_download
          )
        {
            MAIN_TRACE_DEBUG("bt_fw_download_complete_cback(): Reset baudrate to %d",
                             HCI_UART_DEFAULT_BAUDRATE
                            );
            bt_update_platform_baudrate(HCI_UART_DEFAULT_BAUDRATE);
        }

        if(HCI_UART_DEFAULT_BAUDRATE
           != p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_feature
          )
        {
            MAIN_TRACE_DEBUG("bt_fw_download_complete_cback(): Changing baudrate to %d",
                             p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_feature
                            );

            bt_fwdl_cb.state = BT_POST_RESET_STATE_UPDATE_BAUDRATE_FOR_FEATURE;
            bt_update_controller_baudrate(p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_feature);
        }
        else
        {
            MAIN_TRACE_DEBUG("bt_fw_download_complete_cback(): post-reset process is Done");

            bt_fwdl_cb.state = BT_POST_RESET_STATE_DONE;
            wiced_bt_continue_reset();
        }
    }
    else
    {
        MAIN_TRACE_ERROR("bt_patch_download_complete_cback(): Failed (%d)",
                         status
                        );
        bt_fwdl_cb.state = BT_POST_RESET_STATE_FAILED;
    }

#ifdef FW_DATBLOCK_SEPARATE_FROM_APPLICATION
    cy_rslt_t result;
    result = cy_ota_fwdb_free_bt_fw(&bt_fw_buffer_info);
    if (result != CY_RSLT_SUCCESS)
    {
        MAIN_TRACE_ERROR("cy_ota_fwdb_free_bt_fw(): Failed to free RAM (%d)", result);
    }
#endif /* FW_DATBLOCK_SEPARATE_FROM_APPLICATION */
}

void bt_post_reset_cback(void)
{
    const cybt_platform_config_t *p_bt_platform_cfg = cybt_platform_get_config();

    MAIN_TRACE_DEBUG("bt_post_reset_cback()");

    bt_fwdl_cb.state = BT_POST_RESET_STATE_IDLE;
#ifndef FW_DATBLOCK_SEPARATE_FROM_APPLICATION
    if(0 < brcm_patch_ram_length)
#else /* !FW_DATBLOCK_SEPARATE_FROM_APPLICATION */
    cy_rslt_t   result;
    cy_ota_fwdb_bt_fw_info_t bt_fw_info;
    /* We need to read the BT FW patch from external flash */
    result = cy_ota_fwdb_get_bt_fw_info(&bt_fw_info);
    if ((result == CY_RSLT_SUCCESS) && (0 < bt_fw_info.BT_FW_size))
#endif /* !FW_DATBLOCK_SEPARATE_FROM_APPLICATION */
    {
        if(p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_fw_download != HCI_UART_DEFAULT_BAUDRATE)
        {
            MAIN_TRACE_DEBUG("bt_post_reset_cback(): Change baudrate (%d) for FW downloading",
                             p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_fw_download
                            );
            bt_fwdl_cb.state = BT_POST_RESET_STATE_UPDATE_BAUDRATE_FOR_FW_DL;
            bt_update_controller_baudrate(p_bt_platform_cfg->hci_config.hci.hci_uart.baud_rate_for_fw_download);
        }
        else
        {
            MAIN_TRACE_DEBUG("bt_post_reset_cback(): Starting FW download...");
            bt_start_fw_download();
        }
    }
    else
    {
        MAIN_TRACE_ERROR("bt_post_reset_cback(): invalid length");
    }
}

