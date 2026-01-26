#include "proc_vmem.h"

static MemBlock null_memblock{};

int MapVBlockAllocator::Traverse(traversal_function_t tf)
{
    MutexGuard cg(m);
    for(auto &b : l)
    {
        auto &bb = b.second;
        tf(bb);
    }
    return 0;
}

MemBlock &MapVBlockAllocator::IsAllocated(uintptr_t addr)
{
    MutexGuard cg(m);
    // get the index to the left
    auto iter = l.find(addr);
    if(iter != l.end())
    {
        // exact match
        auto &bb = iter->second;
        return bb;
    }

    // otherwise do an insert to find the appropriate place
    MemBlock new_nullblock;
    auto cloc = l.insert_or_assign(addr, std::move(new_nullblock));

    // if nothing to the left then fail
    if(cloc.first == l.begin())
    {
        l.erase(cloc.first);
        return null_memblock;
    }

    auto prev = std::prev(cloc.first);
    auto &bb = prev->second;
    l.erase(cloc.first);

    if((addr >= bb.b.base) && (addr < bb.b.end()))
    {
        // success
        return bb;
    }

    return null_memblock;
}

VMemBlock MapVBlockAllocator::AllocFixed(MemBlock region)
{
    MutexGuard cg(m);
    // if there is an exact match then fail
    auto addr = region.b.base;
    auto addrend = region.b.end();
    auto ret = region.b;
    region.b.valid = true;

    auto iter = l.find(addr);
    if(iter != l.end())
    {
        // exact match
        return InvalidVMemBlock();
    }

    // try an insert
    auto cloc = l.insert_or_assign(addr, region);

    // test block before for intersection
    bool fail = false;
    if(cloc.first != l.begin())
    {
        auto before = std::prev(cloc.first);

        if(addr < before->second.b.end())
        {
            // fail
            fail = true;
        }
    }

    // test block after for intersection
    if(!fail)
    {
        auto next_block = std::next(cloc.first);
        if(next_block != l.end())
        {
            if(addrend > next_block->second.b.base)
            {
                // fail
                fail = true;
            }
        }
    }

    // delete just inserted block if fail
    if(fail)
    {
        l.erase(cloc.first);
        return InvalidVMemBlock();
    }

    ret.valid = true;
    return ret;
}

int MapVBlockAllocator::Dealloc(VMemBlock &region)
{
    MutexGuard cg(m);

    auto iter = l.find(region.base);
    if(iter != l.end())
    {
        // found
        l.erase(iter);
        return 0;
    }

    // not found
    return -1;
}

int VBlockAllocator::Dealloc(MemBlock& region)
{
    return Dealloc(region.b);
}

VMemBlock MapVBlockAllocator::AllocAny(MemBlock region, bool lowest_first)
{
    auto len = (uintptr_t)region.b.length;
    region.b.valid = true;

    // for a map with 'n' used entries, there are potentially 'n+1' empty spaces
    //  either side/between the used entries.  Iterate to check all of them
    
    if(lowest_first)
    {
        for(auto iter = l.begin(); iter != l.end(); iter++)
        {
            // if begin, check from base to us, and from us to next
            //  otherwise, just check from us to next

            if (iter == l.begin())
            {
                auto prev = nullptr;
                auto next = &iter->second;

                auto [fr, saddr, _] = fits(len, prev, next);
                if (fr)
                {
                    region.b.base = saddr;
                    l.insert_or_assign(region.b.base, region);
                    return region.b;
                }
            }

            {
                auto prev = &iter->second;
                auto niter = std::next(iter);
                auto next = (niter == l.end()) ? nullptr :
                    &niter->second;

                auto [fr, saddr, _] = fits(len, prev, next);
                if (fr)
                {
                    region.b.base = saddr;
                    l.insert_or_assign(region.b.base, region);
                    return region.b;
                }
            }
        }
    }
    else
    {
        for(auto iter = l.rbegin(); iter != l.rend(); iter++)
        {
            // if rbegin, check from us to end, and from rnext to us
            //  otherwise, just check from rnext to us

            if (iter == l.rbegin())
            {
                auto next = nullptr;
                auto prev = &iter->second;

                auto [fr, _, eaddr] = fits(len, prev, next);
                if (fr)
                {
                    region.b.base = eaddr - len;
                    l.insert_or_assign(region.b.base, std::move(region));
                    return region.b;
                }
            }

            {
                auto next = &iter->second;
                auto piter = std::next(iter);
                auto prev = (piter == l.rend()) ? nullptr :
                    &piter->second;

                auto [fr, _, eaddr] = fits(len, prev, next);
                if (fr)
                {
                    region.b.base = eaddr - len;
                    l.insert_or_assign(region.b.base, std::move(region));
                    return region.b;
                }
            }
        }
    }

    return InvalidVMemBlock();
}

std::tuple<bool, uintptr_t, uintptr_t> MapVBlockAllocator::fits(uintptr_t len,
            MemBlock *prev,
            MemBlock *next)
{
    auto bstart = prev ? (*prev).b.end() : base;
    auto bend = next ? (*next).b.base : (base + length);

    if(bstart >= bend)
    {
        return std::make_tuple(false, 0, 0);
    }

    auto blen = bend - bstart;
    return std::make_tuple(blen >= len, bstart, bend);
}

MemBlock &MapVBlockAllocator::Split(uintptr_t address)
{
    // notimpl
    return null_memblock;
}
