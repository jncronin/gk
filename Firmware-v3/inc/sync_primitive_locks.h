#include "osmutex.h"
#include <unordered_set>
#include "process.h"

template <class PrimType, class OwnerType> void add_sync_primitive(PrimType *primitive,
    std::unordered_set<PrimType *> &pset,
    OwnerType *owner)
{
    CriticalGuard cg;
    pset.insert(primitive);
}

template <class PrimType, class OwnerType> void delete_sync_primitive(PrimType *primitive,
    std::unordered_set<PrimType *> &pset,
    OwnerType *owner)
{
    CriticalGuard cg;
    auto iter = pset.find(primitive);
    if(iter != pset.end())
        pset.erase(iter);
}

template <class PrimType> void delete_all_process_sync_primitives(
    const std::unordered_set<PrimType *> &pset,
    Process *p)
{
    CriticalGuard cg;
    for(auto prim : pset)
    {
        delete prim;
    }
}

template <class PrimType> void unlock_all_thread_sync_primitives(
    const std::unordered_set<PrimType *> &pset,
    Thread *t)
{
    CriticalGuard cg;
    for(auto prim : pset)
    {
        prim->unlock();
    }
}
