# WPA3 External supplicant library

## Overview

The WPA3 External Supplicant supports WPA3 SAE authentication using HnP (Hunting and Pecking Method) using RFC https://datatracker.ietf.org/doc/html/rfc7664 and H2E ( Hash to Element Method)  using RFC https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-10 and following 802.11 spec 2016.

The WPA3 External Supplicant consists of the following components:

- The WPA3 External Supplicant has WPA3 SAE finite state machine based on 802.11 spec 2016 "Wireless LAN Medium Access Control (MAC) and Physical Layer (PHY) Specifications".

- The WPA3 External supplicant has cyrptograhic library to support ECP Group 19 (NIST P-256 elliptic curve). The WPA3 External supplicant uses MBEDTLS API cyrptographic suite support to perform ECP curve NISTP256 operations of bignum and ECP point operations to compute the scalar and element for SAE authentication commit message and computes the shared secret which is used to send confirm message and verify peer confirm message.

- The WPA3 External supplicant implements constant time comparison, assignment etc and uses MBEDTLS API(s) to prevent leakage of security information to side channel attacks.

- The WPA3 External supplicant interface with WiFi-Host-Driver to set PMK, PMKID and WLAN F/W handles EAPOL key exchange to derive keys and connect to WPA3 Access point.

## Requirements

- [ModusToolbox® software](https://www.cypress.com/products/modustoolbox-software-environment) v2.4

- Programming Language: C

## Integration Notes

- The WPA3 External supplicant library has been designed to work with ModusToolbox SDK.

## Additional Information

- [WPA3 External Supplicant version](./version.xml)

## Supported Software and Tools
ToolChain                 | OS
--------------------------|----------
GCC_ARM , IAR and ARMC6   | FreeRTOS
-------------------------------------

All other trademarks or registered trademarks referenced herein are the property of their respective owners.

-------------------------------------------------------------------------------

(c) 2022, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
This software, associated documentation and materials ("Software") is owned by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and is protected by and subject to worldwide patent protection (United States and foreign), United States copyright laws and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software ("EULA"). If no EULA applies, then any reproduction, modification, translation, compilation, or representation of this Software is prohibited without the express written permission of Cypress.
Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the Software without notice. Cypress does not assume any liability arising out of the application or use of the Software or any product or circuit described in the Software. Cypress does not authorize its products for use in any products where a malfunction or failure of the Cypress product may reasonably be expected to result in significant property damage, injury or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the manufacturer of such system or application assumes all risk of such use and in doing so agrees to indemnify Cypress against all liability.

