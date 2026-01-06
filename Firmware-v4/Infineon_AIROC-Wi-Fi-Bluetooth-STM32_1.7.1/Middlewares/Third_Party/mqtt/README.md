# MQTT client library

This repo contains the MQTT client library that can work with the family of Infineon connectivity devices. This library uses the AWS IoT device SDK MQTT client library and implements the glue layer that is required for the library to work with Infineon connectivity platforms.

The ModusToolbox&trade; MQTT client code examples download this library automatically, so you don't need to.

Existing MQTT application users who are using MQTT 3.X can use the [porting guide]( https://github.com/Infineon/mqtt/blob/master/porting_guide.md ) to migrate to MQTT v4.0.

## Multi-core architecture and virtual API support

The library supports multi-core architecture by making a subset of APIs available as virtual APIs. The virtualization of the MQTT library helps to access the MQTT APIs defined in one core from the other core using Inter Process Communication (IPC). MQTT can now be run on two cores simultaneously, with one core containing the full stack (primary core) and the other core containing the subset of virtual-only APIs (secondary core).

The virtual APIs pipe the API requests over IPC to the primary core where the API is actually executed and the result is passed back to the secondary core. This virtualization abstracts out the implementation details and complexity of IPC, thus making multi-core connectivity application development simple.

For more information on virtualization, see the [Virtual Connectivity Manager]( https://github.com/Infineon/virtual-connectivity-manager ) library.

## Features

All features supported by the [AWS IoT device SDK MQTT library]( https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/202103.00 ) are supported by this library.

Some of the key features include:

- Supports Wi-Fi and Ethernet connections

- MQTT 3.1.1 client

- Synchronous API for MQTT operations

- Multi-threaded API by default

- Complete separation of MQTT and network stack, allowing MQTT to run on top of any network stack

- Supports MQTT persistent sessions

- Supports Quality-of-Service (QoS) levels 0, 1, and 2

- Supports MQTT connections over both secured and non-secured TCP connections

- Supports authentication based on both X509 certificate and SAS tokens for MQTT connection with Azure broker

- Glue layer implementation for MQTT library to work on Infineon connectivity platforms

- Supports multi-core architecture by providing the following APIs as virtual APIs:
  - `cy_mqtt_get_handle`
  - `cy_mqtt_register_event_callback`
  - `cy_mqtt_deregister_event_callback`
  - `cy_mqtt_publish`
  - `cy_mqtt_subscribe`
  - `cy_mqtt_unsubscribe`

**Note:** Using this library in a project will cause the AWS IoT device SDK to be downloaded on your computer. It is your responsibility to understand and accept the AWS IoT Device SDK license.


## Supported platforms

- [PSoC&trade; 6 Wi-Fi Bluetooth&reg; prototyping kit  (CY8CPROTO-062-4343W)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062-4343w/ )

- [PSoC&trade; 62S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062S2-43012)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062s2-43012/ )

- [PSoC&trade; 6 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-062-WiFi-BT)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062-wifi-bt/ )

- [PSoC&trade; 64S0S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-064S0S2-4343W)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-064s0s2-4343w/ )

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-LAI-4373M2)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/ )

- [CYW954907AEVAL1F evaluation kit (CYW954907AEVAL1F)]( https://www.infineon.com/cms/en/product/evaluation-boards/cyw954907aeval1f/ )

- [CYW943907AEVAL1F Evaluation Kit (CYW943907AEVAL1F)]( https://www.infineon.com/cms/en/product/evaluation-boards/cyw943907aeval1f/ )

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-MUR-43439M2)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/ )

- [XMC7200D-E272K8384 kit (KIT-XMC72-EVK)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc72_evk/)

- [XMC7200D-E272K8384 kit (KIT_XMC72_EVK_MUR_43439M2)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc72_evk/)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-CYW43022CUB)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)

**Note:** Virtual APIs are supported on all PSoC 62 devices, but they have only been tested on the CY8CEVAL-062S2-MUR-43439M2 kit.

## Dependent libraries

This MQTT client library depends on the following libraries. These libraries are included by default.

- [AWS IoT device SDK port]( https://github.com/Infineon/aws-iot-device-sdk-port )

- [AWS IoT device SDK MQTT library]( https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/202103.00 )

If virtual APIs are to be used, it additionally depends on the [Virtual Connectivity Manager]( https://github.com/Infineon/virtual-connectivity-manager ) library.

## Quick start

1. To use mqtt library with Wi-Fi kits on FreeRTOS, lwIP, and Mbed TLS combination, the application should pull [mqtt]( https://github.com/Infineon/mqtt ) library and [wifi-core-freertos-lwip-mbedtls]( https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls ) library which will internally pull secure-sockets, wifi-connection-manager, FreeRTOS, lwIP, Mbed TLS and other dependent modules.
To pull wifi-core-freertos-lwip-mbedtls and mqtt libraries create the following *.mtb* files in deps folder.
   - *wifi-core-freertos-lwip-mbedtls.mtb:*
      `https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls#latest-v1.X#$$ASSET_REPO$$/wifi-core-freertos-lwip-mbedtls/latest-v1.X`
               
      **Note:** To use TLS version 1.3, please upgrade wifi-core-freertos-lwip-mbedtls to latest-v2.X (It is supported on all the platforms except [PSoC&trade; 64S0S2 Wi-Fi Bluetooth&reg; pioneer kit (CY8CKIT-064S0S2-4343W)](https://www.cypress.com/documentation/development-kitsboards/psoc-64-standard-secure-aws-wi-fi-bt-pioneer-kit-cy8ckit))
   - *mqtt.mtb:*
      `https://github.com/Infineon/mqtt#latest-v4.X#$$ASSET_REPO$$/mqtt/latest-v4.X`

2. To use mqtt library with Ethernet kits on FreeRTOS, lwIP, and Mbed TLS combination, the application should pull [mqtt]( https://github.com/Infineon/mqtt ) library and [ethernet-core-freertos-lwip-mbedtls]( https://github.com/Infineon/ethernet-core-freertos-lwip-mbedtls ) library which will internally pull secure-sockets, ethernet-connection-manager, FreeRTOS, lwIP, Mbed TLS and other dependent modules.
To pull ethernet-core-freertos-lwip-mbedtls and mqtt libraries create the following *.mtb* files in deps folder.
   - *ethernet-core-freertos-lwip-mbedtls.mtb:*
      `https://github.com/Infineon/ethernet-core-freertos-lwip-mbedtls#latest-v1.X#$$ASSET_REPO$$/ethernet-core-freertos-lwip-mbedtls/latest-v1.X`
     
     **Note:** To use TLS version 1.3, please upgrade ethernet-core-freertos-lwip-mbedtls to latest-v2.X
     
   - *mqtt.mtb:*
      `https://github.com/Infineon/mqtt#latest-v4.X#$$ASSET_REPO$$/mqtt/latest-v4.X`

3. Ensure that the application includes *./include/cy_mqtt_api.h* to use the library APIs and structure.

4. The referenced file *./include/core_mqtt_config.h* that is bundled with this library provides the default configurations required for the [AWS IoT device SDK](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/202103.00) library.

5. By default, [AWS IoT device SDK](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/202103.00) library logging is turned off. To enable log messages, map the logging-related macros `LogError`, `LogWarn`, `LogInfo`, and `LogDebug` in *./include/core_mqtt_config.h* to the application-specific logging implementation.

6. Define the following macros in the application Makefile with values to suit the use case and network conditions:

   Macro | Description | Example
   ------|-------------|-------
   `CY_MQTT_ACK_RECEIVE_TIMEOUT_MS` | A "reasonable amount of time" (timeout value) to wait for receiving the acknowledgment packet from the MQTT broker for MQTT publish/subscribe messages with QoS1/QoS2 | `DEFINES += CY_MQTT_ACK_RECEIVE_TIMEOUT_MS=3000`
   `CY_MQTT_MESSAGE_SEND_TIMEOUT_MS` | MQTT message send timeout | `DEFINES += CY_MQTT_MESSAGE_SEND_TIMEOUT_MS=3000`
   `CY_MQTT_MESSAGE_RECEIVE_TIMEOUT_MS` | MQTT message receive timeout | `DEFINES += CY_MQTT_MESSAGE_RECEIVE_TIMEOUT_MS=500`
   `CY_MQTT_MAX_RETRY_VALUE` | MQTT library retry mechanism for MQTT publish/subscribe/unsubscribe messages if the acknowledgement is not received from the broker on time. You can configure the maximum number of retries. | `DEFINES += CY_MQTT_MAX_RETRY_VALUE=3`
   `CY_MQTT_MAX_OUTGOING_PUBLISHES` | To perform multiple publish operations simultaneously on a single MQTT instance, configure the `CY_MQTT_MAX_OUTGOING_PUBLISHES` macro with the number of simultaneous publish operations to be performed. For the default value of this macro, see the MQTT library API header file. This macro can be configured by adding a define in the application Makefile. | `DEFINES += CY_MQTT_MAX_OUTGOING_PUBLISHES=2`
   `CY_MQTT_MAX_OUTGOING_SUBSCRIBES` | To perform multiple subscribe operations simultaneously on a single MQTT instance, configure hte `CY_MQTT_MAX_OUTGOING_SUBSCRIBES` macro with the number of simultaneous subscribe operations to be performed. For the default value of this macro, see the MQTT library API header file. This macro can be configured by adding a define in the application Makefile. | `DEFINES += CY_MQTT_MAX_OUTGOING_SUBSCRIBES=2`
   `MQTT_PINGRESP_TIMEOUT_MS` | A "reasonable amount of time" (timeout value) to wait for the keepalive response from the MQTT broker | `DEFINES += MQTT_PINGRESP_TIMEOUT_MS=5000`
   `MQTT_RECV_POLLING_TIMEOUT_MS` | A "maximum polling duration" that is allowed without any data reception from the network for the incoming packet | `DEFINES += MQTT_RECV_POLLING_TIMEOUT_MS=1000`
   `MQTT_SEND_RETRY_TIMEOUT_MS` | A "maximum duration" that is allowed for no data transmission over the network through the transport send function | `DEFINES += MQTT_SEND_RETRY_TIMEOUT_MS=500`
<br>

**Note:** It is important to note that having the `MQTT_RECV_POLLING_TIMEOUT_MS` timeout as too short will result in MQTT being disconnected due to the possibility of partial data being received. If you have small TCP buffers and a high-latency network, the optimum value for the timeout can be surprisingly long. In such cases, the optimum value for timeout can be better determined based on experimenting the MQTT applications with payloads bigger than the TCP buffer. See [AWS coreMQTT documentation](https://docs.aws.amazon.com/embedded-csdk/202103.00/lib-ref/libraries/standard/coreMQTT/docs/doxygen/output/html/mqtt_timeouts.html#mqtt_timeouts_receive_polling) for more details.<br>

7. Review and make the required changes to the pre-defined configuration files.

   The configuration files are bundled with the wifi-mw-core library for FreeRTOS, lwIP, and Mbed TLS. See [README.md]( https://github.com/Infineon/wifi-mw-core/blob/master/README.md ) for details.

   If the application uses the bundle library, the configuration files are in the bundle library. For example, if the application uses the **Wi-Fi core freertos lwip mbedtls bundle library**, the configuration files are in the *wifi-core-freertos-lwip-mbedtls/configs* folder. Similarly, if the application uses the **Ethernet Core FreeRTOS lwIP mbedtls library**, the configuration files are in the *ethernet-core-freertos-lwip-mbedtls/configs* folder.

8. Define the following COMPONENTS in the application's Makefile for the Azure port library.
    ```
    COMPONENTS=FREERTOS MBEDTLS LWIP SECURE_SOCKETS
    ```

9. The 'aws-iot-device-sdk-port' layer includes the 'coreHTTP' and 'coreMQTT' modules of the 'aws-iot-device-sdk-embedded-C' library by default. If the user application does not use HTTP client features, update the application Makefile to exclude the coreHTTP source files from the build. The Makefile entry should look like as follows:
    ```
    CY_IGNORE+= $(SEARCH_aws-iot-device-sdk-embedded-C)/libraries/standard/coreHTTP
    ```

10. For a secured platform like CY8CKIT-064S0S2-4343W, define the following macros in the application Makefile:

    Macro | Description | Example
    ------|-------------|-------
    `CY_TFM_PSA_SUPPORTED` <br> <br>`TFM_MULTI_CORE_NS_OS` | When the MQTT library is used on secured platforms such as CY8CKIT-064S0S2-4343W | `DEFINES+=CY_TFM_PSA_SUPPORTED TFM_MULTI_CORE_NS_OS`
    `CY_SECURE_SOCKETS_PKCS_SUPPORT` | Enables PKCS-PSA support on secured platforms such as CY8CKIT-064S0S2-4343W | `DEFINES+=CY_SECURE_SOCKETS_PKCS_SUPPORT`

    <br>

    **Note:** Secured platforms such as CY8CKIT-064S0S2-4343W use trusted-firmware-m library for crypto operations. Due to security vulnerability in HW acceleration, trusted-firmware-m version v1.3.3 uses software implementation for ECC crypto algorithm. Because the software implementation is slower, it is recommended to set `configEXPECTED_IDLE_TIME_BEFORE_SLEEP` to at least 100 in the application's *FreeRTOSConfig.h* file.

11. Add the trusted firmware library include path before the Mbed TLS library include path in the application Makefile. This is required to compile the MQTT library for secured platforms such as CY8CKIT-064S0S2-4343W. The Makefile entry should look like as follows:
    ```
    INCLUDES=$(SEARCH_trusted-firmware-m)/COMPONENT_TFM_NS_INTERFACE/include
    INCLUDES+=libs/trusted-firmware-m/COMPONENT_TFM_NS_INTERFACE/include
    ```

**Note:** This MQTT client library does not support secured connections to the public `test.mosquitto.org` broker by default because the server uses the SHA1 hashing algorithm. As cautioned by Mbed TLS, SHA-1 is considered a weak message digest and is therefore not enabled in Mbed TLS by default. The use of SHA-1 for certificate signing constitutes a security risk. It is recommended to avoid dependencies on it, and consider stronger message digests instead.

### Virtual API usage

1. To use MQTT virtual APIs, pull [Virtual Connectivity Manager]( https://github.com/Infineon/virtual-connectivity-manager ) library. Create the following *.mtb* file to pull the library

     *virtual-connectivity-manager.mtb:* `https://github.com/Infineon/virtual-connectivity-manager#latest-v1.X#$$ASSET_REPO$$/virtual-connectivity-manager/latest-v1.X`

**Note:** To use MQTT APIs in a multi-core environment, applications on both the cores should include MQTT and VCM libraries.

2. Define the following compile-time macro in the primary core application's Makefile:
   ```
   DEFINES+=ENABLE_MULTICORE_CONN_MW VCM_ENABLE_MQTT
   ```
3. Define the following compile-time macros in the secondary core application's Makefile:
   ```
   DEFINES+=ENABLE_MULTICORE_CONN_MW USE_VIRTUAL_API VCM_ENABLE_MQTT
   ```
4. Call the `cy_vcm_init()` function provided by the VCM library from the application on both cores, before invoking the virtual MQTT APIs.

   See [Virtual Connectivity Manager library API documentation]( https://infineon.github.io/virtual-connectivity-manager/api_reference_manual/html/index.html ).

**Notes:**

- To ensure that the VCM initialization is synchronized, the project which boots first(i.e CM0+ project in case of psoc62) must call `cy_vcm_init` before bringing up the second project(i.e CM4 project in case of psoc62).

- The first project must initialize VCM, by passing `config.hal_resource_opt` as `CY_VCM_CREATE_HAL_RESOURCE` in `cy_vcm_init`. The second project must pass `config.hal_resource_opt` as `CY_VCM_USE_HAL_RESOURCE`.

### Log messages

The MQTT library disables all debug log messages by default. Do the following to enable log messages:

1. Add the `ENABLE_MQTT_LOGS` macro to the *DEFINES* in the code example's Makefile. The Makefile entry should look like as follows:
    ```
    DEFINES+=ENABLE_MQTT_LOGS
    ```

2. Call the `cy_log_init()` function provided by the *cy-log* module. cy-log is part of the *connectivity-utilities* library. See [connectivity-utilities library API documentation]( https://infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html ) for cy-log details.

   - To enable logs in a dual core application please refer to [Enable logs in dual core application]( https://github.com/Infineon/virtual-connectivity-manager#enable-logs-in-dual-core-application ) section in Virtual Connectivity Manager.

## Stack size

The default stack size of the mqtt event processing thread is 3 KB (3*1024). To customize the stack size, add  `CY_MQTT_EVENT_THREAD_STACK_SIZE` macro to the `DEFINES` in the application Makefile with the required stack size. The Makefile entry for 8KB stack would look like as follows:

  ```
  DEFINES+=CY_MQTT_EVENT_THREAD_STACK_SIZE=8*1024
  ```


## Usage notes

- The `cy_mqtt_init()` and `cy_mqtt_deinit()` functions are not thread-safe.

- The application must allocate a network buffer; the same buffer must be passed while creating the MQTT object. The MQTT library uses this buffer for sending and receiving MQTT packets. The `CY_MQTT_MIN_NETWORK_BUFFER_SIZE` macro defined in *./include/cy_mqtt_api.h* specifies the minimum size of the network buffer required for the library to handle MQTT packets.

- The application must call `cy_mqtt_init()` before calling any other MQTT library function, and must not call other MQTT library functions after calling `cy_mqtt_deinit()`.

- An MQTT instance is created using `cy_mqtt_create`. The MQTT library allocates resources for each created instance; therefore, the application must delete the MQTT instance by calling `cy_mqtt_delete()` after executing the desired MQTT operations.

- After calling `cy_mqtt_delete`, all the resource associated with the MQTT handle are freed. Therefore, do not use the same MQTT handle after delete.

- If X509 certificate-based authentication is used, the password must be NULL in the MQTT connect request for the Azure MQTT broker.

- If SAS token-based authentication is used, values of RootCA, private key, and device certificate must be NULL in the MQTT connect request for the Azure MQTT broker.

- If SAS token-based authentication is used on a secured platform like CY8CKIT-064S0S2-4343W, you must set RootCA, device certificate, and key location as RAM to avoid reading them from a secured element during a TLS connection.

- The MQTT disconnection event notification is not sent to the application when disconnection is initiated from the application.

- When the application receives an MQTT disconnection event notification from the library, the application must call `cy_mqtt_disconnect()` to release the resource allocated during `cy_mqtt_connect()`. The following table summarizes the reasons for MQTT disconnection event from library:

   MQTT Event type | Reason | Description
   ----------------|--------|------------
   `CY_MQTT_EVENT_TYPE_DISCONNECT` | `CY_MQTT_DISCONN_TYPE_NETWORK_DOWN` | Network is disconnected.
   `CY_MQTT_EVENT_TYPE_DISCONNECT` | `CY_MQTT_DISCONN_TYPE_BROKER_DOWN` | Ping response is not received from the broker for the ping request sent; possibly the broker is down.
   `CY_MQTT_EVENT_TYPE_DISCONNECT` | `CY_MQTT_DISCONN_TYPE_BAD_RESPONSE` | A bad response received form the MQTT broker. <br>Possibly received a MQTT packet with invalid packet type ID.
   `CY_MQTT_EVENT_TYPE_DISCONNECT` | `CY_MQTT_DISCONN_TYPE_SND_RCV_FAIL` | MQTT packet send/receive failed because of high network latency and smaller values set for `MQTT_RECV_POLLING_TIMEOUT_MS` and/or `MQTT_SEND_RETRY_TIMEOUT_MS` macros.

<br>


### Dependencies

For secured platforms such as CY8CKIT-064S0S2-4343W, the library depends on other libraries for PKCS-PSA support. Do the following to add dependent libraries:

1. If using .lib files, create the [freertos-pkcs11-psa.lib]( https://github.com/Linaro/freertos-pkcs11-psa/#80292d24f4978891b0fd35feeb5f1d6f6f0fff06 ) file for freertos-pkcs11-psa and add the .lib file along with other dependent library .lib files.

2. If using .mtb files, create the [freertos-pkcs11-psa.mtb]( https://github.com/Linaro/freertos-pkcs11-psa/#80292d24f4978891b0fd35feeb5f1d6f6f0fff06#$$ASSET_REPO$$/freertos-pkcs11-psa/master ) file for freertos-pkcs11-psa and add the .mtb file along with other dependent library *.mtb* files.

3. Execute the `make getlibs` command.


### MQTT connection using provisioned certificates and keys

Do the following to establish a connection with the broker using credentials stored in the secured memory for secured platforms such as CY8CKIT-064S0S2-4343W:

1. Provision the kit with the device and RootCA certificates. See [Device provisioning steps]( https://community.infineon.com/t5/Resource-Library/Provisioning-Guide-for-the-Cypress-CY8CKIT-064S0S2-4343W-Kit/ta-p/252469 ).

2. User can update device certificate and RootCA certificate by modifying respective the policy *.json* file with the device and RootCA certificate paths to be provisioned to the secured element as follows:

    ```
    "provisioning:"
    {
        "chain_of_trust": ["../certificates/device_cert.pem", "../certificates/rootCA.pem"]
    },
    ```

3. Add the `CY_TFM_PSA_SUPPORTED`, `TFM_MULTI_CORE_NS_OS`, and `CY_SECURE_SOCKETS_PKCS_SUPPORT` macros in the application Makefile. The Makefile entry should look like as follows:
    ```
    DEFINES+=CY_TFM_PSA_SUPPORTED TFM_MULTI_CORE_NS_OS CY_SECURE_SOCKETS_PKCS_SUPPORT
    ```
    **Note:** Do not pass any credentials/certificate from the application. While establishing the connection with the MQTT broker, certificates and keys stored in the secured memory are used.


## Additional information

- [MQTT client library RELEASE.md](./RELEASE.md)

- [MQTT client API documentation]( https://infineon.github.io/mqtt/api_reference_manual/html/index.html )

- [Porting guide for MQTT v4.0.0]( https://github.com/Infineon/mqtt/blob/master/porting_guide.md )

- [ModusToolbox&trade; software environment, quick start guide, documentation, and videos]( https://www.infineon.com/modustoolbox )

- [AWS-IoT device SDK library]( https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/202103.00 )

- [ModusToolbox&trade; code examples]( https://github.com/Infineon?q=mtb-example-anycloud%20NOT%20Deprecated )
