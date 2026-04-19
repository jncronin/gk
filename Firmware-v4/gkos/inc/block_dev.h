#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include <string>
#include <memory>

class BlockDevice
{
public:
    virtual std::string name() = 0;
    virtual size_t block_size() = 0;
    virtual size_t block_count() = 0;
    virtual int transfer(size_t block_start, size_t block_count, void *mem_address, bool is_read) = 0;
};

class BlockSubDevice : public BlockDevice
{
protected:
    size_t offset = 0;
    size_t nblocks = 0;
    std::shared_ptr<BlockDevice> parent;
    std::string name_append;

public:
    virtual std::string name();
    virtual size_t block_size();
    virtual size_t block_count();
    virtual int transfer(size_t block_start, size_t block_count, void *mem_address, bool is_read);
    bool is_parent_relative_access_valid(size_t block_start, size_t block_count);

    BlockSubDevice(std::shared_ptr<BlockDevice> parent, size_t offset, size_t nblocks,
        std::string name_append);
};

#endif
