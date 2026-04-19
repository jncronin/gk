#ifndef MBR_H
#define MBR_H

#include "block_dev.h"
#include <memory>
#include <string>
#include <vector>

struct mbr_dev
{
    unsigned int part_no;
    std::string driver;
    std::shared_ptr<BlockDevice> d;
};

using mbr_devs_t = std::vector<mbr_dev>;

mbr_devs_t mbr_parse(std::shared_ptr<BlockDevice> dev);

#endif
