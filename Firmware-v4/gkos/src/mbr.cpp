#include "mbr.h"

#include <vector>
#include <cstdint>
#include <memory>

#include "logger.h"

mbr_devs_t mbr_parse(std::shared_ptr<BlockDevice> dev)
{
    mbr_devs_t ret;
    if(dev->block_size() < 512U)
    {
        klog("mbr: device block size too small: %u\n", dev->block_size());
        return ret;
    }
    if(dev->block_count() < 1U)
    {
        klog("mbr: device block count too small: %u\n", dev->block_count());
        return ret;
    }
    std::unique_ptr<uint8_t[]> mbr(new uint8_t[dev->block_size()]);
    if(!mbr)
    {
        klog("mbr: failed to allocate mbr buffer of size %u\n", dev->block_size());
        return ret;
    }

    auto tret = dev->transfer(0, 1, mbr.get(), true);
    if(tret)
    {
        klog("mbr: transfer of block 0 failed: %d\n", tret);
        return ret;
    }

    // validate mbr
    if(mbr[0x1fe] != 0x55 || mbr[0x1ff] != 0xaa)
    {
        klog("mbr: not valid: %2x %2x\n", mbr[0x1fe], mbr[0x1ff]);
        return ret;
    }

    for(auto i = 0u; i < 4u; i++)
    {
        auto start = 0x1beu + i * 0x10u;

        auto ptype = mbr[start + 0x4u];
        auto lba_start = *(uint32_t *)&mbr[start + 0x08u];
        auto nsects = *(uint32_t *)&mbr[start + 0x0cu];

        std::string ptype_str = "";
        bool is_valid = true;

        switch(ptype)
        {
            case 0x01:
            case 0x04:
            case 0x06:
            case 0x07:
            case 0x0b:
            case 0x0c:
            case 0x0e:
            case 0x0f:
                ptype_str = "fat";
                break;

            case 0x83:
                ptype_str = "ext";
                break;

            default:
                is_valid = false;
                break;
        }

        // check extents
        if((lba_start >= dev->block_count()) || ((lba_start + nsects) > dev->block_count()))
        {
            is_valid = false;
        }

        if(is_valid)
        {
            mbr_dev newdev;
            newdev.driver = ptype_str;
            newdev.part_no = i;
            newdev.d = std::make_shared<BlockSubDevice>(dev, lba_start, nsects, "p" + std::to_string(i));

            klog("mbr: partition %u: %s, lba_start: %u, nsects: %u\n", i, ptype_str.c_str(),
                lba_start, nsects);
            ret.push_back(newdev);
        }
        else
        {
            klog("mbr: partition %u: invalid\n", i);
        }
    }

    return ret;
}
