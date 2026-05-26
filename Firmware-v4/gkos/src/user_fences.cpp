#include "user_fences.h"

uint32_t UserFenceManager::Register(std::shared_ptr<dma_fence> &f, std::shared_ptr<UserFenceManager> &fm)
{
    CriticalGuard cg(sl);
    auto id = next_fence_id++;
    m[id] = f;
    f->id = id;
    f->has_id = true;
    f->ufm = fm;
    if(m.size() > 100)
        klog("userfencemanager: size: %u\n", m.size());
    return id;
}

std::shared_ptr<dma_fence> UserFenceManager::Get(uint32_t id)
{
    CriticalGuard cg(sl);
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

void UserFenceManager::Deregister(uint32_t id)
{
    CriticalGuard cg(sl);
    auto iter = m.find(id);
    if(iter != m.end())
        m.erase(iter);
}
