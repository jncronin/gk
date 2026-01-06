# Command console library

## What's included?

See the [README.md](./README.md) for a complete description of the command console library.

## Known issues
| Problem | Workaround |
| ------- | ---------- |
| Running iperf command in client UDP mode with the `-i 1` (interval) option reports a lower throughput. | Run the iPerf without the `-i` (interval) option (or) use an interval value greater than 1 second. |
| IAR 9.40.2 toolchain throws build errors on Debug mode, if application explicitly includes iar_dlmalloc.h file | Add '--advanced_heap' to LDFLAGS in application Makefile. |
| Running iperf command without network connection will lead to memory leak. This memory leak can lead to unpredicted behaviour. | Ensure the network connection is up before issuing any iperf commands. |

## Changelog

### v6.0.0
- Removed btstack-integration from command-console dependency.
- Added support for CYW955913EVK-01 kit

### v5.2.1
- Fixed minor compilation warning for ARM compiler with Keil IDE.

### v5.2.0
- Added support for CY8CEVAL-062S2-CYW43022CUB kit

### v5.1.0
- Added new command get_sta_ifconfig which will provide IP & MAC address of STA
- Added support for KIT_XMC72_EVK_MUR_43439M2 kit

### v5.0.0
- Removed support for AFR and MBED OS
- Added support for new security type wpa2_aes_sha256
- Minor bug fixes

### v4.1.1

- Minor bug fixes

### v4.1.0

- Added support for KIT-XMC72-EVK kit
- Fixed build warnings with IAR 9.30 toolchain
- Minor Documentation updates

### v4.0.0

- Added ethernet utility support
- Added support for CM0P core
- Minor Documentation updates

### v3.2.0

- Added support for CYW954907AEVAL1F and CYW943907AEVAL1F kits

### v3.1.0

- Added support for CY8CEVAL-062S2-MUR-43439M2 kit

### v3.0.1

- Renamed Wi-Fi command names

- Build switch provided to disable command sets

### v3.0.0

- Minor documentation and BT configuration updates

- Fix added for smartcoex LE disconnection issue

- Introduced ARMC6 toolchain build support for AnyCloud

- Integrated with BTSTACK library version v3.x

### v2.0.0

- Added support of the command console library for the FreeRTOS framework

- Added Wi-Fi diagnostic commands to perform network operations

- Added BT diagnostic commands to perform Bluetooth&reg; LE operations

### v1.0.1

- Documentation updates

- For the IAR toolchain, added a fix to echo the commands as they are typed

### v1.0.0

- This is the first version of the command console library for Mbed OS and AnyCloud

## Supported software and tools

The current version of the library was validated for compatibility with the following software and tools:

| Software and tools                                      | Version |
| :---                                                    | :----:  |
| ModusToolbox&trade; software                            | 3.2     |
| ModusToolbox&trade; Device Configurator                 | 4.20    |
| GCC Compiler                                            | 11.3.1  |
| IAR Compiler (only for ModusToolbox&trade;)             | 9.40.2  |
| Arm® Compiler 6                                         | 6.16    |
