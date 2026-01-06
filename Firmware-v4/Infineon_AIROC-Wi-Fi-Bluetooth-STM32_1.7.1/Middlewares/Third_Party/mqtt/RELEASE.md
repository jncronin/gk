# MQTT client library

## What's included?

See the [README.md](./README.md) for a complete description of the MQTT client library.


## Known issues

| Problem | Workaround |
| ------- | ---------- |
| IAR 9.30 toolchain throws build errors on Debug mode if the application explicitly includes the *iar_dlmalloc.h* file | Add `--advance-heap` to LDFLAGS in the application Makefile. |
| MQTT connection fails over TLS 1.3 with brokers that enabled session tickets. For example, MQTT Eclipse broker ("mqtt.eclipseprojects.io") | Currently there is no workaround as there is a bug in Mbed TLS library in session ticket parsing logic. |

## Changelog

### v4.3.0

- Supports Mbed TLS version 3.4.0
- Supports MQTT connection with TLS version 1.3
- Added support for CY8CEVAL-062S2-CYW43022CUB kit

### v4.2.0

- Minor bug fixes.
- Added support for KIT_XMC72_EVK_MUR_43439M2 kit

### v4.1.0

- Added support for KIT_XMC72_EVK kit

### v4.0.0

- Updated FreeRTOS-specific code to make it generic
- Added new APIs and updated signature of some of the old APIs to support multicore virtualization
- Enabled multi-core, virtual API support for the following MQTT APIs:
   - `cy_mqtt_get_handle`
   - `cy_mqtt_register_event_callback`
   - `cy_mqtt_deregister_event_callback`
   - `cy_mqtt_publish`
   - `cy_mqtt_subscribe`
   - `cy_mqtt_unsubscribe`
- Updated documentation

This version of the library is not backward-compatible with pervious library versions.


### v3.4.2

- Removed unwanted dependencies from the deps folder
- Added support for CM0P core
- Minor documentation updates


### v3.4.1

- Documentation updates


### v3.4.0

- Added support for CY8CEVAL-062S2-MUR-43439M2 kit


### v3.3.0

- Added support for CYW943907AEVAL1F and CYW954907AEVAL1F kits


### v3.2.0

- Added support for secured kits (for example: CY8CKIT-064S0S2-4343W)

- Integrated with secure sockets PKCS support

- Added support for Low power assistant (LPA) applications

- Upgraded the library to integrate with the wifi-mw-core 3.1.1 library version

- General bug fixes


### v3.1.1
- Documentation updates

- Upgraded the library to integrate the with wifi-mw-core 3.x library version for any cloud

- Introduced ARMC6 compiler support for any cloud build


### v3.1.0
- Performance improvements

- Documentation updates


### v3.0.0
- Supports QoS-2

- Fully compliant with MQTT spec v3.1.1

- Provides simplified C APIs

- Integrated with the latest AWS IoT Device C SDK library version #202011.00

- This version of the library is not backward-compatible with pervious library versions


### v2.1.0
-  Introduced asynchronous receive logic

-  Added support for SAS token-based authentication for Azure broker connection


### v2.0.0

- Changes to adapt to ModusToolbox&trade; 2.2.0 flow and any cloud's support for multiple Wi-Fi interfaces - STA, softAP, and concurrent STA+softAP mode


### v1.0.1

- Code snippets added to the documentation


### v1.0.0

- Initial release of the MQTT client library


### Supported software and tools

This version of the library was validated for compatibility with the following software and tools:

| Software and tools                                             | Version |
| :---                                                           | :----:  |
| ModusToolbox&trade; software environment                       | 3.1     |
| ModusToolbox&trade; device configurator                        | 4.10    |
| GCC compiler                                                   | 11.3.1  |
| IAR compiler                                                   | 9.30    |
| Arm&reg; compiler 6                                            | 6.16    |
