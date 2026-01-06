#AIROC™ BT STack porting layer for CYW20829

## Overview
The btstack-integration/COMPONENT_BTSS-IPC is the adaptation layer (porting layer) between AIROC™ BT STack and 
Cypress Abstraction Layers (CYHAL and CYOSAL) for CYW20829 BLE platform where the application and host stack are running on the one CM33 core of CYW20829 and BLE controller firmware is running on another CM33 core of CYW20829.
The porting layer implements routines to provide OS and memory services from platform and facilitates communication with controller over BT-IPC.

## Platform HCI transport config
Application shall call **cybt_platform_config_init( )** API to initialize the porting layer before calling any stack API.
IPC hardware is used to communicate with the controller running on CM33 processor, hence the member variable hci_config.hci_transport of input argument cybt_platform_config_t shall be set to CYBT_HCI_IPC.
Other parameters are ignored by the porting layer.

Please refer to cybt_platform_config.h 

*  structure **cybt_platform_config_t**
*  API **cybt_platform_config_init( )**

## Debug uart configuration
The UART on CYW20829 can be configured for capturing application and HCI traces by calling **cybt_debug_uart_init()** with appropriate configuration in **cybt_debug_uart_config_t**.
Passing a valid function pointer to cybt_debug_uart_data_handler_t allows application to handle any data received over UART. However, it can be set to NULL if the application is not required to handle any data.
Please refer to cybt_debug_uart.h.

*  structure **cybt_debug_uart_config_t**
*  API **cybt_debug_uart_init( )**

## Design 
* To initialize the IPC hardware, porting layer calls Cy_BTIPC_Init API and to reset the BLE controller the Porting layer will assert and desserts the reset bit in the group 3 IP(BTSS) slave control register by using Cy_SysClk_PeriGroupSetSlaveCtl API.
* Porting layer receives HCI events and Command complete events from BLE controller module in IPC buffers via the BT-IPC driver.
* All received events from BLE controller are enqueued in bt_task_queue. This queue is read from bt_task and the event is conveyed to BTSTACK for further processing. The data received from controller in the shared buffer is copied into a porting layer static buffer for processing and the shared buffer is freed by calling Cy_BTIPC_HCI_RelBuffer  API.
* Porting layer sends hci commands and data to BLE controller by getting IPC shared buffer using Cy_BTIPC_HCI_GetWriteBufPtr API and then calling Cy_BTIPC_HCI_Write API to send the data.


## API Reference Manual
 - [Bluetooth platform API manual](https://infineon.github.io/btstack-integration/COMPONENT_BTSS-IPC/docs/api_reference_manual/html/index.html)
 - [Bluetooth stack BLE API manual](https://infineon.github.io/btstack/ble/api_reference_manual/html/index.html)
    
© Infineon Technologies, 2022.
