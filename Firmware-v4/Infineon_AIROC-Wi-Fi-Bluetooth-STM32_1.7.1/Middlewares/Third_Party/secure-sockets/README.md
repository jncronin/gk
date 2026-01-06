# Secure sockets library

The secure sockets library provides APIs to create software that can send and/or receive data over the network using sockets. This library supports both secure and non-secure sockets, and abstracts the complexity involved in directly using network stack and security stack APIs. This library supports both IPv4 and IPv6 addressing modes for UDP and TCP sockets.

To ease the integration of Wi-Fi connectivity components, this secure sockets library has been bundled into the [Wi-Fi core freertos lwip mbedtls library](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls).



## Features and functionality

- Supports non-secure TCP and UDP sockets

- Secure TCP (TLS) socket communication using Mbed TLS library

- Supports both IPv4 and IPv6 addressing. Only link-local IPv6 addressing is supported

- Supports UDP multicast and broadcast for both IPv4 and IPv6

- Thread-safe APIs

- Provides APIs for both Client and Server mode operations

- Supports both Synchronous and Asynchronous APIs for receiving data on a socket

- Asynchronous Server APIs for accepting client connections

- Provides a socket-option API to configure send/receive timeout, callback for asynchronous mode, TCP keepalive parameters, certificate/key, and TLS extensions

- Integrated with PSA Lib through the PKCS interface to support secure client TCP (TLS) connection using the device certificate and device keys provisioned in the secured element

## Quick Start
* To use secure-sockets library for FreeRTOS, lwIP, and Mbed TLS, the application should pull [wifi-core-freertos-lwip-mbedtls](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls) library which will internally pull secure-sockets, wifi-connection-manager, FreeRTOS, lwIP, Mbed TLS and other dependent modules.
To pull wifi-core-freertos-lwip-mbedtls create the following *.mtb* file in deps folder.
   - *wifi-core-freertos-lwip-mbedtls.mtb:* https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls#latest-v1.X#$$ASSET_REPO$$/wifi-core-freertos-lwip-mbedtls/latest-v1.X
* A set of pre-defined configuration files have been bundled with the wifi-core-freertos-lwip-mbedtls library for FreeRTOS, lwIP, and Mbed TLS. The developer is expected to review the configuration and make adjustments. See the "Quick Start" section in [README.md](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls/blob/master/README.md).

* A set of COMPONENTS must be defined in the code example project's Makefile for this library. See the "Quick Start" section in [README.md](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls/blob/master/README.md).

* The secure-sockets library disables all the debug log messages by default. To enable log messages, the application must perform the following:

 - Add the `ENABLE_SECURE_SOCKETS_LOGS` macro to the *DEFINES* in the code example's Makefile. The Makefile entry would look like as follows:
   ```
   DEFINES+=ENABLE_SECURE_SOCKETS_LOGS
   ```
 
 - Call the `cy_log_init()` function provided by the *cy-log* module. cy-log is part of the *connectivity-utilities* library. 
 
 - See [connectivity-utilities library API documentation](https://Infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html).


## Supported platforms

This library and its features are supported on the following Infineon MCUs:

- [PSoC&trade; 6 Wi-Fi Bluetooth&reg; prototyping kit (CY8CPROTO-062-4343W)](https://www.cypress.com/documentation/development-kitsboards/psoc-6-wi-fi-bt-prototyping-kit-cy8cproto-062-4343w)

- [PSoC&trade; 62S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062S2-43012)](https://www.cypress.com/documentation/development-kitsboards/psoc-62s2-wi-fi-bt-pioneer-kit-cy8ckit-062s2-43012)

- [PSoC&trade; 6 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062-WiFi-BT)](https://www.cypress.com/documentation/development-kitsboards/psoc-6-wifi-bt-pioneer-kit-cy8ckit-062-wifi-bt)

- [PSoC&trade; 64S0S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-064S0S2-4343W)](https://www.cypress.com/documentation/development-kitsboards/psoc-64-standard-secure-aws-wi-fi-bt-pioneer-kit-cy8ckit)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-LAI-4373M2)](https://www.cypress.com/documentation/development-kitsboards/psoc-62s2-evaluation-kit-cy8ceval-062s2)

- [CYW954907AEVAL1F Evaluation Kit(CYW954907AEVAL1F)](https://www.cypress.com/documentation/development-kitsboards/cyw954907aeval1f-evaluation-kit)

- [CYW943907AEVAL1F Evaluation Kit(CYW943907AEVAL1F)](https://www.cypress.com/documentation/development-kitsboards/cyw943907aeval1f-evaluation-kit)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-MUR-43439M2)](https://www.cypress.com/documentation/development-kitsboards/psoc-62s2-evaluation-kit-cy8ceval-062s2)

## Send and receive timeout values

The secure sockets library configures the default send and receive timeout values to 10 seconds for a newly created socket. These can be changed using the `cy_socket_setsockopt` API function. To change the send timeout, use the `CY_SOCKET_SO_SNDTIMEO` socket option; similarly, for receive timeout, use the `CY_SOCKET_SO_RCVTIMEO` socket option. Adjust the default timeout values based on the network speed or use case.


## TCP/IP and security stacks

The secure sockets library has been designed to support different flavors of the TCP/IP stack or security stack. Currently, lwIP and Mbed TLS are the default network and security stacks respectively. Therefore, any application that uses the secure sockets library must ensure that the following COMPONENTS are defined in the code example project's Makefile.

To do so, add `LWIP` and `MBEDTLS` components to the Makefile. The Makefile entry would look like as follows:

  ```
  COMPONENTS+=LWIP MBEDTLS
  ```

Applications using the secure sockets library must include only the *cy_secure_sockets.h* file for non-secure connections. For secure connections, the application must include both *cy_secure_sockets.h* and *cy_tls.h* header files.


## Stack size

The default stack size of the secure sockets library is 6 KB (6*1024). To customize the stack size add the `SECURE_SOCKETS_THREAD_STACKSIZE` macro to the `DEFINES` in the code example's Makefile with the required stack size. The Makefile entry would look like as follows:

  ```
  DEFINES+=SECURE_SOCKETS_THREAD_STACKSIZE=8*1024
  ```

## Validity period verification

The default Mbed TLS configuration provided by the *Wi-Fi middleware core library* disables the validity period verification of the certificates. To perform this verification, enable `MBEDTLS_HAVE_TIME_DATE` in the [mbedtls_user_config.h](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls/blob/master/configs/mbedtls_user_config.h) file.

Ensure that the system time is set prior to the `cy_socket_connect()` function call. To set the system time, get the time from the NTP server and set the system's RTC time using `cyhal_rtc_init()`, `cyhal_rtc_write()` and `cy_set_rtc_instance()` functions. See the [time support details](https://github.com/Infineon/clib-support/blob/master/README.md#time-support-details) for reference.

See the code snippet given in [secure sockets API documentation](https://Infineon.github.io/secure-sockets/api_reference_manual/html/index.html) to get the time from the NTP server.


## PKCS/Non-PKCS mode

Secure sockets library can be built using PKCS and Non-PKCS mode on secure platform such as CY8CKIT-064S0S2-4343W. When Secure sockets library is built with PKCS flow, the certificates and keys can be provisioned in the secure element of the platform, and the provisioned certificates and keys will be read/used by secure sockets library while establishing the TLS connection with the peer. On the other hand, in non-PKCS mode, the certificates/keys will be passed from the application stored in flash/RAM.

### Non-PKCS mode

1. Provision the kit. See [Device provisioning steps](https://community.cypress.com/t5/Resource-Library/Provisioning-Guide-for-the-Cypress-CY8CKIT-064S0S2-4343W-Kit/ta-p/252469).

2. Add `CY_TFM_PSA_SUPPORTED` and `TFM_MULTI_CORE_NS_OS` to the `DEFINES` in the application's Makefile. The Makefile entry would look like as follows:
   ```
   DEFINES+=CY_TFM_PSA_SUPPORTED TFM_MULTI_CORE_NS_OS
   ```

### PKCS mode

1. Provision the kit. See [Device provisioning steps](https://community.cypress.com/t5/Resource-Library/Provisioning-Guide-for-the-Cypress-CY8CKIT-064S0S2-4343W-Kit/ta-p/252469).

2. Provision device certificate/RootCA certificate. Add/modify the respective policy *.json* file with the device and RootCA certificate path to be provisioned to the secured element as follows, and then re-provision the kit:

   ```
   "provisioning:"
    {
       "chain_of_trust": ["../certificates/device_cert.pem", "../certificates/rootCA.pem"]
    },
   ```


#### **Dependencies**

The secure sockets library depends on the other libraries for PKCS support. Ensure that the following  libraries are pulled in by creating the following *.mtb* files:

   - *aws-iot-device-sdk-embedded-C.mtb:* https://github.com/aws/aws-iot-device-sdk-embedded-C/#202103.00#$$ASSET_REPO$$/aws-iot-device-sdk-embedded-C/202103.00

   - *freertos-pkcs11-psa.mtb:* https://github.com/Linaro/freertos-pkcs11-psa/#80292d24f4978891b0fd35feeb5f1d6f6f0fff06#$$ASSET_REPO$$/freertos-pkcs11-psa/master


##### ***Pull required libraries and enable PKCS mode***
1. Execute the `make getlibs` command to pull the required libraries created as .mtb.

2. Add the `CY_TFM_PSA_SUPPORTED`, `TFM_MULTI_CORE_NS_OS` and `CY_SECURE_SOCKETS_PKCS_SUPPORT` macros to the `DEFINES` in the code example's Makefile. The Makefile entry would look like as follows:

   ```
    DEFINES+=CY_TFM_PSA_SUPPORTED TFM_MULTI_CORE_NS_OS CY_SECURE_SOCKETS_PKCS_SUPPORT
   ```

##### ***Trusted firmware library include path***

To compile the FreeRTOS PKCS PSA integration library, add the trusted firmware library include path before the MBEDTLS library include path. Add the following lines to the Makefile.

   ```
    INCLUDES=$(SEARCH_trusted-firmware-m)/COMPONENT_TFM_NS_INTERFACE/include
    INCLUDES+=./libs/trusted-firmware-m/COMPONENT_TFM_NS_INTERFACE/include
   ```

##### ***Configuration for PKCS11***

A pre-defined configuration file *core_pkcs11_config.h* is bundled with the secure sockets library. To change the default configuration for PKCS11, copy the *core_pkcs11_config.h* file from the secure sockets library to the top-level application directory, and then modify it.

[FreeRTOS PSA PKCS11](https://github.com/Linaro/freertos-pkcs11-psa/) implementation supports only SHA-256 hashing algorithm. So the application should chose the cipher suite list compatible for SHA-256. To chose the cipher suite list(compatible for SHA-256), application need to copy *mbedtls_user_config.h* file from *libs/wifi-core-freertos-lwip-mbedtls/configs* to root folder and add required cipher suites to the `MBEDTLS_SSL_CIPHERSUITES` macro.


To use secure-sockets library for FreeRTOS, lwIP, and Mbed TLS, pull [wifi-core-freertos-lwip-mbedtls](https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls) library which will internally pull secure-sockets, wifi-connection-manager, FreeRTOS, lwIP, mbed TLS and other dependent moduleswifi-core-freertos-lwip-mbedtls will internally pull secure-sockets and other dependent modules.


## Additional information

- [Secure sockets RELEASE.md](./RELEASE.md)

- [Secure sockets API documentation](https://Infineon.github.io/secure-sockets/api_reference_manual/html/index.html)

- [Connectivity utilities API documentation - for cy-log details](https://Infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html)

- [ModusToolbox&trade; software environment, quick start guide, documentation, and videos](https://www.cypress.com/products/modustoolbox-software-environment)

- [Secure sockets version](./version.xml)

- [ModusToolbox&trade; code examples](https://github.com/infineon?q=mtb-example-anycloud%20NOT%20Deprecated)
