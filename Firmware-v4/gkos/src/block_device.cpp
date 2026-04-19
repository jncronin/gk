#include "block_dev.h"
#include "logger.h"

std::string BlockSubDevice::name()
{
    return parent->name() + name_append;
}

size_t BlockSubDevice::block_size()
{
    return parent->block_size();
}

size_t BlockSubDevice::block_count()
{
    return nblocks;
}

int BlockSubDevice::transfer(size_t block_start, size_t blockcount,
    void *mem_address, bool is_read)
{
    if((block_start >= nblocks) || ((block_start + blockcount) > nblocks))
    {
        klog("block_sub_device: invalid access: block_start: %llu, block_count: %llu, nblocks: %llu\n",
            block_start, blockcount, nblocks);
        return -1;
    }
    return parent->transfer(block_start + offset, blockcount, mem_address, is_read);
}

bool BlockSubDevice::is_parent_relative_access_valid(size_t block_start, size_t blockcount)
{
    if((block_start < offset) || (block_start >= (offset + nblocks)) ||
        ((block_start + blockcount) > (offset + nblocks)))
    {
        return false;
    }
    return true;
}

BlockSubDevice::BlockSubDevice(std::shared_ptr<BlockDevice> _parent, size_t _offset, size_t _nblocks,
        std::string _name_append) :
        offset(_offset),
        nblocks(_nblocks),
        parent(_parent),
        name_append(_name_append)
{
}
