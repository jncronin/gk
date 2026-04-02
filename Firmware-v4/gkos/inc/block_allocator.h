#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <cstdint>
#include <map>
#include <memory>
#include <limits>

/* Defines an allocator with base and length that does not allow blocks to
    overlap.

    Requires external synchronization.

    Provides AllocFixed and AllocAny methods which return an iterator to
    a map entry.
    
    Provides IsAllocated which returns an iterator if the given
    address is allocated.

    Provides Dealloc (iterator and address-based) functions.

    Provides iterators to the underlying map.
*/

template<typename KeyT, typename AddrT = uintptr_t> class BlockAllocator
{
    public:
        struct BlockAddress
        {
            AddrT start, length;

            bool operator< (const BlockAddress &other) const
            {
                return start < other.start;
            }

            AddrT end() const { return start + length; }
        };

        using MapT = std::map<BlockAddress, KeyT>;
        using iterator = MapT::iterator;

    protected:
        MapT l;

        /* Determines if there is enough space for a block of size
            len between two other blocks:
                prev if specified or the start of the address space
                next if specified or the end of the address space.
            Returns [ false, _invalid_ ] if it doesn't fit,
                else [ true, empty space ] if it does */
        std::tuple<bool, BlockAddress> fits(AddrT len,
            const BlockAddress *prev, const BlockAddress *next)
        {
            auto bstart = prev ? (*prev).end() : addrspace.start;
            auto bend = next ? (*next).start : addrspace.end();

            if(bstart >= bend)
            {
                return std::make_tuple(false, BlockAddress{});
            }

            auto blen = bend - bstart;
            return std::make_tuple(blen >= len, BlockAddress { bstart, blen });
        }

        BlockAddress addrspace;

    public:
        BlockAllocator(BlockAddress _addrspace =
            {
                std::numeric_limits<AddrT>::min(),
                std::numeric_limits<AddrT>::max()
            }) : addrspace(_addrspace) { }

        BlockAllocator(AddrT start, AddrT length)
        {
            addrspace.start = start;
            addrspace.length = length;
        }

        iterator AllocFixed(BlockAddress region, KeyT &&val = KeyT{})
        {
            // if there is an exact match then fail
            auto iter = l.find(region);
            if(iter != l.end())
            {
                // exact match
                return l.end();
            }

            // try an insert
            auto cloc = l.insert_or_assign(region, val);

            // test block before for intersection
            bool fail = false;
            if(cloc.first != l.begin())
            {
                auto before = std::prev(cloc.first);

                if(region.start < before->first.end())
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
                    if(region.end() > next_block->first.start)
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
                return l.end();
            }

            return cloc.first;
        }

        iterator AllocAny(AddrT len, KeyT &&val = KeyT{}, bool lowest_first = true)
        {
            BlockAddress region { .start = 0, .length = len };

            // The following logic does not work with an empty map, special case this
            if(l.size() == 0)
            {
                if(len > addrspace.length)
                {
                    return l.end();
                }
                if(lowest_first)
                {
                    region.start = addrspace.start;
                }
                else
                {
                    region.start = addrspace.end() - len;
                }
                return l.insert_or_assign(region, val).first;
            }

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
                        auto next = &iter->first;

                        auto [ fr, baddr ] = fits(len, prev, next);
                        if (fr)
                        {
                            region.start = baddr.start;
                            return l.insert_or_assign(region, val).first;
                        }
                    }
                    else
                    {
                        auto prev = &iter->first;
                        auto niter = std::next(iter);
                        auto next = (niter == l.end()) ? nullptr :
                            &niter->first;

                        auto [ fr, baddr ] = fits(len, prev, next);
                        if (fr)
                        {
                            region.start = baddr.start;
                            return l.insert_or_assign(region, val).first;
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
                        auto prev = &iter->first;

                        auto [ fr, baddr ] = fits(len, prev, next);
                        if (fr)
                        {
                            region.start = baddr.end() - len;
                            return l.insert_or_assign(region, val).first;
                        }
                    }
                    else
                    {
                        auto next = &iter.base()->first;
                        auto piter = std::next(iter);
                        auto prev = (piter == l.rend()) ? nullptr :
                            &piter.base()->first;

                        auto [ fr, baddr ] = fits(len, prev, next);
                        if (fr)
                        {
                            region.start = baddr.end() - len;
                            return l.insert_or_assign(region, val).first;
                        }
                    }
                }
            }

            return l.end();
        }

        iterator IsAllocated(AddrT addr)
        {
            BlockAddress test { .start = addr, .length = 0 };

            /* Is there an exact match for addr? */
            auto iter = l.find(test);
            if(iter != l.end())
            {
                return iter;
            }

            /* If not, find the block immediately to the left of the test block */
            // do a dummy insert to get an iterator
            auto cloc = l.insert_or_assign(test, KeyT{});

            // if nothing to the left then fail
            if(cloc.first == l.begin())
            {
                l.erase(cloc.first);
                return l.end();
            }

            auto prev = std::prev(cloc.first);
            auto &prev_region = prev->first;
            l.erase(cloc.first);

            if((addr >= prev_region.start) && (addr < prev_region.end()))
            {
                // success
                return prev;
            }

            return l.end();
        }

        /* Find the highest block with start address equal to or lower than addr */
        iterator LeftBlock(AddrT addr)
        {
            BlockAddress test { .start = addr, .length = 0 };

            /* Is there an exact match for addr? */
            auto iter = l.find(test);
            if(iter != l.end())
            {
                return iter;
            }

            /* If not, find the block immediately to the left of the test block */
            // do a dummy insert to get an iterator
            auto cloc = l.insert_or_assign(test, KeyT{});

            // if nothing to the left then fail
            if(cloc.first == l.begin())
            {
                l.erase(cloc.first);
                return l.end();
            }

            auto prev = std::prev(cloc.first);
            l.erase(cloc.first);

            return prev;
        }

        /* Find the lowest block with address higher than addr */
        iterator RightBlock(AddrT addr)
        {
            BlockAddress test { .start = addr, .length = 0 };

            /* Is there an exact match for addr? */
            auto iter = l.find(test);
            if(iter == l.end())
            {
                /* If not, find the block immediately to the right of the test block */
                // do a dummy insert to get an iterator
                auto cloc = l.insert_or_assign(test, KeyT{});

                // if nothing to the right then fail
                if(cloc.first == l.end())
                {
                    l.erase(cloc.first);
                    return l.end();
                }

                auto next = std::next(cloc.first);
                l.erase(cloc.first);
                return next;
            }
            else
            {
                auto next = std::next(iter);
                return next;
            }
        }

        iterator Dealloc(iterator pos)
        {
            return l.erase(pos);
        }

        iterator Dealloc(AddrT addr)
        {
            auto iter = IsAllocated(addr);
            if(iter != l.end())
                return l.erase(iter);
            else
                return l.end();
        }

        /* Allows the key in the underlying map to be changed */
        iterator ChangeAddress(iterator iter, BlockAddress addr)
        {
            auto node = l.extract(iter);
            node.key() = addr;
            return l.insert(std::move(node)).position;
        }

        iterator begin() { return l.begin(); }
        iterator end() { return l.end(); }
        iterator rbegin() { return l.rbegin(); }
        iterator rend() { return l.rend(); }
        iterator cbegin() { return l.cbegin(); }
        iterator cend() { return l.cend(); }
        iterator crbegin() { return l.crbegin(); }
        iterator crend() { return l.crend(); }        
};

#endif
