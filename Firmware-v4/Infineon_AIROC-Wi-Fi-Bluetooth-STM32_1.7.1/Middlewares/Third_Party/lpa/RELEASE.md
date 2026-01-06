# Low Power Assistant Middleware(LPA) Library 4.0.0

### What's Included?

Please refer to the [README.md](./README.md) and the [Low Power Assistant Middleware Library](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html) for a complete description of the LPA Middleware.
The revision history of the LPA Middleware is also available on the [Low Power Assistant Middleware Library Changelog](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html#group_lpa_changelog).

New in this release:
Add LPA support for 4373 Kit  


### Release Versions

|  Version         | Description of Change                                                  | Comments                                                 |
| ---------------- | ---------------------------------------------------------------------- | -------------------------------------------------------------- |
| 4.0.0            | Add LPA support for 43439 Kit                                          | ModusToolbox                                               |
| 3.2.0            | Add LPA support for 4373 Kit                                           | ModusToolbox                                               |
| 3.1.1            | Fix for wait_net_suspend with TCPIP core locking Configuration         | MBED OS 6.8.0 , ModusToolbox, Amazon FreeRTOS v202007.00  |
| 3.1.0            | Minor Fixes and documentation update                                   | MBED OS 6.8.0 , ModusToolbox, Amazon FreeRTOS v202007.00  |
| 3.0.0            | LPA Middleware for all SDK(s) and Modustoolbox 2.2                     | MBED OS 6.2.0 , ModusToolbox, Amazon FreeRTOS v202007.00  |
| 2.1.0(ER)        | Add AFR SDK Support (Wi-Fi Only LPA) and WLAN low power configurations | Amazon FreeRTOS v202002.00                                     |
| 2.0.0            | Add TCP Keepalive offload Feature                                      | ModusToolbox SDK                                          |
| 1.0.0            | New LPA Middleware. Only ARP and Packet Filter offloads supported      | MBEDOS upto 5.15.2                                             |


### Known Issues

### Defect Fixes

### Supported Software and Tools
This version of the LPA Middleware was validated for compatibility with the following Software and Tools:

| Software and Tools                                                        | Version   |
| :---                                                                      | :------:  |
| ModusToolbox Software Environment                                         |   3.0     |
| - ModusToolbox Device Configurator                                        |   3.0     |
| - ModusToolbox MCU Personality in Device Configurator                     |   1.2     |
| - ModusToolbox WiFi and BT Personalities in Device Configurator           |   1.0     |
| GCC compiler for MBED-OS                                                  |   9.2.0   |
| GCC compiler for ModusToolbox                                             |   10.3.1  |
| GCC compiler for AMAZON FREERTOS                                          |   7.2     |
| IAR Compiler                                                              |   9.3     |
| ARM Compiler 6                                                            |   6.16    |
| MBED OS                                                                   |   6.8.0   |
| AMAZON FREERTOS                                                           | V202007.00|

### More information
The following resources contain more information:
* [LPA Middleware RELEASE.md](./RELEASE.md)
* [Low Power Assistant Middleware Library](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html)
* [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)
* [LPA Middleware WLAN Low power Code Example for MBED OS](https://github.com/Infineon/mbed-os-example-wlan-lowpower)
* [LPA Middleware WLAN ARP Offload Example for MBED OS](https://github.com/Infineon/mbed-os-example-wlan-offload-arp)
* [LPA Middleware WLAN Packet Filter Offload Example for MBED OS](https://github.com/Infineon/mbed-os-example-wlan-offload-packet-filter)
* [LPA Middleware WLAN Low power Code Example for FREERTOS](https://github.com/Infineon/mtb-example-anycloud-wlan-lowpower)
* [LPA Middleware WLAN TCP Keepalive Offload Example for FREERTOS](https://github.com/Infineon/mtb-example-anycloud-offload-tcp-keepalive)
* [LPA Middleware WLAN Low power Code Example for AMAZON FREERTOS](https://github.com/Infineon/afr-example-wlan-lowpower)
* [LPA Middleware WLAN Offload Example for AMAZON FREERTOS](https://github.com/Infineon/afr-example-wlan-offloads)
* [ModusToolbox Device Configurator Tool Guide](https://www.cypress.com/ModusToolboxDeviceConfig)
* [PSoC 6 Technical Reference Manual](https://www.cypress.com/documentation/technical-reference-manuals/psoc-6-mcu-psoc-63-ble-architecture-technical-reference)
* [PSoC 63 with BLE Datasheet Programmable System-on-Chip datasheet](http://www.cypress.com/ds218787)
  
---

© 2021, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
This software, associated documentation and materials ("Software") is owned by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and is protected by and subject to worldwide patent protection (United States and foreign), United States copyright laws and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software ("EULA"). If no EULA applies, then any reproduction, modification, translation, compilation, or representation of this Software is prohibited without the express written permission of Cypress.
Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the Software without notice. Cypress does not assume any liability arising out of the application or use of the Software or any product or circuit described in the Software. Cypress does not authorize its products for use in any products where a malfunction or failure of the Cypress product may reasonably be expected to result in significant property damage, injury or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the manufacturer of such system or application assumes all risk of such use and in doing so agrees to indemnify Cypress against all liability.
