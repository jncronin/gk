#include "osmutex.h"
#include <unordered_set>
#include "process.h"

template <class PrimType, class OwnerType> void GKOS_FUNC(add_sync_primitive)(PrimType *primitive,
    std::unordered_set<PrimType *> &pset,
    OwnerType *owner)
{
    GKOS_FUNC(CriticalGuard) cg(owner->sl);
    pset.insert(primitive);
}

template <class PrimType, class OwnerType> void GKOS_FUNC(delete_sync_primitive)(PrimType *primitive,
    std::unordered_set<PrimType *> &pset,
    OwnerType *owner)
{
    GKOS_FUNC(CriticalGuard) cg(owner->sl);
    auto iter = pset.find(primitive);
    if(iter != pset.end())
        pset.erase(iter);
}

template <class PrimType> void GKOS_FUNC(delete_all_process_sync_primitives)(
    const std::unordered_set<PrimType *> &pset,
    GKOS_FUNC(Process) *p)
{
    GKOS_FUNC(CriticalGuard) cg(p->sl);
    for(auto prim : pset)
    {
        delete prim;
    }
}

template <class PrimType> void GKOS_FUNC(unlock_all_thread_sync_primitives)(
    const std::unordered_set<PrimType *> &pset,
    Thread *t)
{
    GKOS_FUNC(CriticalGuard) cg(t->sl);
    for(auto prim : pset)
    {
        prim->unlock();
    }
}
