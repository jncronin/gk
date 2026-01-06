# WiFi Host Driver Board Support Package Integration

## Overview

This library provides some convenience functions for connecting the WiFi Host Driver (WHD) library to a Board Support Package (BSP) that includes a WLAN chip. This library initializes the hardware and passes a reference to the communication interface on the board into WHD. It also sets up the LwIP or NetXDuo based network buffers to be used for sending packets back and forth.

The BSP is expected to set a component indicating what communication interface to use to relay information between the MCU and the WIFI chip. The legal options are restricted based on what MCU and WIFI chips are being used. The selected interface must be supported by both sides. The legal component values are one of:
* WIFI_INTERFACE_SDIO
* WIFI_INTERFACE_SPI
* WIFI_INTERFACE_M2M

Some additional customization of the library is possible by setting defines when building the library. If no custom value is provided, the library will pick a reasonable default value. The following options are available for customization:
* CY_WIFI_COUNTRY: Defines the country this will operate in for wifi initialization parameters. See the wifi-host-driver's whd_country_code_t for legal options.
* CY_WIFI_THREAD_STACK_SIZE: Defines the amount of stack memory available for the wifi thread.
* CY_WIFI_THREAD_PRIORITY: Defines the priority of the thread that services wifi packets. Legal values are defined by the RTOS being used.
* CY_WIFI_HOST_WAKE_SW_FORCE: Defines whether to use the out-of-band pin to allow the WIFI chip to wake up the MCU.
* CY_WIFI_OOB_INTR_PRIORITY: Defines the priority of the interrupt that handles out-of-band notifications from the WIFI chip. Legal values are defined by the MCU running this code.


## Getting Started

To use this library:
1. Implement the cy_network_process_ethernet_data() function. This should be something similar to the example below.
```cpp
/* This needs to be the same item as passed to netifapi_netif_add() */
static struct netif *default_interface = NULL;

void cy_network_process_ethernet_data(whd_interface_t iface, whd_buffer_t buf)
{
    if (default_interface != NULL)
    {
        if (default_interface->input(buf, default_interface) != ERR_OK)
            cy_buffer_release(buf, WHD_NETWORK_RX);
    }
    else
    {
        cy_buffer_release(buf, WHD_NETWORK_RX);
    }
}
```
2. Include a reference to `cybsp_wifi.h`.
3. Call cybsp_wifi_init_primary() to initialize the interface. This needs to be done after having called cybsp_init().

## Features

* APIs for setting up the WHD interface with the BSP's SDIO/SPI/M2M interface.
    * Initialize a primary WiFi interface
    * Optionally initialize a secondary WiFi interface
    * Cleanup interfaces
* APIs for connecting WHD to LwIP/NetXDuo memory buffers (whd_buffer_funcs_t)
* Framework for connecting WHD to LwIP/NetXDuo network interface (whd_netif_funcs_t)

## More information
* [API Reference Guide](https://infineon.github.io/whd-bsp-integration/html/modules.html)
* [Cypress Semiconductor, an Infineon Technologies Company](http://www.cypress.com)
* [Infineon GitHub](https://github.com/infineon)
* [ModusToolbox™](https://www.cypress.com/products/modustoolbox-software-environment)

---
© Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation, 2019-2022.
