# Wi-Fi Connection Manager (WCM)

## What's included?

See the [README.md](./README.md) for a complete description of the Wi-Fi Connection Manager.

## Known issues

| Problem | Workaround |
| ------- | ---------- |
| The `cy_wcm_deinit` API does not bring down the network stack because the default underlying lwIP stack does not have an implementation for deinit. Therefore, the expectation is that `cy_wcm_init` and `cy_wcm_deinit` APIs should be invoked only once. | No workaround. Support will be added in a future release. |
| The `cy_wcm_connect_ap` API does not connect to AP if the MAC address is specified as a connection parameter.| No workaround. This issue will be fixed in future release. |
| IAR 9.40.2 toolchain throws build errors on Debug mode if the application explicitly includes the *iar_dlmalloc.h* file | Add `'--advanced_heap'` to `LDFLAGS` in the application Makefile. |

## Changelog

### v3.6.0

- Enabled 6G support

### v3.5.0

- Added support for CY8CEVAL-062S2-CYW955513SDM2WLIPA kit

### v3.4.0

- Added support for CYW955913EVK-01 kit

### v3.3.1

- Minor bug fixes

### v3.3.0

- WPS changes to support MBEDTLS version 3.4.0
- Added support for CY8CEVAL-062S2-CYW43022CUB kit

### v3.2.0

- Added support for KIT_XMC72_EVK_MUR_43439M2 kit

### v3.1.2

- Minor bug fixes

### v3.1.1

- Updated documentation

### v3.1.0

- Enabled multi-core, virtual API support for the following WCM APIs:
  - `cy_wcm_is_connected_to_ap`
  - `cy_wcm_register_event_callback`
  - `cy_wcm_deregister_event_callback`
- Enabled low power mode for 4390x devices
- Updated documentation
- Added support for new security type `CY_WCM_SECURITY_WPA2_AES_PSK_SHA256`
- General bug fixes


### v3.0.0

- wifi-connection-manager updated to be network stack agnostic

- Added support for CM0P core
- Updated documentation


### v2.3.0

- Added support for WPA3-EXT SAE Supplicant


### v2.2.0

- Added support for the CY8CEVAL-062S2-MUR-43439M2 kit


### v2.1.0

- Added support for CYW943907AEVAL1F and CYW954907AEVAL1F kits


### v2.0.4

- Documentation updates
- Minor updates and improvements added in connect to AP and WPS APIs
- General bug fixes


### v2.0.3

- Added Coverity fixes


### v2.0.2
- Fixed passphrase length check for enterprise security auth types


### v2.0.1

- Minor fixes added in ping API, and in re-connection logic for statically assigned IP
- WEP security support is disabled in connect AP API
- Documentation updates


### v2.0.0

- Added support for SoftAP and concurrent (simultaneous SoftAP + STA) modes
- Documentation updates


### v1.1.0

- Introduced APIs to get the following:
  - Connected AP information
  - WLAN interface statistics
  - IPv6 link-local address
  - Gateway IP address
  - Netmask IP address
  - Gateway MAC address
- Introduced API to ping IPv4 address
- Minor documentation updates


### v1.0.1
- Code snippets added to the documentation


### v1.0.0
- Initial release for Wi-Fi Connection Manager
- Includes support for Wi-Fi Protected Setup (WPS) - Enrollee role
- Exposes Wi-Fi APIs to scan, join, and leave a Wi-Fi network
- Connection monitoring: Monitor active connections and link events. Provides a mechanism to register for event notification. Re-authenticates the connection with the AP in case of intermittent connection loss.


### Supported software and tools

This version of the library was validated for compatibility with the following software and tools:

| Software and tools                                              | Version |
| :---                                                            | :----:  |
| ModusToolbox&trade; software environment                        | 3.2     |
| ModusToolbox&trade; Device Configurator                         | 4.20    |
| GCC Compiler                                                    | 11.3.1  |
| IAR Compiler                                                    | 9.40.2  |
| Arm&reg; Compiler 6                                             | 6.16    |
