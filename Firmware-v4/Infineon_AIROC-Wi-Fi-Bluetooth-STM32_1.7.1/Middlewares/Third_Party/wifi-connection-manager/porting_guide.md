# Porting Guide for Wi-Fi Connection Manager version 3.0

This guide details the steps required to migrate from Wi-Fi Connection Manager version 2.x to Wi-Fi Connection Manager version 3.0. Unlike normal releases, Wi-Fi Connection Manager 3.0 breaks compatibility with previous versions, so users might need to change their own code in order to make it work with Wi-Fi Connection Manager 3.0.

## Migration steps:

 - Delete *wifi-connection-manager.mtb* from the *deps* folder.

 - To use Wi-Fi Connection Manager for *freertos/lwip/mbedtls* combination, create *wifi-core-freertos-lwip-mbedtls.mtb* file with the following content.

   ```
   mtb://wifi-core-freertos-lwip-mbedtls#latest-v1.X#$$ASSET_REPO$$/wifi-core-freertos-lwip-mbedtls/latest-v1.X
   ```

   **Note:** Wi-Fi connection-manager version 3.0 is network stack agnostic but currently only freertos-lwip-mbedtls combination is supported on Infineon platforms. In future when more *RTOS/network-stack/security-stack* combinations are supported, appropriate *.mtb* file needs to be created similar to *wifi-core-freertos-lwip-mbedtls.mtb*.

### API changes

#### Wi-Fi Connection Manager version 3.0:

Following API syntax has been changed. For more details on API usage please refer to [API documentation](https://infineon.github.io/wifi-connection-manager/api_reference_manual/html/index.html)

| *Wi-Fi Connection Manager 2.X* API signature | *Wi-Fi Connection Manager 3.0* API signature |
| ------- | ---------- |
| cy_wcm_get_ip_addr(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *ip_addr, **uint8_t addr_count**) | cy_wcm_get_ip_addr(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *ip_addr) |
| cy_wcm_get_ipv6_addr(cy_wcm_interface_t interface_type, cy_wcm_ipv6_type_t ipv6_addr_type, cy_wcm_ip_address_t *ip_addr, **uint8_t addr_count**) | cy_wcm_get_ipv6_addr(cy_wcm_interface_t interface_type, cy_wcm_ipv6_type_t ipv6_addr_type, cy_wcm_ip_address_t *ip_addr) |
| cy_wcm_get_gateway_ip_address(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *gateway_addr, **uint8_t addr_count**) | cy_wcm_get_gateway_ip_address(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *gateway_addr) |
| cy_wcm_get_ip_netmask(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *net_mask_addr, **uint8_t addr_count**) | cy_wcm_get_ip_netmask(cy_wcm_interface_t interface_type, cy_wcm_ip_address_t *net_mask_addr) |
| cy_wcm_get_mac_addr(cy_wcm_interface_t interface_type, cy_wcm_mac_t *mac_addr, **uint8_t addr_count**) | cy_wcm_get_mac_addr(cy_wcm_interface_t interface_type, cy_wcm_mac_t *mac_addr) |

#### Wi-Fi middleware core:

 - *Wi-Fi middleware core* library used by Wi-Fi Connection Manager 2.X is deprecated. And Wi-Fi Connection Manager 3.0, uses *lwIP network interface integration* library instead of *Wi-Fi middleware core* library.
 - Applications using the *Wi-Fi middleware core* APIs should migrate to use the new *lwIP network interface integration* APIs. For more details on porting, please refer to the [porting guide](https://github.com/Infineon/lwip-network-interface-integration/porting_guide.md).
