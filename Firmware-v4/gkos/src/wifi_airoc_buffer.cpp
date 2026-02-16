#include "cy_network_buffer.h"
#include "logger.h"
#include <cstdlib>
#include "osnet.h"
#include "whd_wifi_api.h"

const size_t cy_header = NET_SIZE_ETHERNET_HEADER + NET_SIZE_WIFI_HEADER;
const size_t cy_footer = NET_SIZE_ETHERNET_FOOTER;

whd_result_t cy_host_buffer_get(whd_buffer_t* buffer, whd_buffer_dir_t direction,
                                uint16_t size, uint32_t timeout_ms)
{
    auto pbuf = net_allocate_pbuf(size, cy_header);
    if(!pbuf)
    {
        return WHD_MALLOC_FAILURE;
    }
    *buffer = pbuf;
    return WHD_SUCCESS;
}

void cy_buffer_release(whd_buffer_t buffer, whd_buffer_dir_t direction)
{
    net_deallocate_pbuf((pbuf_t)buffer);
}

uint8_t* cy_buffer_get_current_piece_data_pointer(whd_buffer_t buffer)
{
    return (uint8_t *)((pbuf_t)buffer)->Ptr();
}

whd_result_t cy_buffer_set_size(whd_buffer_t buffer, uint16_t size)
{
    return (((pbuf_t)buffer)->SetSize(size) == 0) ? WHD_SUCCESS : WHD_RTOS_ERROR;
}

whd_result_t cy_buffer_add_remove_at_front(whd_buffer_t* buffer, int32_t add_remove_amount)
{
    auto pcb = reinterpret_cast<pbuf_t *>(buffer);
    auto cb = *pcb;
    return (cb->AdjustStart(add_remove_amount) == 0) ? WHD_SUCCESS : WHD_RTOS_ERROR;
}

uint16_t cy_buffer_get_current_piece_size(whd_buffer_t buffer)
{
    return (uint16_t)((pbuf_t)buffer)->GetSize();
}

void cy_network_process_ethernet_data(whd_interface_t interface, whd_buffer_t buffer)
{
    WifiNetInterface *iface;
    whd_wifi_get_private_data(interface, (void **)&iface);

    net_inject_ethernet_packet((pbuf_t)buffer, iface);

    //klog("cy_network_process_ethernet_data\n");
}
