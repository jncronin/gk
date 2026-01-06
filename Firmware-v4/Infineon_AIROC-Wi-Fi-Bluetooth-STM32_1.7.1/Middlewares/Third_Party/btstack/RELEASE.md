# Bluetooth Stack Library

## Known Issues/Limitations
Following are the limitations when using host based address resolution (only applies to the 4343 controller chip):
    Since whitelists are implemented purely in the controller, advertisement whitelist filtering will not work if a peer is using Resolvable Private Addresses (since the address cannot be resolved in the controller). To be safe, applications should not use the whitelist at all, but at a minimum not use it if paired with a private peer device.
    There is a very small window when the host stack gets an ADV, resolves the address and tries to connect to the private address, but during that time the peripheral has changed the address. This might very occasionally cause a connection attempt to fail.
    If the device is acting as a central it should not enable privacy since if a peripheral sends a directed connectable ADV, the controller would not be able to match the RPA and the connection will fail.

## Changelog

## V3.7.1
BTSTACK3.7.1 is a patch release with following enhancements -
 - Add v2 support for HCI_LE_Set_Extended_Advertising_Parameters present in BT Core Spec 5.4

## V3.7.0
BTSTACK3.7 contains following enhancements and fixes -
 - Enhancements for lowering stack library code size. Only the part of the stack code that is used by application is included in the linked image.
   - Applications which do not create GATT/ACL connections or those which do not need SMP may override the default initializations done in the stack by defining the macro **DISABLE_DEFAULT_BTSTACK_INIT** in the applicatin Makefile.
       #Set DISABLE_DEFAULT_BTSTACK_INIT=1
       DEFINES+=DISABLE_DEFAULT_BTSTACK_INIT=1
   - GATT Server applications which need to implement GATT Robust Caching will need to invoke **wiced_bt_gatt_server_enable_caching** in the **BTM_ENABLED_EVT**
   - GATT applications work with signed data will need to invoke **wiced_bt_gatt_enable_signing** in the **BTM_ENABLED_EVT**
 - New APIs added (See API documentation for details)
   - wiced_bt_gatt_server_enable_caching
   - wiced_bt_gatt_server_enable_signing
 - Fix to Correct macro names used for S=2 and S=8 coding
 - Fix for A2DP/SNK/AVP/BI-20-C, A2DP/SNK/AVP/BI-10-C failures
 - Fix issue setting default Tx power with CY BLESS controller.
 - Updated document for following APIs -
   - wiced_bt_l2cap_enable_update_ble_conn_params
   - wiced_bt_avdt_write_req
   - wiced_bt_l2cap_update_ble_conn_params
 - L2CAP/ECFC/BI-08-C is under discussion/debug with PTS (see TSE#17703).

## V3.6.1
BTSTACK3.6.1 is a patch release with the following fixes -
 - Fix to check for ISOC support in controller before issuing read_buf_size_v2 HCI command.
 - Fix to mask Enhanced Connection Complete Event v2 HCI event in the HCI event mask as this is not handled in stack.

## V3.6.0
BTSTACK3.6 is BT5.3 certified. QDID: [196979](https://launchstudio.bluetooth.com/ListingDetails/164219).
Below test cases fail and are under discussion/debug with PTS.
 - L2CAP/COS/CED/BI-02-C, L2CAP/ECFC/BI-08-C, refer to TSE#17703
 - L2CAP/ECFC/BI-09-C, refer to TSE#18208

This release contains bug fixes and and an enhancement listed below.
 - Support HCI_LE_Read_buffer_size [v2].
 - Fix to return correct BD Address in the encryption status event callback.
 - Documentation update for wiced_bt_rfcomm_read_data().
 - The known issue from BTSTACK3.5 related to the handling of directed advertisement flags in the LE Set Extended Adv commands, is handled by changes in the application to set the correct address type to start directed adv.

## V3.5.0
BTSTACK3.5 release contains bug fixes and and an enhancement listed below.
 - Updated API description for wiced_bt_gatt_client_send_read_by_type().
 - Fix to issue where setting adv parameters was allowing incorrect adv type to be set for directed advertisements.
 - Fix for an issue in wiced_bt_delete_heap().
 - Fixed an issue to return correct connection handle when there is an ISOC CIS disconnection and added reason field.
 - Fix to avoid clearing of CIS data path info from stack during CIS disconnection.
 - Added sections in the code for aiding placement of btstack data path in the linker file.
 - Creation of stack libraries using ARM and IAR toolchains.

### Known issues in V3.5.0
 - The issue is related to the handling of directed advertisement flags in the LE Set Extended Adv commands. Android expects the device to send directed advertisement packets with its
   Random Private Address, whereas the device issues the same with the remotes public address.
   The workaround for the issue is to use undirected advertisements for applications to continue with their development when using a controller that supports Extended Advertisements.

## V3.4.0
BTSTACK3.4 release contains bug fixes and and an enhancement listed below.
 - Stack now conforms to PTS8.1.1. Below are the test cases that fail and are under discussion/debug with PTS,
   - TSE#12608 --> GATT/CL/GPM/BV-12-C
   - TSE#17703 --> L2CAP/COS/CED/BI-02-C, L2CAP/ECFC/BI-08-C
   - TSE#17581 --> L2CAP/ECFC/BV-38-C
   - TSE#18208 --> L2CAP/ECFC/BI-09-C
 - Added data type and macros for HCI roles. See wiced_bt_dev.h
 - Added new event BTM_BLE_DATA_LENGTH_UPDATE_EVENT to notify change in the data length and timeout configured for Rx and Tx on the BLE link. See wiced_bt_dev.h.
 - Added new API wiced_bt_dev_sec_pair_without_bonding() for pairing but without bonding. See wiced_bt_dev.h.
 - Added new APIs wiced_bt_dev_is_address_resolution_enabled and wiced_bt_dev_is_privacy_supported to query the support status for controller based address resolution and device privacy mode, respectively. See wiced_bt_dev_utils.h.
 - GATT status code updated with error code for improper configuration of CCCD (WICED_BT_GATT_CCCD_IMPROPER_CONFIGURED). See wiced_bt_gatt.h.
 - Added APIs to get heap and pool statistics in wiced_memory.h.


## V3.3.0
BTSTACK3.3 release contains bug fixes and and an enhancement listed below.
 - Fix for GAP conformance test case GAP/SEC/SEM/BV-14-C.
 - Fix for an issue where Maximum possible multi-adv connections value was wrong and adv instance creation was failing.
 - Update the documentation for wiced_bt_ble_security_grant() API.
 - Removed unused platform API wiced_bt_stack_platform_deinit().
 - Added memory management APIs to allocate free long term memory blocks, wiced_memory_alloc_long_term_mem_block() and wiced_memory_free_long_term_mem_block() in wiced_memory.h


## V3.2.0
BTSTACK3.2 contains below updates to the Bluetooth host stack:
 - New APIs added
   - wiced_bt_l2cap_deregister_fixed_channel() API to deregister a fixed L2CAP channel (see also wiced_bt_l2cap_register_fixed_channel().
   - wiced_bt_dev_update_debug_trace_mode() to enable/disable debug traces.
   - APIs for ECRB (Enhanced Credit Based L2CAP) channels management.
   - wiced_ble_private_device_address_resolution() that checks if a private address is resolvable with the given IRK.
   - wiced_bt_dev_get_acl_conn_handle() to get connection handle given a peer BD Address.
 - Bug fixes and other updates
   - The stack internally sets the default BLE data length to 251 at start-up, i.e., enables BLE Data Length Extension. Also, pairing is disabled by default so that application can enable using wiced_bt_set_pairable_mode(),
   - Fix for crash during LE-COC data transfer.
   - Fix for issue where extended advertisement was enabled before random address was set.
   - Fix to update just_works field correctly in BTM_USER_CONFIRMATION_REQUEST_EVT.

### Known issues in V3.2.0
 - Documentation bug - in the documentation the API wiced_bt_ble_security_grant(), possible values for argument 'res' is mentioned as BTM_SUCCESS, BTM_MODE_UNSUPPORTED and BTM_REPEATED_ATTEMPTS. Please read them as WICED_BT_SUCCESS, WICED_BT_UNSUPPORTED and WICED_BT_REPEATED_ATTEMPTS, respectively.

## V3.1.0
BTSTACK3.1 is BT5.2 certified. Certification includes EATT and ISOC features, QDID: [172247](https://launchstudio.bluetooth.com/ListingDetails/134246).
 - APIs added in this release:
   - API to get number of GATT packets in the tx queue: int wiced_bt_gatt_get_num_queued_tx_packets()
   - API to get num packets in L2C queue: wiced_bt_l2cap_get_num_queued_tx_packets()
   - API for checkecking whether a connection id is still in operation/connected: wiced_bt_gatt_status_t wiced_bt_gatt_validate_conn_id()
 - Minor documentation updates are done including the issue in wiced_bt_gatt.h using the term clcb_operation instead of operation that was present in BTSTACK3.0.
 - Bug fixes
   - In GATT Disconnection event connection id always set to 0x8000.
   - Fix crash in API wiced_bt_remove_from_queue when dequeuing from an empty queue.
   - Fix for incorrect discovery_type value in GATT_DISCOVERY_CPLT_EVT.
   - Fix to allow back to back operations on event complete in gatt read, read multiple, read_by_type_rsp.

## V3.0.0
This release contains major updates BTSTACK interfaces requiring modifications in existing applications in order for them to work with BTSTACK3.0. Please see the **[Migration Guide](https://infineon.github.io/btstack/BTSTACK_2.0_to_3.0_API_Migration_Guide.htm)** for more details on how to migrate your application written for BTSTACK1.X or BTSTACK2.0 to BTSTACK3.0.
 - Stack configuration parameters (wiced_bt_cfg_settings_t) are updated for better readability and to remove parameters that are not necessary, providing APIs where requred. Stack APIs such as GATT profile APIs have been modified for ease of use.
 - BT SIG has updated the naming of a number of entities (e.g., roles of bluetooth devices), the stack interfaces and internal code has been updated accordingly.
 - Stack Config Structure - Changes to stack config settings structure has been done to keep the config elements to a minimum and to be able to seperate BLE configuration elements from BR/EDR configuration elements for ease of use and readability. Parameters that can't be modified using config structure are allowed to be modified using APIs where required.
As part of this change, the wiced_bt_cfg_settings_t and associated structures are changed.
 - GATT APIs - GATT APIs have been updated to simplify for ease of use of the APIs.
   - Updated APIs let application determine and allocate the memory required for GATT data transfers.
   - Provides a single interface to respond to a request by eliminating multiple paths that was present earlier (by API call, implicit response by return in callback).
   - On the GATT server, applications are expected to invoke specific response APIs or the error response to respond to incoming GATT requests
 - Better Congestion handling
 - Improved overall memory usage for GATT applications

### Known issues in V3.0.0
- In GATT Disconnection event connection id always set to 0x8000.
  Workaround to identify the terminated connection is to check the BD ADDR in the event data.
- Documentation - in wiced_bt_gatt.h, the term clcb_operation should be read as 'operation' in the following structure definition
  - wiced_bt_gatt_discovery_complete_t, in documentation of member status.
  - wiced_bt_gatt_operation_complete_t, in documentation of member op.

## V2.0.0
 - The BT Host Stack is BT5.2 certified, QDID: 160340.
 - Extended Advertisement feature is supported.
 - EATT is supported.
 - Documentation improvements.
 - Documentation fixes.

## V1.5.0
This release is an update over BTSTACK1.4 release.
 - New API in wiced_bt_stack_Platform.h, wiced_bt_stack_indicate_lower_tx_complete() has been added. This is called by the lower layer transport driver to restart sending ACL data to the controller when buffers are agian available with the lower layer transport driver after having run out.
 - The stack now supports Host based address resolution when using controllers that do not support address resolution.
 - API wiced_bt_stack_shutdown in wiced_bt_stack_platform.h, to be called by porting layer to clean-up stack context data when wiced_bt_stack_deinit() is called by application.


## V1.4.0
BTSTACK v1.40 has bug fixes and enhancements.
 - New interface in wiced_bt_stack_platform.h, which defines porting layer facing interfaces, to disable stack traces and identify trace type (DEBUG, ERROR, etc.,).
 - New API to get expected dynamic memory required in stack based on a given wiced_bt_cfg_settings_t is added (wiced_bt_stack_get_dynamic_memory_size_for_config).
- GATT Caching:
  - Perform OTFK (On-The-Fly_Key hash generation for the database. This reduces the intermediate memory requirements for calculating the hash)
  - Support for split database added
- wiced_bt_delete_heap API to delete application created heaps added

## V1.3.0
- Initial public release of Bluetooth Host stack
