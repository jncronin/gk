# PSoC 6 MCU: WLAN Manufacturing Test Middleware

The WLAN Manufacturing Test Middleware application is used to validate the WLAN firmware and radio performance of the Wi-Fi device. The mfg-test middleware repo can accept the serial input byte stream from the Mfg Test application and transform the contained commands into IOVAR/IOCTL messages to the wlan firmware. It can get the response from the wlan firmware (if expected), and transport them back to the 'wl tool' running on the host.

This repo should be used with FreeRTOS and Mbed OS applications to test the wlan firmware.

The Mfg Test middleware library consists of the Mfg Test Middleware Porting layer to interface with the wlan firmware and Wi-Fi functionality across SDKs such as FreeRTOS and Mbed OS.

Version 3.3.0 fixed minor compilation warning for ARM compiler.

Version 3.2.0 updated the documentation.

Version 3.1.0 updated the documentation and copyright.

Version 3.0.1 adds support for the following:
- [ModusToolbox® software](https://www.cypress.com/products/modustoolbox-software-environment) v2.3
- Supported Toolchains: ARMC for FreeRTOS


## Requirements

- [ModusToolbox® software](https://www.cypress.com/products/modustoolbox-software-environment) v2.2
- Programming Language: C
- Supported Toolchains: Arm® GCC, IAR
- Associated Parts: All [PSoC® 6 MCU](http://www.cypress.com/PSoC6) parts


## Supported Software and Tools

ToolChain     | OS
--------------|----
GCC_ARM and IAR | FreeRTOS
GCC_ARM and ARMC6 | Mbed OS|


## Integration Notes

- The Wi-Fi Manufacturing Test Middleware library works with ModusToolBox Wifi Manfacturing Tester applications.

- The library is integrated into Wi-Fi manufacturing tester applications.

- You only need to include this library in the intended ecosystem to use these utilities. Depending on the ecosystem, the relevant source files will be picked up and linked, using `COMPONENT_ model_`.


## Additional Information

- [Wi-Fi Mfg Test Utilities API reference guide](https://cypresssemiconductorco.github.io/wifi-mfg-test/api_reference_manual/html/index.html)


---
(c) 2021, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
This software, associated documentation and materials ("Software") is owned by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and is protected by and subject to worldwide patent protection (United States and foreign), United States copyright laws and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software ("EULA"). If no EULA applies, then any reproduction, modification, translation, compilation, or representation of this Software is prohibited without the express written permission of Cypress.
Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the Software without notice. Cypress does not assume any liability arising out of the application or use of the Software or any product or circuit described in the Software. Cypress does not authorize its products for use in any products where a malfunction or failure of the Cypress product may reasonably be expected to result in significant property damage, injury or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the manufacturer of such system or application assumes all risk of such use and in doing so agrees to indemnify Cypress against all liability.
