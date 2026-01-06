# Porting Guide for MQTT

To support virtual APIs in a multi-core environment, some of the existing MQTT library API signature(s) have been updated.

### API changes:

*cy_mqtt_create* API function is updated to no longer take the event callback and user data as argument. The event callback needs to be registered by calling the new *cy_mqtt_register_event_callback* API.
Additionally, *cy_mqtt_create* now takes a descriptor string to uniquely identify each newly created MQTT handle.

For more details on API usage please refer to [API documentation](https://github.com/Infineon/mqtt/api_reference_manual/html/index.html).

Below is the API mapping between the older and newer APIs.

| New *MQTT* API signature | Old *MQTT* API signature |
| ----- | ----- |
| cy_rslt_t cy_mqtt_create( uint8_t *buffer, uint32_t buff_len,<br>                            cy_awsport_ssl_credentials_t *security,<br>                            cy_mqtt_broker_info_t *broker_info,<br>                            __char__ __\*descriptor__,<br>                            cy_mqtt_t *mqtt_handle ); | cy_rslt_t cy_mqtt_create( uint8_t *buffer, uint32_t buff_len,<br>                            cy_awsport_ssl_credentials_t *security,<br>                            cy_mqtt_broker_info_t *broker_info,<br>                            __cy_mqtt_callback_t__ __event_callback__,<br>                            __void__ __\*user_data__,<br>                            cy_mqtt_t *mqtt_handle );|

### Newly added APIs to MQTT

Below are the list of new APIs added to *MQTT* library. For more details on API usage please refer to [API documentation](https://github.com/Infineon/mqtt/api_reference_manual/html/index.html).

 - cy_mqtt_get_handle
 - cy_mqtt_register_event_callback
