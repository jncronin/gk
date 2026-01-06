# lwIP network interface integration library

## Overview

This library is an integration layer that links the lwIP network stack with the underlying Wi-Fi host driver (WHD) and Ethernet driver. This library interacts with FreeRTOS, lwIP TCP/IP stack, Wi-Fi host driver (WHD), and Ethernet driver. It contains the associated code to bind these components together.

*lwIP network interface integration* library is an equivalent of *Wi-Fi middleware core* is now deprecated. *Wi-Fi middleware core* users can use the [porting guide](https://github.com/Infineon/lwip-network-interface-integration/blob/master/porting_guide.md) to migrate the application to use *lwIP network interface integration* library.

## Features and functionality

This library is designed to work with both PSoC&trade; kits with Wi-Fi capability, and XMC&trade; MCU kits with Ethernet capability, and is supported through the ModusToolbox&trade; software environment.

## Supported platforms

This library and its features are supported on the following platforms:

- [PSoC&trade; 6 Wi-Fi Bluetooth&reg; prototyping kit  (CY8CPROTO-062-4343W)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062-4343w/)

- [PSoC&trade; 62S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062S2-43012)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062s2-43012/)

- [PSoC&trade; 6 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062-WiFi-BT)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062-wifi-bt/)

- [PSoC&trade; 64S0S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-064S0S2-4343W)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-064s0s2-4343w/)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-LAI-4373M2)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)

- [CYW954907AEVAL1F evaluation kit (CYW954907AEVAL1F)](https://www.infineon.com/cms/en/product/evaluation-boards/cyw954907aeval1f/)

- [CYW943907AEVAL1F Evaluation Kit (CYW943907AEVAL1F)](https://www.infineon.com/cms/en/product/evaluation-boards/cyw943907aeval1f/)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-MUR-43439M2)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)

- [XMC7200D-E272K8384 kit (KIT-XMC72-EVK)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc72_evk/)

- [XMC4700 Relax Kit (KIT_XMC47_RELAX_V1)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc47_relax_v1/)

- [XMC4800 Relax EtherCAT Kit (KIT_XMC48_RELAX_ECAT_V1)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc48_relax_ecat_v1/)

- [XMC7200D-E272K8384 kit (KIT_XMC72_EVK_MUR_43439M2)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc72_evk/)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-CYW43022CUB)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)

## Log messages

By default, the lwIP network interface integration library disables all debug log messages. Do the following to enable log messages:

1. Add the `ENABLE_CONNECTIVITY_MIDDLEWARE_LOGS` macro to the `DEFINES` in the code example's Makefile. The Makefile entry should look like as follows:
   ```
   DEFINES+=ENABLE_CONNECTIVITY_MIDDLEWARE_LOGS
   ```
2. Call the `cy_log_init()` function provided by the *cy-log* module. cy-log is part of the *connectivity-utilities* library. See [connectivity-utilities library API documentation](https://infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html) for cy-log details.

## Dependencies

The lwIP network interface integration library depends on the following:

- [lwIP FreeRTOS integration](https://github.com/Infineon/lwip-freertos-integration)

- [lwIP](https://savannah.nongnu.org/projects/lwip/)

To ensure that all required libraries are fetched, include [ethernet-core-freertos-lwip-mbedtls](https://github.com/Infineon/ethernet-core-freertos-lwip-mbedtls) library depending on the application.

- *For Wi-Fi applications:*

   Create the *wifi-core-freertos-lwip-mbedtls.mtb* file with the following content:

   `https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls#latest-v1.X#\$$ASSET_REPO$$/wifi-core-freertos-lwip-mbedtls/latest-v1.X`

- *For Ethernet applications:*

   Create the *ethernet-core-freertos-lwip-mbedtls.mtb* file with the following content:

   `https://github.com/Infineon/ethernet-core-freertos-lwip-mbedtls#latest-v1.X#\$$ASSET_REPO$$/ethernet-core-freertos-lwip-mbedtls/latest-v1.X`

- Run `make getlibs` to fetch all required libraries including the lwip-network-interface-integration library.

## Additional information

- [lwIP network interface integration RELEASE.md](./RELEASE.md)

- [lwIP network interface integration API reference guide](https://infineon.github.io/lwip-network-interface-integration/api_reference_manual/html/index.html)

- [Porting guide for lwIP network interface integration](https://github.com/Infineon/lwip-network-interface-integration/blob/master/porting_guide.md)

- [ModusToolbox&trade; software environment, quick start guide, documentation, and videos](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)

- [lwIP](https://savannah.nongnu.org/projects/lwip/)

- [lwIP network interface integration version](./version.xml)
