# NetXDuo network interface integration library

## Overview

This library is an integration layer that links the NetXDuo network stack with the underlying Wi-Fi host driver (WHD). This library interacts with ThreadX, NetXDuo TCP/IP stack, and Wi-Fi host driver (WHD). It contains the associated code to bind these components together.

## Features and functionality

This library is designed to work with both PSoC&trade; kits with Wi-Fi capability, and is supported through the ModusToolbox&trade; software environment.

## Supported platforms

This library and its features are supported on the following platforms:

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-LAI-4373M2)](https://www.cypress.com/documentation/development-kitsboards/psoc-62s2-evaluation-kit-cy8ceval-062s2)

- [PSoC&trade; 62S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062S2-43012)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062s2-43012/)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-MUR-43439M2)](https://www.cypress.com/documentation/development-kitsboards/psoc-62s2-evaluation-kit-cy8ceval-062s2)

- [CYW955913EVK-01 Wi-Fi Bluetooth&reg; Prototyping Kit (CYW955913EVK-01)](https://www.infineon.com/CYW955913EVK-01)

## Log messages

By default, the NetXDuo network interface integration library disables all debug log messages. Do the following to enable log messages:

1. Add the `ENABLE_CONNECTIVITY_MIDDLEWARE_LOGS` macro to the `DEFINES` in the code example's Makefile. The Makefile entry should look like as follows:
   ```
   DEFINES+=ENABLE_CONNECTIVITY_MIDDLEWARE_LOGS
   ```
2. Call the `cy_log_init()` function provided by the *cy-log* module. cy-log is part of the *connectivity-utilities* library. See [connectivity-utilities library API documentation](https://infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html) for cy-log details.

## Additional information

- [NetXDuo network interface integration RELEASE.md](./RELEASE.md)

- [NetXDuo network interface integration API reference guide](https://infineon.github.io/netxduo-network-interface-integration/api_reference_manual/html/index.html)

- [ModusToolbox&trade; software environment, quick start guide, documentation, and videos](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)

- [NetXDuo network interface integration version](./version.xml)
