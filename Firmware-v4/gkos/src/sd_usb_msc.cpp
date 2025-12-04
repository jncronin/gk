#include "sd.h"
#include "usb_class.h"
#include "fs_provision.h"

extern bool usb_israwsd;

uint32_t usb_msc_cb_get_num_blocks()
{
    if(!sd_get_ready())
    {
        return 0;
    }
    else
    {
        auto size = usb_israwsd ? (unsigned int)(sd_get_size() / 512ULL) : fake_mbr_get_sector_count();
        return static_cast<uint32_t>(size);
    }
}

uint32_t usb_msc_cb_get_block_size()
{
    return 512U;
}

bool usb_msc_cb_get_is_ready()
{
    return sd_get_ready();
}
