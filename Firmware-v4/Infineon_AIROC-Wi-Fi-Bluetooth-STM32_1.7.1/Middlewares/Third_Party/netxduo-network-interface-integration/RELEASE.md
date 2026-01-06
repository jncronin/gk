# NetXDuo network interface integration library

## What's Included?

See the [README.md](./README.md) for a complete description of the [NetXDuo network interface integration](https://github.com/Infineon/netxduo-network-interface-integration) library.

## Known Issues
| Problem | Workaround |
| ------- | ---------- |
| DHCP negotiation fails for the wifi interface type CY_NETWORK_WIFI_AP_INTERFACE when used with GCC 11.3.1, which inturn causes wifi-connection-manager SoftAP failure | Add '-fno-strict-aliasing' to CFLAGS in application Makefile |

## Changelog

### v1.3.0

- Added support for CYW955913EVK-01 kit.

### v1.2.0

- Added support for IOCTL buffers greater than MTU size.
- Updated random number generate logic to eliminate use of HAL trng APIs

### v1.1.1

- Moved netxduo and netxsecure configuration files to wifi-core-threadx-netxduo-netxsecure repo.

### v1.1.0

- Upgraded to netxduo v6.2.0_rel
- Added true random number generation function
- Updated DNS address retrieval from DHCP configuration

### v1.0.0

- Initial release for NetXDuo network interface integration library

- Adds support for WiFi capability capability on NetXDuo TCP/IP stack

### Supported software and tools

This version of the library was validated for compatibility with the following software and tools:

| Software and tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox&trade; software environment  | 3.2     |
| ModusToolbox&trade; Device Configurator   | 4.20    |
| GCC compiler                              | 11.3.1  |
| IAR compiler                              | 9.40.2  |
| Arm&reg; compiler 6                       | 6.16    |


## Additional information

- [NetXDuo network interface integration library README.md](./README.md)

- [NetXDuo network interface integration API reference guide](https://infineon.github.io/netxduo-network-interface-integration/api_reference_manual/html/index.html)

- [ModusToolbox&trade; software environment, quick start guide, documentation, and videos](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)
