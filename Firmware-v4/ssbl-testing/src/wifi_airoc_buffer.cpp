#include "cy_network_buffer.h"

whd_result_t cy_host_buffer_get(whd_buffer_t* buffer, whd_buffer_dir_t direction,
                                uint16_t size, uint32_t timeout_ms)
{
    return WHD_RTOS_ERROR;
}

void cy_buffer_release(whd_buffer_t buffer, whd_buffer_dir_t direction)
{

}

uint8_t* cy_buffer_get_current_piece_data_pointer(whd_buffer_t buffer)
{
    return nullptr;
}

whd_result_t cy_buffer_set_size(whd_buffer_t buffer, uint16_t size)
{
    return WHD_RTOS_ERROR;
}

whd_result_t cy_buffer_add_remove_at_front(whd_buffer_t* buffer, int32_t add_remove_amount)
{
    return WHD_RTOS_ERROR;
}

uint16_t cy_buffer_get_current_piece_size(whd_buffer_t buffer)
{
    return 0;
}

void cy_network_process_ethernet_data(whd_interface_t interface, whd_buffer_t buffer)
{

}
