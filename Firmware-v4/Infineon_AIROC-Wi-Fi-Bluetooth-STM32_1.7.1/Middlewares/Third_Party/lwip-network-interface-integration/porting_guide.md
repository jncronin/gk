# Porting Guide for lwIP network interface integration

To support multiple *RTOS/network-stack/security-stack* combinations, *Wi-Fi middleware core* is deprecated and split into multiple libraries, because of this *Wi-Fi middleware core* equivalent APIs are now part of *lwIP network interface integration* library.

### API changes:

Below is the API mapping between *lwIP network interface integration* and *Wi-Fi middleware core*. For more details on API usage please refer to [API documentation](https://github.com/Infineon/lwip-network-interface-integration/api_reference_manual/html/index.html).

| New *lwIP network interface integration* APIs | Old *Wi-Fi middleware core* APIs |
| ----- | ----- |
| cy_network_init and cy_network_deinit | init and deinit APIs not available in *Wi-Fi middleware core* |
| cy_network_add_nw_interface | cy_lwip_add_interface |
| cy_network_remove_nw_interface | cy_lwip_remove_interface |
| cy_network_get_nw_interface | cy_lwip_get_interface |
| cy_network_ip_up | cy_lwip_network_up |
| cy_lwip_network_up | cy_lwip_network_down |
| cy_network_dhcp_renew | cy_lwip_dhcp_renew |
| (*cy_network_ip_change_callback_t) | (*cy_lwip_ip_change_callback_t) |
| (*cy_network_activity_event_callback_t) | (*cy_network_activity_event_callback_t) |
| cy_network_register_ip_change_cb | cy_lwip_register_ip_change_cb |

### Newly added APIs to lwIP network interface integration

Below are the list of APIs added to *lwIP network interface integration*. For more details on API usage please refer to [API documentation](https://github.com/Infineon/lwip-network-interface-integration/api_reference_manual/html/index.html).

 - cy_network_get_ip_address
 - cy_network_get_ipv6_address
 - cy_network_get_gateway_ip_address
 - cy_network_get_gateway_mac_address
 - cy_network_get_netmask_address
 - cy_network_ping
