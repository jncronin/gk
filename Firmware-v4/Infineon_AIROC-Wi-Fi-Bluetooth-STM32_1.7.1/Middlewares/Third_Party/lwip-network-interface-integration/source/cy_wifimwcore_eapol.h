/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
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

#include "whd_wifi_api.h"
#include "cy_result.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
* \addtogroup group_lwip_network_interface_integration lwip-network-interface-integration
* \{
*
* \defgroup group_wifimwcore_eapol_functions EAPOL Functions
*/

/**
* \addtogroup group_wifimwcore_eapol_functions
* \{
* * Provides functions that the application or library can use to register and deregister.
* * These APIs are generally used by the enterprise security library.
*/

/**
 * EAPOL packet handler function pointer type; on receiving the EAPOL data, the WHD will send the data to the Wi-Fi Middleware Core.
 * The buffer should be freed by the EAPOL handler.
 *
 * @param[in] whd_iface  WHD interface.
 * @param[in] buffer     Buffer received from the WHD.
 *
 */
typedef void (*cy_wifimwcore_eapol_packet_handler_t) (whd_interface_t whd_iface, whd_buffer_t buffer);

/**
 *
 * This API allows registering the callback functions to receive EAPOL packets
 * from the WHD. If the callback is registered, and the received packet is an EAPOL packet,
 * it will be redirected to the registered callback. Passing "NULL"
 * as the handler will deregister the previously registered callback.
 *
 * @param[in] eapol_packet_handler : Callback function to be invoked when EAPOL packets are received from the WHD.
 *
 * @return CY_RSLT_SUCCESS if the registration was successful; returns \ref generic_lwip_whd_port_defines otherwise.
 *
 */
cy_rslt_t cy_wifimwcore_eapol_register_receive_handler(cy_wifimwcore_eapol_packet_handler_t eapol_packet_handler);

/** \} group_wifimwcore_eapol_functions */
#ifdef __cplusplus
}
#endif
/** \} group_lwip_network_interface_integration */
