# btstack-integration Overview

The btstack-integration hosts platform adaptation layer (porting layer) between AIROC™ BT Stack and Cypress Abstraction Layers (CYHAL and CYOSAL) for different cypress hardware platforms. This layer implements/invokes the interfaces defined by BTSTACK which is a platform agnostic library, to provide OS services and to enable communication with the BT controller.

While these adaptation layer interfaces and functionality are the same, the hardware platform they run on differ in the inter processor communication mechanisms used for communication between host stack and controller.

This asset provides three components, COMPONENT_BLESS-IPC, COMPONENT_BTSS-IPC and COMPONENT_HCI-UART porting layer components for various hardware platforms (such as psoc6-bless, 20829, psoc6+43xx respectively) and IPC methods (IPC_PIPE, IPC_BTSS and UART respectively) supported. Below table points to further documentation for using each of these components.

Please refer below each COMPONENT's README for more details.

|  Component's name  | Hardware platforms | IPC Method |                                          IPC Method link                                          |                              Portinglayer documentation link                              |
| :-----------------: | :----------------: | :--------: | :------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------: |
| COMPONENT_BLESS-IPC |    psoc6-bless    |  IPC_PIPE  | https://infineon.github.io/mtb-pdl-cat1/pdl_api_reference_manual/html/group__group__ipc__pipe.html | https://github.com/Infineon/btstack-integration/blob/master/COMPONENT_BLESS-IPC/README.md |
| COMPONENT_BTSS-IPC |    psoc6-20829    |  IPC_BTSS  |  https://infineon.github.io/mtb-pdl-cat1/pdl_api_reference_manual/html/group__group__ipc__bt.html  | https://github.com/Infineon/btstack-integration/blob/master/COMPONENT_BTSS-IPC/README.md |
| COMPONENT_HCI-UART |     psoc6-43xx     |    UART    | https://infineon.github.io/mtb-pdl-cat1/pdl_api_reference_manual/html/group__group__scb__uart.html | https://github.com/Infineon/btstack-integration/blob/master/COMPONENT_HCI-UART/README.md |

Please refer IPC (Inter Process Communication) section of https://infineon.github.io/mtb-pdl-cat1/pdl_api_reference_manual/html/index.html for documentation of IPC mechanisms.

## Updates from code size optimization 
### Code size optimizations for Mesh/Beaconing Apps 
Applications which do not create GATT/ACL connections or those which do not need SMP may override the default initializations done in the stack by defining the macro **DISABLE_DEFAULT_BTSTACK_INIT** to 1 in the application Makefile
```
  #Set DISABLE_DEFAULT_BTSTACK_INIT=1 
  DEFINES+=DISABLE_DEFAULT_BTSTACK_INIT=1
```

### Requirements for enabling GATT Robust Caching 
GATT Server applications which need to implement GATT Robust Caching will need to invoke **wiced_bt_gatt_server_enable_caching** in the **BTM_ENABLED_EVT**

### Requirements for enabling GATT Data Signing
GATT applications work with signed data will need to invoke **wiced_bt_gatt_enable_signing** in the **BTM_ENABLED_EVT**

## Platform HCI transport config

Besides of stack settings, HCI configuration is also required to specify,
including pins, format and UART baudrate. Please refer to cybt_platform_config.h
for more detail:

- structure **cybt_platform_config_t**
- API **cybt_platform_config_init( )**

The API **cybt_platform_config_init( )** shall be invoked prior to
**wiced_bt_stack_init( )**

## How to enable trace log?

- *Compile time definition:* Please refer to cybt_platform_trace.h which can set individual trace log level
  - Application makefile add **DEFINES+=CYBT_PLATFORM_TRACE_ENABLE=0** to disable debug log
  - Application makefile add **DEFINES+=CYBT_PLATFORM_TRACE_ENABLE=1** to enable debug log
  - Default set all category is CYBT_TRACE_LEVEL_ERROR
- *Run time update:* Dynamic set trace level by using API **cybt_platform_set_trace_level(id, level);**
  - For example: set all catoegories as debug level
    ```
    cybt_platform_set_trace_level(CYBT_TRACE_ID_ALL, CYBT_TRACE_LEVEL_DEBUG);
    ```

## How to enable BTSpy logs?

- BTSpy is a trace utility that can be used in the AIROC&trade; Bluetooth&reg; platforms to view HCI protocol and generic trace messages from the embedded device
- Add macro ENABLE_BT_SPY_LOG in Makefile or command line
  - `DEFINES+=ENABLE_BT_SPY_LOG`
  - This will automatically enable ENABLE_DEBUG_UART
- Call **cybt_debug_uart_init(&debug_uart_configuration, NULL);**
  - The first argument is the debug_uart_configurations structure pointer, which has hardware pin configurations (refer schematic for getting actual port_pin details) along with baud_rate and flow_control configurations. Recommended baudrate is 3000000, although 115200 is also supported by the BTSpy tool. The second argument is an optional callback function which can be set to NULL
  - Ensure retarget-io is not enabled on the same UART port as BTSpy. There is no need to initialize the retarget-io libray if the application wants to send both application messages and BT protocol traces to the same port through BTSpy
  - Compiler directives can be used to either initialize the retarget-io library or BTSpy logs depending on the debug macro setting. For example:
    ```
    #ifdef ENABLE_BT_SPY_LOG
       {
           #define DEBUG_UART_BAUDRATE 3000000
           #define DEBUG_UART_RTS        (P5_2)
           #define DEBUG_UART_CTS        (P5_3)
           cybt_debug_uart_config_t debug_uart_config = {
                   .uart_tx_pin = CYBSP_DEBUG_UART_TX,
                   .uart_rx_pin = CYBSP_DEBUG_UART_RX,
                   .uart_cts_pin = DEBUG_UART_CTS,
                   .uart_rts_pin = DEBUG_UART_RTS,
                   .baud_rate = DEBUG_UART_BAUDRATE,
                   .flow_control = TRUE
           };
           cybt_debug_uart_init(&debug_uart_config, NULL);
       }
    #else
       cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    #endif
    ```
- Additional steps only for psoc6-bless and 20829
  - On receiving BTM_ENABLED_EVT in Application, register a callback for HCI traces using wiced_bt_dev_register_hci_trace()
   ```
    #ifdef ENABLE_BT_SPY_LOG
        wiced_bt_dev_register_hci_trace(hci_trace_cback);
    #endif
  ```
  - Define the callback function in Application
  ```
    #ifdef ENABLE_BT_SPY_LOG
        void hci_trace_cback(wiced_bt_hci_trace_type_t type,
                     uint16_t length, uint8_t* p_data)
        {
            cybt_debug_uart_send_hci_trace(type, length, p_data);
        }
    #endif
  ```

- If ENABLE_BT_SPY_LOG is not defined in makefile, Application traces can be captured
 in Teraterm
- If HCI protocol traces are not required but BTSpy utility is to be used
for getting generic trace messages, then add ENABLE_DEBUG_UART in Makefile or
command line
  - `DEFINES+=ENABLE_DEBUG_UART`
- Download and use [BTSPY](https://github.com/Infineon/btsdk-utils)
  - Click on serial port setup
  - Select "Enable Serial Port"
  - Select the corrct baudrate, port number and enable HW flow control

## Memory allocations for HCI protocol/trace logs

Enabling ENABLE_BT_SPY_LOG creates two threads for handling TX and RX data over UART. It also allocates a heap whose size is defined by DEBUG_UART_MEMORY_SIZE. Application developers can tweak the value of this MACRO as per Application requirement.

Also, the TX and RX task stack sizes are defined by the below MACROs and are optimized for allowing maximum traces, but can be modified to suit application needs.

* DEBUG_UART_TX_TASK_STACK_SIZE
* DEBUG_UART_RX_TASK_STACK_SIZE

These MACROs are present in cybt_debug_uart.c

|  Component's name  |    Location of cybt_debug_uart.c    |
| :-----------------: | :---------------------------------: |
| COMPONENT_BLESS-IPC | COMPONENT_BLESS-IPC/platform/common |
| COMPONENT_BTSS-IPC | COMPONENT_BTSS-IPC/platform/common |
| COMPONENT_HCI-UART |      COMPONENT_HCI-UART/debug      |

© Infineon Technologies, 2022.
