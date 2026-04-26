#include "linux_types.h"
#include <unordered_map>
#include "osfile.h"
#include "vmem.h"
#include "proc_vmem.h"
#include "process.h"
#include "drifile.h"
#include <atomic>
#include "screen.h"
#include "pmem.h"

/* mmap offsets are global and weak, handles are per-file and strong */
static Spinlock sl_mmap_offsets;
static u32 next_mmap_offset = 1;
static std::unordered_map<u32, std::weak_ptr<drm_gem_object>> mmap_offsets;

extern std::atomic<int> fb_dma_addr_next;

int drm_gem_object_init(struct drm_device *dev, std::shared_ptr<drm_gem_object> drm, size_t size)
{
    auto fb_idx = fb_dma_addr_next.exchange(-1);
    auto p = GetCurrentProcessForCore();
    if(!p)
    {
        return -1;
    }
    if(fb_idx >= 0 && fb_idx < 3)
    {
        CriticalGuard cg(p->screen.sl);
        auto layer = p->screen.screen_layer;
        cg.unlock();

        auto [ _ , scr_paddr ] = screen_get_layer_vaddr_paddr(layer, fb_idx);
        drm->vaddr = 0;
        drm->dma_addr = scr_paddr;
        drm->psize = ALIGN(size, PAGE_SIZE);
        drm->vsize = 0;
        drm->release_pages = false; // don't free the framebuffer
    }
    else
    {
        drm->vaddr = 0;
        drm->vsize = 0;
        
        auto pb = Pmem.acquire(size);
        if(!pb.valid)
        {
            return -1;
        }

        drm->dma_addr = pb.base;
        drm->psize = pb.length;
        drm->creating_proc = GetCurrentPProcessForCore();
        drm->release_pages = true;
        
        {
            CriticalGuard cg(p->owned_pages.sl);
            p->owned_pages.add(pb);
        }
    }
    drm->handle = 0;
    drm->dev = dev;
    drm->pid = p->id;

    return 0;
}

int drm_gem_handle_create(std::shared_ptr<drm_file> &f, std::shared_ptr<drm_gem_object> obj, u32 *phandle)
{
    CriticalGuard cg(f->sl_handles);
    obj->handle = f->next_handle++;
    *phandle = obj->handle;
    f->handles[obj->handle] = std::move(obj);
    return 0;
}

std::shared_ptr<drm_gem_object> drm_gem_object_lookup(std::shared_ptr<drm_file> &f, u32 handle)
{
    CriticalGuard cg(f->sl_handles);
    auto iter = f->handles.find(handle);
    if(iter == f->handles.end())
        return nullptr;
    return iter->second;
}

drm_gem_object::~drm_gem_object()
{
    if(release_pages)
    {
        PMemBlock pb;
        pb.base = dma_addr;
        pb.length = psize;
        pb.is_shared = false;
        pb.valid = true;

        Pmem.release(pb);

        auto p = creating_proc.lock();
        if(p)
        {
            pb.valid = true;
            CriticalGuard cg(p->owned_pages.sl);
            p->owned_pages.release(pb);
        }
    }

#if GPU_DEBUG > 3
    klog("drm_gem_object: destructor\n");
#endif
}

int drm_gem_object_close(std::shared_ptr<drm_file> &f, u32 handle)
{
    CriticalGuard cg(f->sl_handles);
    auto iter = f->handles.find(handle);
    if(iter == f->handles.end())
    {
        klog("drm_gem_close: handle %u not found\n", handle);
        return -1;
    }
    f->handles.erase(iter);
    return 0;
}

int drm_gem_create_mmap_offset(std::shared_ptr<drm_gem_object> obj)
{
    CriticalGuard cg(sl_mmap_offsets);
    auto offset = next_mmap_offset++;
    mmap_offsets[offset] = obj;
    obj->vma_node = (u64)offset << 32;
    return 0;
}

__u64 drm_vma_node_offset_addr(drm_vma_offset_node *node)
{
    return *node;
}

/* Handle the DRI side of mmap calls.
    Offset is actually a handle created by drm_gem_create_mmap_offset.  We left shift
    the 32-bit handle by 32 so that is will always remain PAGE_SIZE aligned.  Also
    test for the possibility that userland has set the lower bits - not sure what to
    do here so just bail for now.
*/
int dri_mmap(size_t len, void **retaddr, int is_sync,
    int is_read, int is_write, int is_exec, DRIFile &fd, int is_fixed, size_t offset, int *_errno)
{
    std::shared_ptr<drm_gem_object> obj;

    {
        CriticalGuard cg(sl_mmap_offsets);
        u32 handle = (u32)(offset >> 32);
        auto iter = mmap_offsets.find(handle);
        if(iter == mmap_offsets.end())
        {
            klog("dri: invalid mmap offset: %llx\n", offset);
            return -1;
        }
        obj = iter->second.lock();
    }

    // Do some sanity checks
    if(!obj)
    {
        return -1;
    }
    MutexGuard mg(*obj->lock);
    if(is_exec)
    {
        klog("dri: mmap attempt for exec memory\n");
        return -1;
    }
    if(offset & 0xffffffffULL)
    {
        klog("dri: mmap with extra offset: %llx\n", offset);
        return -1;
    }
    if(len > obj->psize)
    {
        klog("dri: mmap length too long for underlying object: %llx vs %llx\n",
            len, obj->psize);
        return -1;
    }

    // Replicate the mmap code somewhat to get the vaddr
    auto p = GetCurrentProcessForCore();
    auto pmb = MemBlock::ZeroBackedReadOnlyMemory(0, len, true, false); // for any extra
    pmb.drm_obj = obj;
    pmb.pmem_is_drm_object = true;

    VMemBlock vb = InvalidVMemBlock();
    if(*retaddr)
    {
        // try fixed alloc
        vb = p->user_mem->vblocks.AllocFixed(pmb);
    }
    if(!vb.valid)
    {
        if(is_fixed)
        {
            klog("mmap: fixed %x @ %p failed.  Current allocs:\n", len, *retaddr);
            p->user_mem->vblocks.Traverse([](MemBlock &mb) { klog("mmap: %p - %p\n", mb.b.base, mb.b.end()); return 0; });

            // fail if we don't deliver the exact block if MAP_FIXED specified
            // also fail if MAP_FIXED and *retaddr = 0 - cannot allocate 0
            *_errno = EEXIST;
            return -1;
        }
        // otherwise, try and allocate anywhere
        vb = p->user_mem->vblocks.AllocAny(pmb, false);
    }

    if(vb.valid)
    {
        *retaddr = (void *)vb.data_start();
    }

    /* If we've reached this far then this is a new mapping for this object
        Add it to sg_table so it gets cache operations done appropriately */
    auto map_len = PAGE_ALIGN(std::min(len, obj->psize));
    sg_entry sge { .vaddr = (void *)vb.data_start(), .paddr = obj->dma_addr, .len = map_len, .mt = obj->mt };
    obj->sgt.push_back(sge);

    // Now map all the physical bits we have
    for(auto i = 0ull; i < map_len; i += PAGE_SIZE)
    {
        vmem_map(vb.data_start() + i, obj->dma_addr + i, true, is_write != 0, false, p->user_mem->ttbr0, ~0ULL,
            nullptr, obj->mt);
    }

    return 0;
}
