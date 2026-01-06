# Wi-Fi Connection Manager (WCM)

WCM is a library which helps application developers to manage Wi-Fi connectivity. The library provides a set of APIs that can be used to establish and monitor Wi-Fi connections on Infineon platforms that support Wi-Fi connectivity.

The library APIs are thread-safe. The library monitors the Wi-Fi connection and can notifies connection state changes through an event notification mechanism. The library also provides APIs to connect to a Wi-Fi network using Wi-Fi Protected Setup (WPS) methods.

## Multi-core architecture and virtual API support

The library supports multi-core architecture by making a subset of APIs available as virtual APIs. The virtualization of the WCM library helps to access the WCM APIs defined in one core from the other core using Inter Process Communication (IPC). The WCM can now be run on two cores simultaneously, with one core containing the full connectivity stack (primary core) and the other core containing the subset of virtual-only APIs (secondary core).

The virtual APIs pipe the API requests over IPC to the primary core where the API is actually executed and the result is passed back to the secondary core. This virtualization abstracts out the implementation details and complexity of IPC, thus making multi-core connectivity application development simple.

For more information on virtualization, see the [Virtual Connectivity Manager]( https://github.com/Infineon/virtual-connectivity-manager ) library.


## Features and functionality

The current implementation has the following features and functionality:

- Supports WPA3 personal network security
- Supports STA, SoftAP, and Concurrent (simultaneous Soft-AP + STA) modes
- Support for WPS - Enrollee role
- Exposes Wi-Fi APIs to scan, join, and leave the Wi-Fi network
- Connection monitoring: Monitors active connections and link events. Automatically reconnects to the AP if the connection with the AP is lost intermittently. Notifies the connection state change through the event notification registration mechanism.
- Supports connectivity applications based on either FreeRTOS, lwIP, mbed TLS combination or ThreadX, NetX Duo, NetX Secure combination(Currently only supported on CYW955913EVK-01)
- Built on top of the abstraction-rtos library that provides the RTOS abstraction API for FreeRTOS and ThreadX
- Supports multi-core architecture by providing the following APIs as virtual APIs:
  - `cy_wcm_is_connected_to_ap`
  - `cy_wcm_register_event_callback`
  - `cy_wcm_deregister_event_callback`


## Supported platforms

This library and its features are supported on the following Infineon platforms:

- [PSoC&trade; 6 Wi-Fi BT prototyping kit (CY8CPROTO-062-4343W)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062-4343w/ )

- [PSoC&trade; 62S2 Wi-Fi BT pioneer kit (CY8CKIT-062S2-43012)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062s2-43012/ )

- [PSoC&trade; 6 WiFi-BT pioneer kit (CY8CKIT-062-WiFi-BT)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ckit-062-wifi-bt/ )

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-LAI-4373M2)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/ )

- [CYW954907AEVAL1F evaluation kit (CYW954907AEVAL1F)]( https://www.infineon.com/cms/en/product/evaluation-boards/cyw954907aeval1f/ )

- [CYW943907AEVAL1F evaluation kit (CYW943907AEVAL1F)]( https://www.infineon.com/cms/en/product/evaluation-boards/cyw943907aeval1f/ )

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-MUR-43439M2)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/ )

- [XMC7200D-E272K8384 kit (KIT_XMC72_EVK_MUR_43439M2)](https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc72_evk/)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-CYW43022CUB)](https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/)

- [CYW955913EVK-01 Wi-Fi Bluetooth&reg; Prototyping Kit (CYW955913EVK-01)](https://www.infineon.com/CYW955913EVK-01)

- [PSoC&trade; 62S2 evaluation kit (CY8CEVAL-062S2-CYW955513SDM2WLIPA)]( https://www.infineon.com/cms/en/product/evaluation-boards/cy8ceval-062s2/ )

**Note**: Virtual APIs are supported on all PSoC 62 devices, but they have only been tested on the CY8CEVAL-062S2-MUR-43439M2 kit.

## Dependent libraries

This library depends on the following:

- [wifi-core-freertos-lwip-mbedtls]( https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls )

- [wifi-core-threadx-cat5]( https://github.com/Infineon/wifi-core-threadx-cat5 ) - For CYW955913EVK-01

- [Wi-Fi Host Driver]( https://github.com/Infineon/wifi-host-driver )

If virtual APIs are to be used, it additionally depends on the [Virtual Connectivity Manager]( https://github.com/Infineon/virtual-connectivity-manager ).

## Quick start

1. To use wifi-connection-manager library for FreeRTOS, lwIP, mbed TLS combination, the application should pull the [wifi-core-freertos-lwip-mbedtls]( https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls ) library which will internally pull wifi-connection-manager, FreeRTOS, lwIP, mbed TLS, and other dependent modules.
   
   To pull [wifi-core-freertos-lwip-mbedtls]( https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls#latest-v1.X#$$ASSET_REPO$$/wifi-core-freertos-lwip-mbedtls/latest-v1.X ), create the *wifi-core-freertos-lwip-mbedtls.mtb* file.

2. To use wifi-connection-manager library for ThreadX, NetX Duo, NetX Secure combination, the application should pull the [wifi-core-threadx-cat5]( https://github.com/Infineon/wifi-core-threadx-cat5 ) library which will internally pull wifi-connection-manager and other dependent modules.

   To pull [wifi-core-threadx-cat5]( https://github.com/Infineon/wifi-core-threadx-cat5#latest-v1.X#$$ASSET_REPO$$/wifi-core-threadx-cat5/latest-v1.X ), create the *wifi-core-threadx-cat5.mtb* file.

3. For existing Wi-Fi Connection Manager version 2.x users, a [porting guide]( https://github.com/Infineon/wifi-connection-manager/blob/master/porting_guide.md ) is available to migrate to Wi-Fi Connection Manager version 3.0.

4. Review the pre-defined configuration files bundled with the wifi-core-freertos-lwip-mbedtls library for FreeRTOS, lwIP, and mbed TLS and make adjustments. See the "Quick start" section in [README.md]( https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls/blob/master/README.md ).

5. Define a set of COMPONENTS in the code example project's Makefile for this library.
   - For FreeRTOS, lwIP, Mbed TLS combination see the "Quick start" section in [README.md]( https://github.com/Infineon/wifi-core-freertos-lwip-mbedtls/blob/master/README.md )
   - For ThreadX, NetX Duo, NetX Secure combination see the "Quick start" section in [README.md]( https://github.com/Infineon/wifi-core-threadx-cat5/blob/master/README.md )

6. WPS is disabled by default. WPS uses Mbed TLS security stack. Enable the following components for WPS:
   ```
   COMPONENTS+=WPS MBEDTLS
   ```
7. The WCM library disables all the debug log messages by default. To enable log messages, the application must perform the following:

   1. Add the `ENABLE_WCM_LOGS` macro to the `DEFINES` in the code example's Makefile. The Makefile entry would look like as follows:
      ```
      DEFINES+=ENABLE_WCM_LOGS
      ```
   2. Call the `cy_log_init()` function provided by the *cy-log* module. cy-log is part of the *connectivity-utilities* library.
      See [connectivity-utilities library API documentation]( https://Infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html ).

      - To enable logs in a dual core application please refer to [Enable logs in dual core application]( https://github.com/Infineon/virtual-connectivity-manager/blob/main/README.md#enable-logs-in-dual-core-application ) section in Virtual Connectivity Manager.

### Virtual API usage

* To use virtual WCM APIs pull [Virtual Connectivity Manager]( https://github.com/Infineon/virtual-connectivity-manager ) library.
  Create the following *.mtb* file to pull the library
  - *virtual-connectivity-manager.mtb:* `https://github.com/Infineon/virtual-connectivity-manager#latest-v1.X#$$ASSET_REPO$$/virtual-connectivity-manager/latest-v1.X`

**Note:** To use WCM APIs in a multi-core environment, applications on both the cores should include WCM and VCM libraries.

* Define the following compile-time macro in the primary core application's Makefile:
   ```
   DEFINES+=ENABLE_MULTICORE_CONN_MW
   ```
* Define the following compile-time macros in the secondary core application's Makefile:
   ```
   DEFINES+=ENABLE_MULTICORE_CONN_MW USE_VIRTUAL_API
   ```
* Call the `cy_vcm_init()` function provided by the VCM library from the application on both cores, before invoking the virtual WCM APIs.

   See [Virtual Connectivity Manager library API documentation]( https://infineon.github.io/virtual-connectivity-manager/api_reference_manual/html/index.html ).

**Notes:**
  - To ensure that the VCM initialization is synchronized, the project which boots first(i.e CM0+ project in case of psoc62) must call `cy_vcm_init` before bringing up the second project(i.e CM4 project in case of psoc62).
  - The first project must initialize VCM, by passing `config.hal_resource_opt` as `CY_VCM_CREATE_HAL_RESOURCE` in `cy_vcm_init`. The second project must pass `config.hal_resource_opt` as `CY_VCM_USE_HAL_RESOURCE`.

## Additional information

- [Wi-Fi Connection Manager RELEASE.md]( ./RELEASE.md )

- [Wi-Fi Connection Manager API documentation]( https://Infineon.github.io/wifi-connection-manager/api_reference_manual/html/index.html )

- [Porting guide for Wi-Fi Connection Manager version 3.0]( https://github.com/Infineon/wifi-connection-manager/blob/master/porting_guide.md )

- [Connectivity Utilities API documentation - for cy-log details]( https://Infineon.github.io/connectivity-utilities/api_reference_manual/html/group__logging__utils.html )

- [ModusToolbox&trade; software environment, quick start guide, documentation, and videos]( https://www.infineon.com/modustoolbox )

- [Wi-Fi Connection Manager version]( ./version.xml )

- [ModusToolbox&trade; code examples]( https://github.com/Infineon/Code-Examples-for-ModusToolbox-Software )
