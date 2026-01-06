# WiFi Host Driver Board Support Package Integration Release Notes
This library helps streamline the process of getting the WiFi Host Driver (WHD) setup and running with a Board Support Package (BSP) that includes a WLAN chip.

### What's Included?
* APIs for setting up the WHD interface with the BSP's SDIO interface.
* APIs for connecting WHD to LwIP/NetXDuo memory buffers (whd_buffer_funcs_t)
* Framework for connecting WHD to LwIP/NetXDuo network interface (whd_netif_funcs_t)

### What Changed?
#### v2.2.1
* Allow BSP to override default configuration of WL_REG_ON and WL_HOST_WAKE pins.
#### v2.2.0
* Fixed issue where some 3.0-style BSPs were inappropriately using SDIO interface.
#### v2.1.0
* Added support for dyanmically allocated NetX Duo packets to support WHD IOVARs with payloads larger than MTU.
* Added support for NetX Duo
* Made SDIO initialization more robust
#### v2.0.0
* Added support for different communication protocols (SDIO, SPI, DMA) between MCU and Radio
* Card initialization process improvements. Code now checks whether SDIO device supports 1.8V signaling, and, if it does, performs the actions needed in order to switch to 1.8V.
* Added support for overriding the default country code by defining CY_WIFI_COUNTRY to a value from whd_country_code_t.
* Added argument to cybsp_wifi_init_primary_extended() to set WHD initialization parameters
* SDIO frequency is increased to 50 MHz if High Speed Mode is supported
* Added support for HAL API v1 or v2
* Minimum required `wifi-host-driver` library version for this release - `2.0.0`
#### v1.2.0
* Fixed possible memory corruption issue introduced with changes from v1.1.0
#### v1.1.2
* Robustness update for WLAN initialization
#### v1.1.1
* Update network buffer allocation for improved performance
#### v1.1.0
* Fixed a memory leak when shutting down the WHD interface
* Minor code style cleanups
#### v1.0.2
* Minor update for documentation & branding
#### v1.0.1
* Fixed issue with a missing ;
#### v1.0.0
* Initial release

### Supported Software and Tools
This version of the WiFi Host Driver Board Support Package Integration was validated for compatibility with the following Software and Tools:

| Software and Tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox™ Software Environment        | 3.0.0   |
| GCC Compiler                              | 11.3.1  |
| IAR Compiler                              | 9.40.2  |
| ARM Compiler                              | 6.16    |

Minimum required ModusToolbox™ Software Environment: v2.0

### More information
Use the following links for more information, as needed:
* [API Reference Guide](https://infineon.github.io/whd-bsp-integration/html/modules.html)
* [Cypress Semiconductor, an Infineon Technologies Company](http://www.cypress.com)
* [Infineon GitHub](https://github.com/infineon)
* [ModusToolbox™](https://www.cypress.com/products/modustoolbox-software-environment)

---
© Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation, 2019-2023.
