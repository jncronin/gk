#include "sd.h"
#include "usb_class.h"
#include "fs_provision.h"
#include <cstring>

extern bool usb_israwsd;

uint32_t usb_msc_cb_get_num_blocks()
{
    if(!sd_get_ready())
    {
        return 0;
    }
    else
    {
        auto size = usb_israwsd ? (unsigned int)(sd_get_size() / (uint64_t)usb_msc_cb_get_block_size) : fake_mbr_get_sector_count();
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

int usb_msc_cb_read_data(uint32_t lba, size_t nbytes, void *buf, size_t *nread)
{
    auto nsectors = nbytes / usb_msc_cb_get_block_size();
    if(lba == 0 && !usb_israwsd)
    {
        // handle fake mbr separately
        nsectors = 1;

        memcpy(buf, fake_mbr_get_mbr(), 512U);
        if(nread) *nread = 512U;
        return 0;
    }

    auto ret = sd_transfer(lba, nsectors, buf, true);
    if(ret == 0)
    {
        if(nread) *nread = nsectors * usb_msc_cb_get_block_size();
    }
    return ret;
}
