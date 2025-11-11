#ifndef BUDDY_H
#define BUDDY_H

#include <cstdint>
#include <cstring>
#include "osmutex.h"
#include "logger.h"

struct BuddyEntry
{
    uint64_t base;
    uint64_t length;
    bool valid;
};

constexpr static bool is_power_of_2(uint64_t v)
{
    if(!v)
        return false;
    while(!(v & 0x1ULL)) v >>= 1;
    return v == 0x1ULL;
}

constexpr static bool is_multiple_of(uint64_t num, uint64_t denom)
{
    return (num % denom) == 0;
}

template <uint64_t min_buddy_size, uint64_t max_buddy_size, uint64_t base_addr> class BuddyAllocator
{
    public:

    protected:
        static_assert(is_power_of_2(min_buddy_size));
        static_assert(is_multiple_of(base_addr, min_buddy_size));
        static_assert(max_buddy_size >= min_buddy_size);

        constexpr static uint64_t level_buddy_size(uint64_t level_id)
        {
            return min_buddy_size << level_id;
        }

        constexpr static uint64_t bits_for_level(uint64_t level_id, uint64_t total_length)
        {
            uint64_t ret = total_length / level_buddy_size(level_id);
            if(total_length % level_buddy_size(level_id))
                ret++;
            return ret;
        }

        constexpr static uint64_t qwords_for_level(uint64_t level_id, uint64_t total_length)
        {
            auto bfl = bits_for_level(level_id, total_length);
            uint64_t ret = bfl / 64;
            if(bfl % 64)
                ret++;
            if(ret & 0x1ULL)
                ret++;          // align to 16 bytes
            return ret;            
        }

        constexpr static uint64_t num_levels()
        {
            uint64_t nlevels = 1;
            uint64_t cur_buddy_size = min_buddy_size;
            while(cur_buddy_size != max_buddy_size)
            {
                nlevels++;
                cur_buddy_size <<= 1;
            }
            return nlevels;
        }

        constexpr static uint64_t total_qwords(uint64_t total_length)
        {
            uint64_t nwords = 0;

            for(uint64_t i = 0; i < num_levels(); i++)
            {
                nwords += qwords_for_level(i, total_length);
            }

            return nwords;
        }

        constexpr static uint64_t buddy_size_to_level(uint64_t size)
        {
            return __builtin_clzll(min_buddy_size) - __builtin_clzll(size);
        }

        constexpr static uint64_t addr_to_bitidx_at_level(uint64_t level, uint64_t addr)
        {
            return addr / level_buddy_size(level);
        }

        uint64_t lock()
        {
            auto ret = DisableInterrupts();
            sl.lock();
            return ret;
        }

        void unlock(uint64_t cpsr)
        {
            sl.unlock();
            RestoreInterrupts(cpsr);
        }

        uint64_t level_starts[num_levels()];
        uint64_t level_qword_counts[num_levels()];
        uint64_t *b;
        Spinlock sl;

        void release_at_level(uint64_t level, uint64_t bitidx)
        {
            auto qwordidx = bitidx / 64ULL;
            auto bit_within_qwordidx = bitidx % 64ULL;
            auto comp_bit = (bit_within_qwordidx & 1ULL) ? (bit_within_qwordidx - 1ULL) :
                (bit_within_qwordidx + 1ULL);

            auto wptr = &b[level_starts[level] + qwordidx];
            if(*wptr & (1ULL << comp_bit) && level < (num_levels() - 1))
            {
                // can release at higher level
                // first unset complementary bit
                *wptr &= ~(1ULL << comp_bit);
                
                release_at_level(level + 1, bitidx / 2);
            }
            else
            {
                // just release at this level
                *wptr |= (1ULL << bit_within_qwordidx);
            }
        }

        uint64_t acquire_at_level(uint64_t level)
        {
            // first see if we can acquire a value at this level
            uint64_t nqwords = level_qword_counts[level];
            uint64_t lstart = level_starts[level];
            for(uint64_t i = 0; i < nqwords; i++)
            {
                auto wptr = &b[lstart + i];
                if(*wptr)
                {
                    // we have a valid value here - return it
                    auto bval = 63ULL - __builtin_clzll(*wptr);
                    klog("buddy: found free level %llu at %llu:%llu, old qword=%llx & %llx\n", level, i, bval, *wptr, (uintptr_t)wptr);
                    *wptr &= ~(1ULL << bval);

                    klog("buddy: found free level %llu at %llu:%llu, new qword=%llx & %llx\n", level, i, bval, *wptr, (uintptr_t)wptr);
                    return bval + i * 64ULL;
                }
            }

            // we didn't manage to find a free bit at this level, check higher if possible
            if(level < (num_levels() - 1))
            {
                auto bval = acquire_at_level(level + 1);
                if(bval != 0xffffffffffffffffULL)
                {
                    // valid bit, in the current level it is going to be doubled
                    bval <<= 1;

                    auto qword_idx = bval / 64;
                    auto bit_idx = bval % 64;

                    // set the complementary bit
                    auto wptr = &b[lstart + qword_idx];
                    *wptr |= (1ULL << (bit_idx + 1));

                    klog("buddy: complementary bit set level %llu at %llu:%llu, new qword=%llx @ %llx\n", level, qword_idx, bit_idx, *wptr, (uintptr_t)wptr);

                    // return our bitindex
                    return bval;
                }
            }

            // fail
            return 0xffffffffffffffffULL;
        }

        uint64_t get_smallest_buddy_size_for_block(uint64_t *addr)
        {
            // align addr up to multiple of min_buddy_size
            auto rem = *addr % min_buddy_size;
            if(rem)
            {
                *addr += min_buddy_size;
                *addr -= rem;
            }

            uint64_t cur_length = max_buddy_size;
            while(true)
            {
                if((*addr % cur_length) == 0)
                {
                    return cur_length;
                }
                else
                {
                    cur_length >>= 1;
                }
            }
        }

    public:
        void release(const BuddyEntry &be)
        {
            uint64_t cpsr;

            if(be.valid)
            {
                auto length = be.length;
                // round up to a buddy size
                if(!is_power_of_2(length))
                {
                    //BKPT();
                    length = length == 1ULL ? 1ULL : 1ULL << (64-__builtin_clzll(length - 1ULL));
                }
                while(length < min_buddy_size)
                {
                    //BKPT();
                    length <<= 1;
                }

                auto level = buddy_size_to_level(length);

                cpsr = lock();
                release_at_level(level,
                    addr_to_bitidx_at_level(level, be.base - base_addr));
            }
            else
            {
                // block is not aligned with a buddy level
                uint64_t cur_addr = be.base - base_addr;
                uint64_t max_addr = be.base - base_addr + be.length;

                cpsr = lock();
                while(true)
                {
                    uint64_t cur_buddy_size = get_smallest_buddy_size_for_block(&cur_addr);

                    if(cur_addr >= max_addr || max_addr - cur_addr < min_buddy_size)
                    {
                        unlock(cpsr);
                        return;
                    }

                    while((cur_addr + cur_buddy_size) > max_addr)
                    {
                        cur_buddy_size >>= 1;
                    }
                    if(cur_buddy_size < min_buddy_size)
                    {
                        unlock(cpsr);
                        return;
                    }
                    
                    auto level = buddy_size_to_level(cur_buddy_size);
                    release_at_level(level, addr_to_bitidx_at_level(level, cur_addr));

                    cur_addr += cur_buddy_size;
                }
            }

            unlock(cpsr);
        }

        BuddyEntry acquire(uint64_t length)
        {
            BuddyEntry ret;

            // round up to a buddy size
            if(!is_power_of_2(length))
            {
                length = length == 1ULL ? 1ULL : 1ULL << (64-__builtin_clzll(length - 1ULL));
            }
            while(length < min_buddy_size)
            {
                length <<= 1;
            }

            auto level = buddy_size_to_level(length);
            if(level >= num_levels())
            {
                // too large
                ret.base = 0UL;
                ret.length = 0UL;
                ret.valid = false;
                return ret;
            }

            auto cpsr = lock();
            auto bitret = acquire_at_level(level);
            unlock(cpsr);

            if(bitret == 0xffffffffffffffffULL)
            {
                // failed
                ret.base = 0ULL;
                ret.length = 0ULL;
                ret.valid = false;
            }
            else
            {
                ret.base = base_addr + bitret * length;
                ret.length = length;
                ret.valid = true;
            }

            return ret;
        }

        void init(void *mem, uint64_t total_length)
        {
            // init buddy to zero, get start values
            // can't use constructor here because needs to be set up prior to malloc, which
            //  may be used in another constructor, and there is no guarantee of order of calling
            //  static constructors
            b = (uint64_t *)mem;
            memset(b, 0, 8*total_qwords(total_length));
            uint64_t cur_start = 0;
            for(uint64_t i = 0; i < num_levels(); i++)
            {
                level_starts[i] = cur_start;
                auto qword_count = qwords_for_level(i, total_length);
                level_qword_counts[i] = qword_count;
                cur_start += qword_count;
            }
        }

        constexpr uint64_t MinBuddySize() { return min_buddy_size; };
        constexpr uint64_t MaxBuddySize() { return max_buddy_size; };
        constexpr uint64_t Base() { return base_addr; }
        constexpr uint64_t BuddyMemSize(uint64_t total_length) { return total_qwords(total_length) * 8ULL; }

        void dump(int (*print_func)(const char *format, ...))
        {
            auto cpsr = lock();

            print_func("buddy: begin dump for region %16llx-%16llx, min_buddy_size: %llu, nlevels: %llu\n",
                base_addr, base_addr + level_qword_counts[0] * 8ULL * min_buddy_size,
                min_buddy_size, num_levels());
            for(unsigned int clevel = 0; clevel < num_levels(); clevel++)
            {
                auto cwc = level_qword_counts[clevel];
                auto cstart = level_starts[clevel];
                auto cbs = level_buddy_size(clevel);

                print_func("buddy:  level: %u, size: %u, start: %u, nwords: %u\n",
                    clevel, cbs, cstart, cwc);

                for(unsigned int cword = 0; cword < cwc; cword++)
                {
                    auto cwval = b[cstart + cword];
                    if(!cwval)
                        continue;
                    for(unsigned int cbit = 0; cbit < 32; cbit++)
                    {
                        if((cwval >> cbit) & 0x1)
                        {
                            // found free bit
                            auto addr = base_addr + (cword * 32 + cbit) * cbs;
                            print_func("buddy:    FREE: %16llx-%16llx\n", addr, addr + cbs);
                        }
                    }
                }
            }

            print_func("buddy: end buddy dump\n");

            unlock(cpsr);
        }
};

#endif
