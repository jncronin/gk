#AIROC™ BT STack porting layer for PSoC6 BLESS

## Overview
The btstack-integration/COMPONENT_BLESS-IPC is the adaptation layer (porting layer) between AIROC™ BT STack and 
Cypress Abstraction Layers (CYHAL and CYOSAL). This porting layer is to be used in conjunction with Cypress PSOC 6 BLE platform where the application and host stack are running on the CM4 core of PSOC and BLESS controller FW is running on CM0 core of PSOC.
The porting layer implements routines to provide OS and memory services from platform and facilitates communication with controller.

## Platform HCI transport config
Application shall call **cybt_platform_config_init( )** API to initialize the porting layer before calling any stack API.
IPC Pipe is used to communicate with the controller running on CM0 processor, hence the member variable hci_config.hci_transport of input argument cybt_platform_config_t shall be set to CYBT_HCI_IPC.
Other parameters are ignored by the porting layer.

Please refer to cybt_platform_config.h 

*  structure **cybt_platform_config_t**
*  API **cybt_platform_config_init( )**

## Debug uart configuration
The UART on PSOC can be configured for capturing application and HCI traces by calling **cybt_debug_uart_init()** with appropriate configuration in **cybt_debug_uart_config_t**.
Passing a valid function pointer to cybt_debug_uart_data_handler_t allows application to handle any data received over UART. However, it can be set to NULL if the application is not required to handle any data.
Please refer to cybt_debug_uart.h.

*  structure **cybt_debug_uart_config_t**
*  API **cybt_debug_uart_init( )**

## Design 
* To initialize BLESS controller, Porting layer will send ENABLE_CNTR_CMD IPC msg to BLESS controller module via CYPIPE IPC mechanism
* Porting layer can receive the HCI events and Command complete events from BLESS controller module as IPC msgs via CYPIPE. 
* All received events from BLESS controller module are queued in bt_task_queue. bt_task thread in porting layer would process the msgs and make calls to BTStack for further processing.
* Porting layer could send hci commands and data by filling IPC_HOST_MSG structure with details and call IPC_Pipe_SendMsg api with destination as CM0 address.while sending the cmds to controller, HOST has to allocate memory for the cmd and its parameters
* The received ptrs in events from controller , can be mapped to IPC_CNTR_MSG , processed in bt_task. After processing the event , HOST needs to a post a msg to Controller to free up the memory.

## API Reference Manual
 - [Bluetooth platform API manual](https://infineon.github.io/btstack-integration/COMPONENT_BLESS-IPC/docs/api_reference_manual/html/index.html)
 - [Bluetooth stack BLE API manual](https://infineon.github.io/btstack/ble/api_reference_manual/html/index.html)
    
© Infineon Technologies, 2022.
