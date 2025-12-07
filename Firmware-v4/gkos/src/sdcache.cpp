#include "sd.h"
#include "vblock.h"
#include "osmutex.h"
#include <map>
#include <list>
#include "pmem.h"
#include "vmem.h"
#include "cache.h"
#include "process.h"

int sd_perform_transfer_async(const sd_request &req);
int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries = 10);

int sd_cache_init();

/* Implements a cache on 64 kIb SD card blocks */
static VMemBlock vb_cache;
static const constexpr unsigned int n_entries = 1024;
static unsigned int next_entry = 0;
[[maybe_unused]] static const constexpr auto cache_size = n_entries * VBLOCK_64k;
static const constexpr uint64_t block_size = 512;
static const constexpr uint64_t b_per_bb = VBLOCK_64k / block_size;

PMutex m_cache;

using sdc_idx = uint64_t;
using lru_list = std::list<sdc_idx>;
using lru_iter = lru_list::iterator;
struct map_value
{
    uintptr_t vaddr;
    uintptr_t paddr;
    lru_iter list_loc;
};
using map_type = std::map<sdc_idx, map_value>;

static lru_list sdc_list;
static map_type sdc_map;


int sd_cache_init()
{
    vb_cache = vblock_alloc(VBLOCK_512M, false, true, false);
    m_cache = MutexList.Create();
    return vb_cache.valid ? 0 : -1;
}

struct addr_ret
{
    uintptr_t vaddr;
    uintptr_t paddr;
    bool has_data;
};

static addr_ret sdc_bigblock_to_addr(sdc_idx bigblock)
{
    // see if we already have an entry
    auto miter = sdc_map.find(bigblock);
    if(miter != sdc_map.end())
    {
        // we do, so can just return it and bring the relevant listiter to the front
        const auto &e = *miter;
        const auto &mval = e.second;

        addr_ret ret;
        ret.has_data = true;
        ret.paddr = mval.paddr;
        ret.vaddr = mval.vaddr;

        // bring iter to front
        sdc_list.splice(sdc_list.begin(), sdc_list, mval.list_loc);

        return ret;
    }

    // we don't have an entry.  Is there space to add one?
    if(next_entry < n_entries)
    {
        auto vaddr = vb_cache.data_start() + next_entry * VBLOCK_64k;
        auto paddr_be = Pmem.acquire(VBLOCK_64k);
        if(!paddr_be.valid)
        {
            klog("sdc: invalid pblock\n");
            return addr_ret{};
        }
        {
            CriticalGuard cg(p_kernel->owned_pages.sl);
            p_kernel->owned_pages.add(paddr_be);
        }
        vmem_map(vaddr, paddr_be.base, false, true, false);

        addr_ret ret;
        ret.has_data = false;
        ret.vaddr = vaddr;
        ret.paddr = paddr_be.base;

        // add to start of the list
        sdc_list.push_front(bigblock);
        auto liter = sdc_list.begin();

        // add to map
        map_value mv;
        mv.list_loc = liter;
        mv.vaddr = vaddr;
        mv.paddr = paddr_be.base;
        sdc_map[bigblock] = mv;

        next_entry++;

        return ret;
    }

    // there is no space.  take the last entry out
    auto last_iter = sdc_list.end();
    last_iter--;
    auto bb_to_erase = *last_iter;
    sdc_list.pop_back();

    map_value mv_old = sdc_map[bb_to_erase];
    sdc_map.erase(bb_to_erase);
    sdc_map[bigblock] = mv_old;

    addr_ret ret;
    ret.has_data = false;
    ret.paddr = mv_old.paddr;
    ret.vaddr = mv_old.vaddr;

#if DEBUG_SDC
    klog("sdc: recycling entry for bb: %u to %u, vaddr: %llx, paddr: %llx\n",
        bb_to_erase, bigblock, ret.vaddr, ret.paddr);
#endif

    // put list iter at the beginning
    sdc_list.push_front(bigblock);
    sdc_map[bigblock].list_loc = sdc_list.begin();

    return ret;
}

static int sdc_read(sdc_idx block_start, sdc_idx block_count, void *mem_address);
static int sdc_write(sdc_idx block_start, sdc_idx block_count, const void *mem_address);

int sd_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read)
{
    m_cache->lock();
    auto ret = is_read ? sdc_read(block_start, block_count, mem_address) :
        sdc_write(block_start, block_count, mem_address);
    m_cache->unlock();
    return ret;
}

int sdc_read(sdc_idx block_start, sdc_idx block_count, void *mem_address)
{
    uintptr_t dest_addr = (uintptr_t)mem_address;

    while(block_count)
    {
        sdc_idx cur_bb = block_start / b_per_bb;
        auto offset_blocks = block_start - cur_bb * b_per_bb;
        auto offset_bytes = offset_blocks * block_size;

        auto has_bb = sdc_bigblock_to_addr(cur_bb);
        if(!has_bb.has_data)
        {
#if DEBUG_SDC
            klog("sdc: cache miss for %llu - loading %llu to v %llx p %llx\n", block_start, cur_bb * b_per_bb,
                has_bb.vaddr, has_bb.paddr);
#endif
            // load the data
            auto rret = sd_perform_transfer(cur_bb * b_per_bb, b_per_bb, (void *)has_bb.paddr, true);
#if DEBUG_SDC
            klog("sdc: big block load complete, ret %d\n", rret);
#endif
            if(rret != 0)
                return rret;
            
            InvalidateA35Cache(has_bb.vaddr, VBLOCK_64k, CacheType_t::Data, true);
        }
        else
        {
#ifdef DEBUG_SDC
            klog("sdc: cache hit for %llu @ v %llx\n", block_start, has_bb.vaddr);
#endif
        }

        memcpy((void *)dest_addr, (const void *)(has_bb.vaddr + offset_bytes), block_size);

        dest_addr += block_size;
        block_count--;
        block_start++;
    }

    return 0;
}

int sdc_write(sdc_idx block_start, sdc_idx block_count, const void *mem_address)
{
    /* Divide into bigblocks.  If the entire bigblock is written then don't need to load the
        bigblock first, otherwise do */

    uintptr_t src_addr = (uintptr_t)mem_address;

#if DEBUG_SDC
    klog("sdc: write: block_start: %llu, block_count: %llu, mem_address: %p\n",
        block_start, block_count, mem_address);
#endif

    while(block_count)
    {
        auto cur_bb = block_start / b_per_bb;
        auto b_offset_within_bb = block_start - cur_bb * b_per_bb;
        auto byte_offset_within_bb = b_offset_within_bb * block_size;
        
        auto blocks_within_bb = std::min(b_per_bb,
            std::min(b_per_bb - b_offset_within_bb, block_count));

        auto whole_bb = blocks_within_bb == b_per_bb;

        auto has_bb = sdc_bigblock_to_addr(cur_bb);

        // need to load if not already loaded and not whole_bb
        if(!has_bb.has_data && !whole_bb)
        {
#if DEBUG_SDC
            klog("sdc_write: partial bigblock write b: %llu within bb: %llx at b_offset: %llx - loading\n",
                    blocks_within_bb, cur_bb, b_offset_within_bb);
#endif
            
            // load the data
            auto rret = sd_perform_transfer(cur_bb * b_per_bb, b_per_bb, (void *)has_bb.paddr, true);
#if DEBUG_SDC
            klog("sdc: big block load complete, ret %d\n", rret);
#endif
            if(rret != 0)
                return rret;
            
            InvalidateA35Cache(has_bb.vaddr, VBLOCK_64k, CacheType_t::Data, true);
        }
        else if(has_bb.has_data)
        {
#if DEBUG_SDC
            klog("sdc_write: cache hit, skipping load\n");
#endif
        }
        else if(whole_bb)
        {
#if DEBUG_SDC
            klog("sdc_write: whole bb, skipping load\n");
#endif
        }

        // copy the data
        auto dest = has_bb.vaddr + byte_offset_within_bb;
#if DEBUG_SDC
        klog("sdc: write: has_bb.vaddr: %llx, byte_offset_within_bb: %llx, src_addr: %llx, blocks_within_bb: %llu\n",
            has_bb.vaddr, byte_offset_within_bb, src_addr, blocks_within_bb);
#endif
        memcpy((void *)dest, (const void *)src_addr, blocks_within_bb * block_size);

        // put back in cache
        CleanA35Cache(dest, blocks_within_bb * block_size, CacheType_t::Data, true);

        // write out
        auto wret = sd_perform_transfer(cur_bb * b_per_bb, b_per_bb, (void *)has_bb.paddr, false);
#if DEBUG_SDC
        klog("sdc: big block write complete, ret %d\n", wret);
#endif
        if(wret != 0)
            return wret;

        block_start += blocks_within_bb;
        block_count -= blocks_within_bb;
        src_addr += blocks_within_bb * block_size;
    }
    return 0;
}
