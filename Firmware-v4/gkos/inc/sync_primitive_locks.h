#ifndef SYNC_PRIMITIVE_LOCKS_H
#define SYNC_PRIMITIVE_LOCKS_H

#include "osmutex.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <set>
#include "threadproclist.h"

using PMutex = std::shared_ptr<Mutex>;
using PCondition = std::shared_ptr<Condition>;
using PRWLock = std::shared_ptr<RwLock>;
using PUserspaceSemaphore = std::shared_ptr<UserspaceSemaphore>;

template <class PrimType> struct owned_sync_list
{
    SyncPrimIDList<PrimType> &global_list;
    using PT = std::shared_ptr<PrimType>;

    owned_sync_list(SyncPrimIDList<PrimType> &_global_list) : global_list(_global_list) {}

    Spinlock sl;
    std::set<id_t> pset;

    id_t add(id_t id)
    {
        CriticalGuard cg(sl, global_list.sl);
        pset.insert(id);
        return id;
    }

    id_t add(PT v)
    {
        return add(v->id);
    }

    void erase(id_t id)
    {
        CriticalGuard cg(sl, global_list.sl);
        auto iter = pset.find(id);
        if(iter != pset.end())
        {
            pset.erase(iter);
            global_list._delete(id);
        }
    }

    void clear()
    {
        CriticalGuard cg(sl, global_list.sl);
        for(auto id : pset)
        {
            global_list._delete(id);
        }
        pset.clear();
    }

    PT get(id_t id)
    {
        CriticalGuard cg(sl, global_list.sl);
        auto iter = pset.find(id);
        if(iter != pset.end())
        {
            auto ret = global_list._get(id);
            if(!ret)
                klog("sync_list_get: id %u not in global list\n", id);
            return ret;
        }
        klog("sync_list_get: id %u not in local list\n", id);
        return nullptr;
    }

    bool exists(id_t id)
    {
        CriticalGuard cg(sl, global_list.sl);
        auto iter = pset.find(id);
        if(iter == pset.end())
            return false;
        return global_list._exists(id);
    }
};

template <class PrimType> struct locked_sync_list
{
    Spinlock sl;

    std::unordered_set<id_t> pset = std::unordered_set<id_t>(8);

    void Add(id_t id)
    {
        CriticalGuard cg(sl);
        pset.insert(id);
    }

    void Delete(id_t id)
    {
        CriticalGuard cg(sl);
        pset.erase(id);
    }
};

#endif
