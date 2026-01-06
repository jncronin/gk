# Low Power Assistant Middleware(LPA) Library

## Overview

The LPA middleware provides an easy way to make the low-power features available to developers in the form of a portable configuration layer. The LPA library functions are only used by Wi-Fi. LPA provides features for MCU Low Power, Wi-Fi Low Power and Bluetooth Low Power but the LPA library only needs to be included in applications that use Wi-Fi low power.

The LPA middleware is essentailly an WiFi offload manager which is instantiated when WiFi-Connection-Manager initialization is done.
The offload manager gets created and manages the offloads based on the configured offloads using CUSTOM DESIGN MODUS via device configurator created personality in PDL. The PDL personality consists of Pin Configuration for CYBSP_WIFI_HOST_WAKE, CYBSP_WIFI_DEVICE_WAKE, CYBSP_BT_HOST_WAKE and CYBSP_BT_DEVICE_WAKE. The saved design.modus generates the sources (cycfg_connectivity_wifi.h, cycfg_connectivity_wifi.c) which are compiled and linked in the executable for configured Wi-Fi Offloads.


LPA middleware consists of the following components:

* Configurator tool (using a personality), which makes the low-power features of the system easy to use (ModusToolbox Device Configurator Tool Guide here  https://www.cypress.com/file/504376/download ). This personality writes data structures and once the configuration is saved it generates cycfg_connectivity_wifi.c and cycfg_connectivity_wifi.h files based on the choices made in the personality.
* The above generated sources (cycfg_connectivity_wifi.h, cycfg_connectivity_wifi.c) are compiled and linked in the executable. The API in the generated source will be invoked at system initialization.
* The LPA configuration is applied from host PSOC6 MCU middleware s/w to IoT wifi-bt Firmware during initialization such as BT low power , ARPOL (ARP Offload) and some of the features such as PFOL(Packet Filter Offload) are applied when host WiFi connection is established with WiFi Access point. TKOL ( TCP Keep-alive offload ) is applied when an TCP connection is established with a remote TCP server.

## Features
* MCU Low Power
* Wi-Fi and Bluetooth Low Power
* Wi-Fi Address Resolution Protocol (ARP) Offload
* Wi-Fi Packet Filter Offload
* Wi-Fi TCP Keepalive Offload

For More Release specific information, refer to [RELEASE.md](./RELEASE.md)

## Requirements
- [ModusToolbox™ software](https://www.cypress.com/products/modustoolbox-software-environment) v2.3
- Programming Language: C

## Supported Platforms
This library and it's features are supported on following Cypress platforms:

##### MBEDOS
* [PSoC 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W) 
* [PSoC 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)
* [PSoC 6 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062-WiFi-BT) (CY8CKIT-062-WIFI-BT)

##### FREERTOS
* [PSoC 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W) 
* [PSoC 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)
* [PSoC 6 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062-WiFi-BT) (CY8CKIT-062-WIFI-BT)
* [CY8CEVAL-062S2](https://www.cypress.com/part/cy8ceval-062s2) + [Sterling LWB5Plus](https://www.mouser.com/new/laird-connectivity/laird-connectivity-sterling-lwb5plus) (CY8CEVAL-062S2-LAI-4373M2)
* [CY8CEVAL-062S2](https://www.cypress.com/part/cy8ceval-062s2) + [1YN M.2 Module](https://www.embeddedartists.com/products/1yn-m-2-module) (CY8CEVAL-062S2-MUR-43439M2)

##### AMAZON FREERTOS
* [PSoC 6 Wi-Fi BT Prototyping Kit](https://www.cypress.com/CY8CPROTO-062-4343W) (CY8CPROTO-062-4343W)
* [PSoC 62S2 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062S2-43012) (CY8CKIT-062S2-43012)
* [PSoC 6 Wi-Fi BT Pioneer Kit](https://www.cypress.com/CY8CKIT-062-WiFi-BT) (CY8CKIT-062-WIFI-BT)

## Limitations
Refer to the [Low Power Assistant Middleware Library Limitations](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html#group_lpa_p2_cc_limitations).

## Dependent libraries
This section provides the list of dependent libraries required for this middleware library.

##### FreeRTOS
* [Wi-Fi Connection Manager](https://github.com/Infineon/wifi-connection-manager)
* [Wi-Fi middleware core](https://github.com/Infineon/wifi-mw-core)
* [Connectivity middleware utilities](https://github.com/Infineon/connectivity-utilities)

##### MBEDOS
* [Connectivity middleware utilities](https://github.com/Infineon/connectivity-utilities)

##### Amazon FreeRTOS
* [Connectivity middleware utilities](https://github.com/Infineon/connectivity-utilities)

## Supported Software and Tools
```
ToolChain : GCC_ARM, IAR and ARMC6 (FreeRTOS)
            GCC_ARM and ARMC6 (MBEDOS)
            GCC_ARM, IAR and ARMC6 (Amazon FreeRTOS)
OS        : MBEDOS , FreeRTOS and Amazon FreeRTOS
```
Refer to the [Low Power Assistant Middleware Library Software and Tools](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html#section_lpa_toolchain).

## Quick Start
The LPA could be configured by the ModusToolbox MCU, WiFi and BT personalities. Refer to the [Low Power Assistant Middleware Library Configuration Considerations](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html#group_lpa_p2_cc).

##### FreeRTOS
Refer to [Wi-Fi middleware core README](https://github.com/cypresssemiconductorco/wifi-mw-core/blob/master/README.md) for basic makefile changes required .

MCU Deep Sleep Functionality is enabled by default in FreeRTOSConfig.h as below 

```
#include <cycfg_system.h>
#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_SLEEP) || (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
extern void vApplicationSleep( uint32_t xExpectedIdleTime );
#define portSUPPRESS_TICKS_AND_SLEEP( xIdleTime ) vApplicationSleep( xIdleTime )
#define configUSE_TICKLESS_IDLE  2
#endif

/* Deep Sleep Latency Configuration */
#if CY_CFG_PWR_DEEPSLEEP_LATENCY > 0
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   CY_CFG_PWR_DEEPSLEEP_LATENCY
#endif
```
##### Amazon FreeRTOS
MCU Deep Sleep Functionality can be enabled by adding below changes to FreeRTOSConfig.h
```
#include <cycfg_system.h>
#if defined(__STDC__) || defined(__cplusplus__)
#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_SLEEP) || (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
extern void vApplicationSleep( uint32_t xExpectedIdleTime );  //This is to prevent IAR toolchain Compile error with CMake
#endif
#endif

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_SLEEP) || (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
#define portSUPPRESS_TICKS_AND_SLEEP( xIdleTime ) vApplicationSleep( xIdleTime )
#define configUSE_TICKLESS_IDLE  2
#endif

/* Deep Sleep Latency Configuration */
#if CY_CFG_PWR_DEEPSLEEP_LATENCY > 0
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   CY_CFG_PWR_DEEPSLEEP_LATENCY
#endif
```

## Debugging
For Debugging purposes, the application may enable debug, log and error log messages by updating ol_log_level[] with LOG_OLA_LVL_DEBUG.
```
eg:
ol_log_level[LOG_OLA_OLM] = LOG_OLA_LVL_DEBUG;
ol_log_level[LOG_OLA_ARP] = LOG_OLA_LVL_DEBUG;
ol_log_level[LOG_OLA_PF]  = LOG_OLA_LVL_DEBUG;
ol_log_level[LOG_OLA_TKO] = LOG_OLA_LVL_DEBUG;
```

## More information
The following resources contain more information:
* [LPA Middleware RELEASE.md](./RELEASE.md)
* [Low Power Assistant Middleware Library](https://cypresssemiconductorco.github.io/lpa/lpa_api_reference_manual/html/index.html)
* [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.cypress.com/products/modustoolbox-software-environment)
* [LPA Middleware Code Example for MBED OS](https://github.com/Infineon/mbed-os-example-wlan-lowpower)
* [LPA Middleware Code Example for FREERTOS](https://github.com/Infineon/mtb-example-anycloud-wlan-lowpower)
* [LPA Middleware Code Example for AMAZON FREERTOS](https://github.com/Infineon/afr-example-wlan-offloads)
* [ModusToolbox Device Configurator Tool Guide](https://www.cypress.com/ModusToolboxDeviceConfig)
* [PSoC 6 Technical Reference Manual](https://www.cypress.com/documentation/technical-reference-manuals/psoc-6-mcu-psoc-63-ble-architecture-technical-reference)
* [PSoC 63 with BLE Datasheet Programmable System-on-Chip datasheet](http://www.cypress.com/ds218787)

---

------

All other trademarks or registered trademarks referenced herein are the property of their respective owners.

-------------------------------------------------------------------------------

© 2021, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
This software, associated documentation and materials ("Software") is owned by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and is protected by and subject to worldwide patent protection (United States and foreign), United States copyright laws and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software ("EULA"). If no EULA applies, then any reproduction, modification, translation, compilation, or representation of this Software is prohibited without the express written permission of Cypress.
Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to the Software without notice. Cypress does not assume any liability arising out of the application or use of the Software or any product or circuit described in the Software. Cypress does not authorize its products for use in any products where a malfunction or failure of the Cypress product may reasonably be expected to result in significant property damage, injury or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the manufacturer of such system or application assumes all risk of such use and in doing so agrees to indemnify Cypress against all liability.
