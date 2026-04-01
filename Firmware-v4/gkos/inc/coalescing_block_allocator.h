#ifndef COALESCING_BLOCK_ALLOCATOR_H
#define COALESCING_BLOCK_ALLOCATOR_H

#include "block_allocator.h"

/*
    Automatically coalesces blocks with already allocated ones, if appropriate.
    Requires KeyT to have a function:
        void CoalesceFrom(KeyT &other, bool other_is_previous)
*/

template<typename KeyT, typename AddrT = uintptr_t> class CoalescingBlockAllocator
{
    protected:
        using ba_t = BlockAllocator<KeyT, AddrT>;

    public:
        using MapT = ba_t::MapT;
        using iterator = MapT::iterator;
        using BlockAddress = ba_t::BlockAddress;

    protected:
        ba_t a;

        iterator coalesce(iterator iter)
        {
            bool changed = false;
            BlockAddress new_ba = iter->first;

            if(iter != a.begin())
            {
                auto piter = std::prev(iter);

                if (piter->first.end() == iter->first.start)
                {
                    iter->second.CoalesceFrom(piter->second, true);
                    new_ba.start = piter->first.start;
                    new_ba.length += piter->first.length;
                    changed = true;
                    a.Dealloc(piter);
                }
            }

            auto niter = std::next(iter);
            if(niter != a.end())
            {
                if (iter->first.end() == niter->first.start)
                {
                    iter->second.CoalesceFrom(niter->second, false);
                    new_ba.length += niter->first.length;
                    changed = true;
                    a.Dealloc(niter);
                }
            }

            if (changed)
            {
                return a.ChangeAddress(iter, new_ba);
            }
            else
            {
                return iter;
            }
        }

    public:
        CoalescingBlockAllocator(BlockAddress _addrspace =
            {
                std::numeric_limits<AddrT>::min(),
                std::numeric_limits<AddrT>::max()
            }) : a(_addrspace) { }

        iterator AllocFixed(BlockAddress region, KeyT &&val = KeyT{})
        {
            auto iter = a.AllocFixed(region, std::move(val));
            if(iter != a.end())
                return coalesce(iter);
            return iter;
        }

        iterator AllocAny(AddrT len, KeyT &&val = KeyT{}, bool lowest_first = true)
        {
            auto iter = a.AllocAny(len, std::move(val), lowest_first);
            if(iter != a.end())
                return coalesce(iter);
            return iter;
        }

        iterator IsAllocated(AddrT addr)
        {
            return a.IsAllocated(addr);
        }

        iterator Dealloc(iterator pos)
        {
            return a.Dealloc(pos);
        }

        iterator Dealloc(AddrT addr)
        {
            return a.Dealloc(addr);
        }

        iterator begin() { return a.begin(); }
        iterator end() { return a.end(); }
        iterator rbegin() { return a.rbegin(); }
        iterator rend() { return a.rend(); }
        iterator cbegin() { return a.cbegin(); }
        iterator cend() { return a.cend(); }
        iterator crbegin() { return a.crbegin(); }
        iterator crend() { return a.crend(); }        
};


#endif

