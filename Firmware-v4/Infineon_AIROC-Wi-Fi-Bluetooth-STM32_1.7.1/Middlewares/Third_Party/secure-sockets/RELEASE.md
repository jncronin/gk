# Secure Sockets

## What's Included?
Refer to the [README.md](./README.md) for a complete description of the Secure Sockets Library.

## Known Issues
| Problem | Workaround |
| ------- | ---------- |
| Datagram Transport Layer Security (DTLS) is not supported in the Secure Sockets Library | Currently, no workaround is available. Support will be added in a future release. |
| TLS client connection takes around 25 seconds in PKCS mode for RSA keys due to the limitation of hardware crypto for RSA. | It is recommended to use ECDHE keys for the client connection. Connection time improvement for RSA keys will be addressed in a future release. |
| IAR 9.30 toolchain throws build errors on Debug mode, if application explicitly includes iar_dlmalloc.h file | Add '--advance-heap' to LDFLAGS in application Makefile. |

## Changelog

### v3.0.0
* Updated to use [lwIP network interface integration library](https://Infineon.github.io/lwip-network-interface-integration) APIs.
* BINDTODEVICE socket option is extended to support getsockopt.
* Added new API to verify if input is a valid X.509 certificate.
* Added support for CM0P core.
* Documentaton Updates.

### v2.5.0
* Added support for CY8CEVAL-062S2-MUR-43439M2 kit

### v2.4.0
* Added support for CYW943907AEVAL1F and CYW954907AEVAL1F kits.

### v2.3.0
* Added support for secured kit CY8CKIT-064S0S2-4343W.
* Added PKCS11 support for client mode in secure sockets library.
* Added socket-options to select the certificate and keys location (secure element, RAM, etc.) for the socket connection when PKCS mode is enabled.
* General bug fixes.

### v2.2.0
* Introduced a new socket option to get the number of bytes currently available in the socket to read.
* Added socket option to enable or disable TCP no delay feature (TCP_NODELAY socket option).
* Socket disconnect function doesn't delete the socket handle implicitly. Caller should invoke socket delete function to delete the socket handle.
* Socket delete function doesn't delete the client sockets associated with the server socket when it's called for server sockets.
* Integrated Low Power Assistant(LPA) calls with the secure sockets library to wake up network stack on socket operations.

### v2.1.0
* Added socket option for type-of-service(TOS).

### v2.0.0
* Added IPv4 and IPv6 multicast support.
* Added socket options for network interface bind, multicast join, and multicast leave.
* Removed RTC initialization code from the library.
* Enabled peer certificate verification for client sockets by default.
* Added code snippet to get the network time using a UDP socket.
* Introduced a configurable option to override the default library stack size.
* Introduced the troubleshooting section in the documentation.
* Supports disabling the MBEDTLS component for non-secure socket use case.
* General bug fixes.

### v1.1.0
* Added support for UDP sockets.
* Added IPv6 addressing mode for both TCP and UDP sockets.
* Introduced an API to shut down socket send and receive operations.
* Introduced code snippets for UDP sockets and IPv6 addressing mode.

### v1.0.1
* Code snippets added to the documentation

### v1.0.0
* Initial release for Secure Sockets Library
* Provides network abstraction APIs for underlying lwIP stack and mbed TLS library
* The Secure Sockets Library eases application development by exposing a socket-like interface for both secure and non-secure connections.
* Currently, supports TCP/IPv4 connections. UDP and IPv6 support will be added in a future release.
* Only blocking mode is supported in this release. Non-blocking mode will be added in future release.

### Supported Software and Tools
This version of the library was validated for compatibility with the following software and tools:

| Software and Tools                                        | Version |
| :---                                                      | :----:  |
| ModusToolbox&trade; Software Environment                  | 3.0     |
| - ModusToolbox&trade; Device Configurator                 | 4.0     |
| - ModusToolbox&trade; CapSense Configurator / Tuner tools | 5.0     |
| PSoC 6 Peripheral Driver Library (PDL)                    | 3.0.0   |
| GCC Compiler                                              | 10.3.1  |
| IAR Compiler                                              | 9.30    |
| Arm Compiler 6                                            | 6.16    |
