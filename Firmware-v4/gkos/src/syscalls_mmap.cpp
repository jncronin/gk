#include "syscalls_int.h"
#include "process.h"
#include "osspinlock.h"

int syscall_mmapv4(size_t len, void **retaddr, int is_sync,
    int is_read, int is_write, int is_exec, int fd, int is_fixed, int *_errno)
{
    ADDR_CHECK_STRUCT_W(retaddr);

    if(fd >= 0)
    {
        klog("mmapv4: file mappings not yet supported\n");
        *_errno = ENOTSUP;
        return -1;
    }

    auto vbsize = vblock_size_for(len);
    if(vbsize == 0)
    {
        klog("mmapv4: request too large (%u)\n", len);
        *_errno = EINVAL;
        return -1;
    }

    auto p = GetCurrentProcessForCore();
    // try allocating a vblock
    CriticalGuard cg(p->user_mem->sl);

    uint32_t tag = VBLOCK_TAG_USER;
    if(fd >= 0)
        tag |= VBLOCK_TAG_FILE;
    else
        tag |= VBLOCK_TAG_CLEAR;
    if(is_exec)
        tag |= VBLOCK_TAG_EXEC;
    if(is_write)
        tag |= VBLOCK_TAG_WRITE;
    if(is_sync && fd < 0)
        tag |= VBLOCK_TAG_WT;

    VMemBlock vb = InvalidVMemBlock();
    if(*retaddr)
    {
        // try fixed alloc
        vb = p->user_mem->blocks.AllocFixed(vbsize, (uintptr_t)*retaddr, tag);

        if(!vb.valid)
        {
            klog("mmap: fixed %x @ %p failed.  Current allocs:\n", vbsize, *retaddr);
            p->user_mem->blocks.Dump();
        }
    }
    if(!vb.valid)
    {
        if(is_fixed)
        {
            // fail if we don't deliver the exact block if MAP_FIXED specified
            // also fail if MAP_FIXED and *retaddr = 0 - cannot allocate 0
            *_errno = EEXIST;
            return -1;
        }
        // otherwise, try and allocate anywhere
        vb = p->user_mem->blocks.Alloc(vbsize, tag);
    }

    if(vb.valid)
    {
        *retaddr = (void *)vb.data_start();
        return 0;
    }
    else
    {
        *_errno = ENOMEM;
        return -1;
    }
}
