#include "user_fences.h"

uint32_t UserFenceManager::Register(std::shared_ptr<dma_fence> &f)
{
    auto id = next_fence_id++;
    m[id] = f;
    return id;
}

std::shared_ptr<dma_fence> UserFenceManager::Get(uint32_t id)
{
    auto iter = m.find(id);
    if(iter == m.end())
        return nullptr;
    auto ret = iter->second.lock();
    if(!ret)
    {
        m.erase(iter);
        return nullptr;
    }
    return ret;
}

