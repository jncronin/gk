### AIROC&trade; BT/BLE Host Stack solution
AIROC&trade; BT/BLE stack provides Bluetooth&reg; functionality with high performance and less resource consumption. It targets on IoT products, especially for embedded devices.

### What's Included?
This release of AIROC&trade; BT/BLE stack solution package includes as following:
* Infineon Bluetooth&reg; BT controller firmware (CYW4343W, CYW43012, 20829)
* Platform and Operating system porting layers for P6+43xx, 20829 and P6+BLESS.

#### v4.3.1
btstack-integration v4.3.1 is a patch release with below mentioned enhancements and fixes:
* Added BT controller firmware for 20829B0
* Increased number of IPC retries
* Fixed memory alignment issue observed with ARM compiler

#### v4.3.0
btstack-integration v4.3.0 contains below updates:
* Added FW patch from Laird(LWB+) for 43439
* Added FW patches from Murata for 4373-2AE and 4373-2BC
* Added code size optimization changes in porting layer
* Added APIs to get IPC operation failure details
* Added changes to separate out HCI BTSpy traces from debug UART logs

#### v4.2.0
btstack-integration v4.2.0 contains below updates:
* Updated BT controller firmware for 20829
* Enabled FW core dump for 20829
* Added DSRAM (DeepSleep RAM) callback registration APIs for 20829
* Reduced debug UART (used for gettig HCI traces) memory size from 30K to 6K for 20829 and P6+BLESS
* Instead of using hardcoded value, read maximum number of connection count set by Application in BT configurator and use it in P6+BLESS porting layer

#### v4.1.1
btstack-integration v4.1.1 contains below update:
* Updated porting layer of P6+43xx to work with the changes done in HAL library

#### v4.1.0
btstack-integration v4.1.0 contains below updates:
* Updated BT controller firmware for 20829 and P6+BLESS
* Added ISOC support for 20829

#### v4.0.0
* Initial release of btstack-integration which has HCI-UART, BLESS-IPC & BTSS-IPC components

| Component | Hardware Platform | Description |
| :-------: | :---------------: | :---------: |
| COMPONENT_BLESS-IPC | P6 + BLESS | Represents BLESS protocol talking over IPC |
| COMPONENT_BTSS-IPC | 20829 | Represents BTSS protocol talking over IPC |
| COMPONENT_HCI-UART | P6 + 43xx | Represents HCI as the protocol talking over UART transport |

bluetooth-freertos v3.4.0 has been included as COMPONENT_HCI-UART in btstack-integration. Hence btstack-integration has initial version as 4.0.

### Supported Software and Tools
This version of Infineon BT/BLE stack API was validated for compatibility with the following Software and Tools:

| Software and Tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox™ Software Environment        | 2.4.0   |
| GCC Compiler                              | 10.3.1  |
| IAR Compiler                              | 8.4     |
| ARM Compiler                              | 6.11    |
| mbed OS                                   | 5.15    |
| FreeRTOS                                  | 10.4.3  |

---
© Infineon Technologies, 2022.
