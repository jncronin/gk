## Introduction
The connectivity utilities library is a collection of general purpose middleware utilities. Several connectivity middleware libraries shall depend on this utilities library. 
Currently, the following utilities are included:

## Features
### JSON parser
The JSON format is often used for serializing and transmitting structured data over a network connection. It is used primarily to transmit data between a server and web application, serving as an alternative to XML. JSON is JavaScript Object Notation. The JSON parser utility library provides helper functions to parse JSON objects and calls the function registered by the caller.

Refer to the [cy_json_parser.h](./JSON_parser/cy_json_parser.h) for API documentation

### Linked list
This is a generic linked list library with helper functions to add, insert, delete and find nodes in a list.

Refer to the [cy_linked_list.h](./linked_list/cy_linked_list.h) for API documentation

### String utilities
The string utilities module is a collection of string conversion helpers to convert between integer and strings.

Refer to the [cy_string_utils.h](./cy_string/cy_string_utils.h) for API documentation

### Network helpers
This is a collection of network helper functions to fetch IPv4 address of the local device, notify IPv4 address change via callback and conversion utilities.

Refer to the [cy_nw_helper.h](./network/cy_nw_helper.h) for API documentation

### Logging functions
This module is a logging subsystem that allows run time control for the logging level. Log messages are passed back to the application for output. A time callback can be provided by the application for the timestamp for each output line. Log messages are mutex protected across threads so that log messages do not interrupt each other.

Refer to the [cy_log.h](./cy_log/cy_log.h) for API documenmtation

### Middleware Error codes
The utilities library includes a collection of middleware error codes for various middleware libraries such as AWS IoT, HTTP server, Enterprise security etc.

Refer to [cy_result_mw.h](./cy_result_mw.h) for details

## Supported platforms
This middleware library and its features are supported on following Infineon platforms:
* [PSoC&trade; 6 Wi-Fi Bluetooth&reg; prototyping kit  (CY8CPROTO-062-4343W)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062-4343w/)
* [PSoC&trade; 62S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062S2-43012)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062s2-43012/)
* [PSoC&trade; 6 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062-WiFi-BT)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062-wifi-bt/)
* [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-LAI-4373M2)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)
* [CYW954907AEVAL1F evaluation kit (CYW954907AEVAL1F)](https://www.infineon.com/cms/en/product/evaluation-boards/cyw954907aeval1f/)
* [CYW943907AEVAL1F Evaluation Kit (CYW943907AEVAL1F)](https://www.infineon.com/cms/en/product/evaluation-boards/cyw943907aeval1f/)
* [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-MUR-43439M2)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)
* [XMC7200D-E272K8384 kit (KIT-XMC72-EVK)](https://www.infineon.com/KIT_XMC72_EVK)
* [XMC4700 Relax Kit (KIT_XMC47_RELAX_V1)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc47_relax_v1/)
* [XMC4800 Relax EtherCAT Kit (KIT_XMC48_RELAX_ECAT_V1)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc48_relax_ecat_v1/)
* [XMC7200D-E272K8384 kit (KIT_XMC72_EVK_MUR_43439M2)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc72_evk/)
* [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-CYW43022CUB)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)
* [CYW955913EVK-01 Wi-Fi Bluetooth&reg; Prototyping Kit (CYW955913EVK-01)](https://www.infineon.com/CYW955913EVK-01)

## Integration Notes
* The connectivity utilities library has been designed to work with both the ARM mbed ecosystem and ModusToolbox&trade; environment. 
* It is adequate to include this library in the desired ecosystem to use these utilities. Depending on the ecosystem, the respective source files will get picked up and linked. This is accomplished using the COMPONENT_ model.
* In order to ease integration of Wi-Fi connectivity components to code examples, this connectivity utilities library has been bundled into the [Wi-Fi core Freertos lwIP mbedtls library](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls). Similarly for Ethernet, this connectivity utilities library has been bundled into the [Ethernet core Freertos lwIP mbedtls library](https://github.com/Infineon/ethernet-core-freertos-lwip-mbedtls)
* For mbed ecosystem, this library has to be included by the respective code examples.
* NOTE: Refer to the COMPOMENT_ folders for implementation details pertinent to the ecosystem. For instance, certain network helper functions are not implemented on AnyCloud, and are leveraged from Wi-Fi Connection Manager

## Additional Information
* [Connectivity Utilities RELEASE.md](./RELEASE.md)
* [Connectivity Utilities API reference guide](https://Infineon.github.io/connectivity-utilities/api_reference_manual/html/index.html)
* [Connectivity Utilities version](./version.xml)
* [lwIP network interface integration library](https://Infineon.github.io/lwip-network-interface-integration)

