#include "cy_network_buffer.h"
#include "logger.h"
#include <cstdlib>

whd_result_t cy_host_buffer_get(whd_buffer_t* buffer, whd_buffer_dir_t direction,
                                uint16_t size, uint32_t timeout_ms)
{
    // TODO: more complex buffer handling here once net ported
    auto ret = malloc(size);
    if(!ret)
        return WHD_WLAN_NOMEM;
    *buffer = ret;
    return WHD_SUCCESS;
}

void cy_buffer_release(whd_buffer_t buffer, whd_buffer_dir_t direction)
{
    free(buffer);
}

uint8_t* cy_buffer_get_current_piece_data_pointer(whd_buffer_t buffer)
{
    // Needs to be more complex
    return (uint8_t *)buffer;
}

whd_result_t cy_buffer_set_size(whd_buffer_t buffer, uint16_t size)
{
    klog("cy_buffer_set_size\n");
    return WHD_RTOS_ERROR;
}

whd_result_t cy_buffer_add_remove_at_front(whd_buffer_t* buffer, int32_t add_remove_amount)
{
    klog("cy_buffer_add_remove_at_front\n");
    return WHD_RTOS_ERROR;
}

uint16_t cy_buffer_get_current_piece_size(whd_buffer_t buffer)
{
    klog("cy_buffer_get_current_piece_size\n");
    return 0;
}

void cy_network_process_ethernet_data(whd_interface_t interface, whd_buffer_t buffer)
{
    klog("cy_network_process_ethernet_data\n");
}
