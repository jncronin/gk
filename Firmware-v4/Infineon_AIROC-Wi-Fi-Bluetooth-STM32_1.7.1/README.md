# Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack 1.7.1

## Overview
The Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack is an extension of the CMSIS-Pack standard established by Arm
to support Wi-Fi and BLE application on STM32 MCUs with Infineon Wireless Combo devices.

This pack is MCU and flow-independent, meaning the user can choose the compiler toolchain and IDE,
and adapt the source code to any STM microcontroller offering sufficient resources (serial
interfaces and memory) and performance to run the application.

This pack uses libraries from the ModusToolbox environment. For more details, refer to
https://www.cypress.com/products/modustoolbox.

You can select and configure the pack in the STM32CubeMX tool, make choices appropriate for your
design, such as which CYW43xxx device to use, and then generate a project from your selection.

## Infineon AIROC Wi-Fi/Bluetooth Combo chip support
|  AIROC Wi-Fi/BT Combo        | Wi-Fi 4/5/6/6E|  STM32H7xx     | STM32H5xx| STM32U5xx     | STM32L5xx | 
| ---------------------------- | ---------  | ---------------------------- | ---------  | ---------------------------- | ---------  |
| CYW43012                     |  Wi-Fi 5   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   |
| CYW43022                     |  Wi-Fi 5   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">  |
| CYW43439                     |  Wi-Fi 4   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   |
| CYW4373/CYW4373E             |  Wi-Fi 5   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   |
| CYW55513                     |  Wi-Fi 6/6E| <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  PENDING   |
| CYW55572                     |  Wi-Fi 6   | <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">                     |  <img width="23" alt="check_ifx" src="https://github.com/Infineon/stm32-connectivity/assets/105271488/33108d4b-bd72-4039-a962-d5d0ddb9f7f5">   | PENDING                     |  PENDING   | 

## Dependencies
Wi-Fi application requires all the assets mentioned in the wifi group of expansion pack to be
selected along with proper CYW43xxx selected in the Device dropbox in STM32CubeMX.

## Expansion Pack Contents
The Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack uses below assets:

|  Asset                                                                                                     | Version |
| ---------------------------------------------------------------------------------------------------------- | ------- |
| [LwIP](https://git.savannah.nongnu.org/cgit/lwip.git)                                                      |  2.1.2  |
| [abstraction-rtos](https://github.com/Infineon/abstraction-rtos)                                           |  1.8.0  |
| [bluetooth-freertos](https://github.com/Infineon/btstack-integration)                                      |  4.3.1  |
| [btstack](https://github.com/Infineon/btstack)                                                             |  3.7.1  |
| [command-console](https://github.com/Infineon/command-console)                                             |  6.0.0  |
| [connectivity-utilities](https://github.com/Infineon/connectivity-utilities)                               |  4.4.0  |
| [core-lib](https://github.com/Infineon/core-lib)                                                           |  1.3.1  |
| [wcm](https://github.com/Infineon/wifi-connection-manager)                                                 |  3.6.0  |
| [whd-bsp-integration](https://github.com/Infineon/whd-bsp-integration)                                     |  2.3.0  |
| [lwip-freertos-integration](https://github.com/Infineon/lwip-freertos-integration)                         |  1.0.0  |
| [lwip-network-interface-integration](https://github.com/Infineon/lwip-network-interface-integration)       |  1.3.1  |
| [netxduo-network-interface-integration](https://github.com/Infineon/netxduo-network-interface-integration) |  1.3.0  |
| [wifi-host-driver](https://github.com/Infineon/wifi-host-driver)                                           |  4.1.0  |
| [wifi-mfg-test](https://github.com/Infineon/wifi-mfg-test)                                                 |  3.3.0  |
| [secure-sockets](https://github.com/Infineon/secure-sockets)                                               |  3.0.0  |
| [stm32_mw_freertos](https://github.com/STMicroelectronics/stm32_mw_freertos)                               | 10.4.6  |
| [wpa3-external-supplicant](https://github.com/Infineon/wpa3-external-supplicant)                           |  1.1.0  |
| [aws-iot-device-sdk-port](https://github.com/Infineon/aws-iot-device-sdk-port)                             |  2.3.0  |
| [aws-iot-device-sdk-embedded-C](https://github.com/aws/aws-iot-device-sdk-embedded-C)                      | 202103  |
| [mqtt](https://github.com/Infineon/mqtt)                                                                   |  4.3.0  |
| PAL                                                                                                        |  1.6.1  |
| Device                                                                                                     |  1.6.1  |

## Additional details
The pack is compliant with the full CMSIS-Pack standard, with additional requirements/restrictions
on the final pack to meet the STM standard.

## Known issues
Refer to section 'Known issues/limitations' of Infineon AIROC-Wi-Fi-Bluetooth-STM32 expansion pack user guide.


## More information
* [RELEASE.md](./RELEASE.md)
* [Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack User Guide](./Documentation/InfineonAIROC-Wi-Fi-Bluetooth-STM32ExpansionPack_UserGuide.pdf)
* [Cypress Semiconductor, an Infineon Technologies Company](http://www.infineon.com)
* [Infineon GitHub](https://github.com/Infineon/)
* [ModusToolbox](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)

------

All other trademarks or registered trademarks referenced herein are the property of their respective
owners.

![Banner](Documentation/ifx-cy-banner.png)

-------------------------------------------------------------------------------

© Cypress Semiconductor Corporation, 2021-2025.
