#ifndef USER_FENCES_H
#define USER_FENCES_H

#include "fencefile.h"
#include <memory>

class UserFenceManager
{
    protected:
        uint32_t next_fence_id = 1;
        std::unordered_map<uint32_t, std::weak_ptr<dma_fence>> m;

    public:
        uint32_t Register(std::shared_ptr<dma_fence> &f);
        std::shared_ptr<dma_fence> Get(uint32_t id);
};

#endif
