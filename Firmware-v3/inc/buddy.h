#ifndef BUDDY_H
#define BUDDY_H

#include <cstdint>
#include <cstring>

#include "util.h"
#include "SEGGER_RTT.h"

struct BuddyEntry
{
    uint32_t base;
    uint32_t length;
    bool valid;
};

constexpr static bool is_power_of_2(uint32_t v)
{
    if(!v)
        return false;
    while(!(v & 0x1UL)) v >>= 1;
    return v == 0x1UL;
}

constexpr static bool is_multiple_of(uint32_t num, uint32_t denom)
{
    return (num % denom) == 0;
}

template <uint32_t min_buddy_size, uint32_t tot_length, uint32_t base_addr> class BuddyAllocator
{
    public:

    protected:
        static_assert(is_power_of_2(min_buddy_size));
        static_assert(is_power_of_2(tot_length));
        static_assert(is_multiple_of(base_addr, min_buddy_size));
        static_assert(tot_length >= min_buddy_size);

        constexpr static uint32_t level_buddy_size(uint32_t level_id)
        {
            return min_buddy_size << level_id;
        }

        constexpr static uint32_t bits_for_level(uint32_t level_id)
        {
            uint32_t ret = tot_length / level_buddy_size(level_id);
            if(tot_length % level_buddy_size(level_id))
                ret++;
            return ret;
        }

        constexpr static uint32_t words_for_level(uint32_t level_id)
        {
            uint32_t ret = bits_for_level(level_id) / 32;
            if(bits_for_level(level_id) % 32)
                ret++;
            return ret;            
        }

        constexpr static uint32_t num_levels()
        {
            uint32_t nlevels = 1;
            uint32_t cur_buddy_size = min_buddy_size;
            while(cur_buddy_size != tot_length)
            {
                nlevels++;
                cur_buddy_size <<= 1;
            }
            return nlevels;
        }

        constexpr static uint32_t total_words()
        {
            uint32_t nwords = 0;

            for(uint32_t i = 0; i < num_levels(); i++)
            {
                nwords += words_for_level(i);
            }

            return nwords;
        }

        constexpr static uint32_t buddy_size_to_level(uint32_t size)
        {
            return __CLZ(min_buddy_size) - __CLZ(size);
        }

        constexpr static uint32_t addr_to_bitidx_at_level(uint32_t level, uint32_t addr)
        {
            return addr / level_buddy_size(level);
        }

        uint32_t lock()
        {
            auto ret = DisableInterrupts();
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
            while(true)
            {
                uint32_t expected_zero = 0;
                cmpxchg(&_lock_val, &expected_zero, 1UL);
                if(expected_zero)
                {
                    // spin in non-locking mode until unset
                    while(_lock_val) __DMB();
                }
                else
                {
                    __DMB();
                    return ret;
                }
            }
#else
            return ret;
#endif
        }

        void unlock(uint32_t cpsr)
        {
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
            set(&_lock_val, 0UL);
            __DMB();
#endif
            RestoreInterrupts(cpsr);
        }

        uint32_t level_starts[num_levels()];
        uint32_t level_word_counts[num_levels()];
        uint32_t b[total_words()];
#if GK_DUAL_CORE | GK_DUAL_CORE_AMP
        volatile uint32_t _lock_val = 0;
#endif

        void release_at_level(uint32_t level, uint32_t bitidx)
        {
            auto wordidx = bitidx / 32;
            auto bit_within_wordidx = bitidx % 32;
            auto comp_bit = (bit_within_wordidx & 1UL) ? (bit_within_wordidx - 1UL) :
                (bit_within_wordidx + 1UL);

            auto wptr = &b[level_starts[level] + wordidx];
            if(*wptr & (1UL << comp_bit) && level < (num_levels() - 1))
            {
                // can release at higher level
                // first unset complementary bit
                *wptr &= ~(1UL << comp_bit);
                
                release_at_level(level + 1, bitidx / 2);
            }
            else
            {
                // just release at this level
                *wptr |= (1UL << bit_within_wordidx);
            }
        }

        uint32_t acquire_at_level(uint32_t level)
        {
            // first see if we can acquire a value at this level
            uint32_t nwords = level_word_counts[level];
            uint32_t lstart = level_starts[level];
            for(uint32_t i = 0; i < nwords; i++)
            {
                auto wptr = &b[lstart + i];
                if(*wptr)
                {
                    // we have a valid value here - return it
                    auto bval = 31 - __CLZ(*wptr);
                    *wptr &= ~(1UL << bval);
                    return bval + i * 32;
                }
            }

            // we didn't manage to find a free bit at this level, check higher if possible
            if(level < (num_levels() - 1))
            {
                auto bval = acquire_at_level(level + 1);
                if(bval != 0xffffffffUL)
                {
                    // valid bit, in the current level it is going to be doubled
                    bval <<= 1;

                    auto word_idx = bval / 32;
                    auto bit_idx = bval % 32;

                    // set the complementary bit
                    auto wptr = &b[lstart + word_idx];
                    *wptr |= (1UL << (bit_idx + 1));

                    // return our bitindex
                    return bval;
                }
            }

            // fail
            return 0xffffffffUL;
        }

        uint32_t get_smallest_buddy_size_for_block(uint32_t *addr)
        {
            // align addr up to multiple of min_buddy_size
            auto rem = *addr % min_buddy_size;
            if(rem)
            {
                *addr += min_buddy_size;
                *addr -= rem;
            }

            uint32_t cur_length = tot_length;
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
            uint32_t cpsr;

            if(be.valid)
            {
                auto length = be.length;
                // round up to a buddy size
                if(!is_power_of_2(length))
                {
                    BKPT();
                    length = length == 1UL ? 1UL : 1UL << (32-__CLZ(length - 1UL));
                }
                while(length < min_buddy_size)
                {
                    BKPT();
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
                uint32_t cur_addr = be.base - base_addr;
                uint32_t max_addr = be.base - base_addr + be.length;

                cpsr = lock();
                while(true)
                {
                    uint32_t cur_buddy_size = get_smallest_buddy_size_for_block(&cur_addr);

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

        BuddyEntry acquire(uint32_t length)
        {
            BuddyEntry ret;

            // round up to a buddy size
            if(!is_power_of_2(length))
            {
                length = length == 1UL ? 1UL : 1UL << (32-__CLZ(length - 1UL));
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

            if(bitret == 0xffffffffUL)
            {
                // failed
                ret.base = 0UL;
                ret.length = 0UL;
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

        void init()
        {
            // init buddy to zero, get start values
            // can't use constructor here because needs to be set up prior to malloc, which
            //  may be used in another constructor, and there is no guarantee of order of calling
            //  static constructors
            memset(b, 0, 4*total_words());
            uint32_t cur_start = 0;
            for(uint32_t i = 0; i < num_levels(); i++)
            {
                level_starts[i] = cur_start;
                auto word_count = words_for_level(i);
                level_word_counts[i] = word_count;
                cur_start += word_count;
            }
        }

        constexpr uint32_t MinBuddySize() { return min_buddy_size; };

        void dump(int (*print_func)(const char *format, ...))
        {
            auto cpsr = lock();

            print_func("buddy: begin dump for region %08x-%08x, min_buddy_size: %u, nlevels: %u, nwords: %u\n",
                base_addr, base_addr + tot_length,
                min_buddy_size, num_levels(),
                total_words());
            for(unsigned int clevel = 0; clevel < num_levels(); clevel++)
            {
                auto cwc = level_word_counts[clevel];
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
                            print_func("buddy:    FREE: %08x-%08x\n", addr, addr + cbs);
                        }
                    }
                }
            }

            print_func("buddy: end buddy dump\n");

            unlock(cpsr);
        }
};

#endif
