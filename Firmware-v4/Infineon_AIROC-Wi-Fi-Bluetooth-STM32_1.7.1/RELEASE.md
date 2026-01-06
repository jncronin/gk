# Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack 1.7.1 Release Notes
The Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack is an extension of the CMSIS-Pack standard established by Arm.
The pack is compliant with the full CMSIS-Pack standard, with additional requirements/restrictions
on the final pack to meet the STM standard.

This pack uses libraries from the ModusToolbox environment.

## What's Included?
* Maintenance Release

## What Changed?

 v1.7.1

*  Update Wifi firmware 4373/4373E TO 13.10.246.356
*  Update the wcm application to version 3.6

## Supported STM32 Boards and MCU
*  STM32L562E-DK kit and STM32L5xx
*  STM32H747I-DISCO Discovery kit and STM32H7xx
*  STM32U575I-EV Evaluation board and STM32U5xx
*  NUCLEO-H563ZI board and STM32H5xx
*  NUCLEO-H745ZI-I board
*  NUCLEO-U575ZI-Q board

## Supported Connectivity Modules

Infineon's CYW43xxx Wi-Fi-BT combo chip family:
*  CYW43012
*  CYW43439 / CYW43438 / CYW4343W
*  CYW4373 / CYW4373/E
*  CYW43022
*  CYW55560
*  CYW55572

## Example apps inside the Pack

### Wi-Fi Scan Example
This example initializes the Wi-Fi device and starts a Wi-Fi scan without any filter and prints the
results on the serial terminal.

Refer to Projects/STM32H747I-DISCO/Applications/wifi_scan/readme.txt for more details

### Wi-Fi Onboarding with Bluetooth LE Example
This example demonstrates a simultaneous usage of Wi-Fi and BLE functionality of CYW43xxx combo
devices. It uses BLE on the combo device to help connect the Wi-Fi to the AP.

Refer to Projects/STM32H747I-DISCO/Applications/ble_wifi_onboarding/readme.txt for more details

### Azure RTOS NetXDuo Wi-Fi UDP echo server
This application provides an example of Azure RTOS/NetXDuo stack usage. It shows how to develop a 
NetX UDP server to communicate with a remote client using the NetX UDP socket API.

### BLE Hello Sensor Example
This code example demonstrates the implementation of a simple Bluetooth Stack functionality in GAP
Peripheral role. During initialization the app registers with LE stack to receive various notifications
including bonding complete, connection status change and peer write.

### Wi-Fi MQTT Client Example
This code example demonstrates implementing an MQTT client using the MQTT library. The library uses
the AWS IoT device SDK Port library and implements the glue layer that is required for the library to
work with Infineon connectivity platforms.

## Compatible Software

|  Software                    | Version   |
| ---------------------------- | --------- |
| STM32 CubeMX                 |  6.14.1   |
| STM32 CubeIDE                |  1.18.1   |
| IAR Embedded Workbench IDE   |  9.30.1   |

### Supported Compiler Version

| Compiler                     | Version |
| :---                         | :----:  |
| GCC Compiler                 | 11.3    |
| IAR Compiler                 | 9.30.1  |


## Change History

| Version | Changes                                                             | Reason for Change                     |
| :----:  | :---                                                                | :----                                 |
| 1.7.1   | Update 4373 firmware, wcm                                           | firmware, applications                |
| 1.7.0   | Added enterprise-security support                                   | New modules, applications             |
| 1.6.1   | Update Wi-Fi assets                                                 | Maintenance                           |
| 1.6.0   | Added New Connectivity Module support                               | New CYWxx modules, assets update, mqtt|
| 1.5.1   | Update Bluetooth FW for CYW4373 modules                             | Update Bluetooth Firmware             |
| 1.5.0   | Added New Connectivity Module support                               | Update Pack Name and new CYWxx modules|
| 1.4.0   | Added STM32H5 support.                                              | Add support of new STM32 series       |
| 1.3.0   | Added STM32U5 support.                                              | Add support of new STM32 series       |
| 1.2.0   | Added STM32L5 support.                                              | Add support of new STM32 series       |
|         | Added ThreadX/NetxDuo support.                                      | Add support for Azure RTOS            |
|         | Added WiFi and BLE Manufacturing tools support.                     | Manufacturing tools (Iperf and mfgtest) support. Documentation updates. |
| 1.1.0   | New release comprising BLE/BT support and various PAL improvements. | BLE/BT support and code example added |
| 1.0.1   | Patch release.                                                      | SDIO Buffer overrun fix.              |
| 1.0.0   | The initial version                                                 |                                       |


## Future release

* Add support of new STM32 series.


### More information
* [README.md](./README.md)
* [Infineon AIROC-Wi-Fi-Bluetooth-STM32 Expansion Pack User Guide](./Documentation/InfineonAIROC-Wi-Fi-Bluetooth-STM32ExpansionPack_UserGuide.pdf)
* [Cypress Semiconductor, an Infineon Technologies Company](http://www.infineon.com)
* [Infineon GitHub](https://github.com/Infineon/)
* [ModusToolbox](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)


-------------------------------------------------------------------------------

© Cypress Semiconductor Corporation, 2021-2024.
