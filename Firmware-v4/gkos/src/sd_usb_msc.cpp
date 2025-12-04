#include "sd.h"
#include "usb_class.h"

uint32_t usb_msc_cb_get_num_blocks()
{
    // TODO

    return 16*1024*1024 / usb_msc_cb_get_block_size();
}

uint32_t usb_msc_cb_get_block_size()
{
    return 512U;
}

bool usb_msc_cb_get_is_ready()
{
    // TODO
    return true;
}
