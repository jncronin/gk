# AIROC&trade; Bluetooth&reg; Host Stack solution (for FreeRTOS)

## Overview
AIROC&trade; Bluetooth&reg; host stack solution includes Bluetooth&reg; stack library,
Bluetooth&reg; controller firmware and platform/os porting layer. Bluetooth&reg; stack library
is designed for embedded device, it consumes less RAM/ROM usage but still keeps
high performance. With AIROC&trade; Bluetooth&reg; API set, application developers can use them
easily to create their own application. The porting layer is implemented by CYHAL and CY_RTOS_AL
(Hardware/Operation System Adaptation Layer), hence it can adapt to Cypress platforms, and easy to 
port to other vendor's platform.  

## Working flow

### ~ release-v1.3.0
 - Porting layer create 2 tasks
   - **HCI task** which handles HCI packet transmission and reception
   - **BTU/BT task** which handles Bluetooth&reg; core stack and profiles
 - TX path:
   - Bluetooth&reg; stack calls pf_write_xxx_to_lower( ) with packet data
   - Inside those functions,
       - Allocate memory from porting layer heap for the TX packet
       - Put TX packet into HCI task queue
   - HCI task gets TX packet from the queue then write the same to HCI UART
 - RX path:
   - UART driver fires the interrupt when data was coming from Bluetooth&reg; controller
   - In IRQ handler, put the related message to HCI task queue
   - Once HCI task gets the message,
     - Allocate memory from porting layer heap, read the packet from UART and write the same to the allocated memory
     - Put RX packet into Bluetooth&reg; task queue.
   - Bluetooth&reg; task gets RX packet then calls wiced_bt_process_xxx() to notify Bluetooth&reg; stack

### since release-v2.0.0
 - Porting layer create 2 tasks
   - **HCI_TX task** which handles HCI packet from Bluetooth&reg; stack to Bluetooth&reg; controller
   - **HCI_RX task** which handles HCI packet from Bluetooth&reg; controller to Bluetooth&reg; stack
 - TX path:
   - Bluetooth&reg; stack calls pf_write_xxx_to_lower() with packet data
   - Inside those functions,
     - Allocate memory from porting layer heap for the TX packet
     - Put TX packet into HCI_TX task queue
   - HCI_TX task gets TX packet from the queue then write the same to HCI UART
 - RX path:
   - UART driver fires the interrupt when data was coming from Bluetooth&reg; controller
   - In IRQ handler, put the related message to HCI_RX task queue
   - Once HCI_RX task gets the message,
     - Read the packet from UART and write the same to the static buffer
     - Call wiced_bt_process_xxx() to notify Bluetooth&reg; stack

## API Reference Manual
 - [Bluetooth&reg; platform API manual](https://infineon.github.io/btstack-integration/COMPONENT_HCI-UART/docs/api_reference_manual/html/index.html)
 - [Bluetooth&reg; stack BLE API manual](https://infineon.github.io/btstack/ble/api_reference_manual/html/index.html)
    
© Infineon Technologies, 2022.
