#include "syscalls_int.h"
#include "process.h"
#include "osspinlock.h"
#include "vmem.h"

int syscall_mmapv4(size_t len, void **retaddr, int is_sync,
    int is_read, int is_write, int is_exec, int fd, int is_fixed, size_t foffset, int *_errno)
{
    ADDR_CHECK_STRUCT_W(retaddr);

    len = (len + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
    if((uintptr_t)*retaddr & (PAGE_SIZE - 1))
    {
        *_errno = EINVAL;
        return -1;
    }
    if((fd >= 0) && (foffset & (PAGE_SIZE - 1)))
    {
        *_errno = EINVAL;
        return -1;
    }

    auto p = GetCurrentProcessForCore();
    // try allocating a vblock
    if(!p || !p->user_mem)
    {
        *_errno = EFAULT;
        return -1;
    }
    MutexGuard mg(p->user_mem->m);
    MemBlock pmb;

    if(fd >= 0)
    {
        CriticalGuard cgf(p->open_files.sl);
        if(p->open_files.f.size() <= (size_t)fd)
        {
            *_errno = EBADF;
            return -1;
        }
        if(foffset & (VBLOCK_64k - 1))
        {
            *_errno = EINVAL;
            return -1;
        }

        if(is_write)
        {
            pmb = MemBlock::FileBackedReadWriteMemory((uintptr_t)*retaddr, len, p->open_files.f[fd], foffset,
                ~0, true, is_exec != 0);
        }
        else
        {
            pmb = MemBlock::FileBackedReadOnlyMemory((uintptr_t)*retaddr, len, p->open_files.f[fd], foffset,
                ~0, true, is_exec != 0);
        }
    }
    else
    {
        if(is_write)
        {
            pmb = MemBlock::ZeroBackedReadWriteMemory((uintptr_t)*retaddr, len, true, is_exec != 0, 0,
                is_sync ? MT_NORMAL_WT : MT_NORMAL);
        }
        else
        {
            pmb = MemBlock::ZeroBackedReadOnlyMemory((uintptr_t)*retaddr, len, true, is_exec != 0, 0,
                is_sync ? MT_NORMAL_WT : MT_NORMAL);
        }
    }

    VMemBlock vb = InvalidVMemBlock();
    if(*retaddr)
    {
        // try fixed alloc
        vb = p->user_mem->vblocks.AllocFixed(pmb);

        if(!vb.valid)
        {
            klog("mmap: fixed %x @ %p failed.  Current allocs:\n", len, *retaddr);
            p->user_mem->vblocks.Traverse([](MemBlock &mb) { klog("mmap: %p - %p\n", mb.b.base, mb.b.end()); return 0; });
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
        vb = p->user_mem->vblocks.AllocAny(pmb, false);
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

int syscall_setprot(const void *addr, int is_read, int is_write, int is_exec, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    if(!p || !p->user_mem)
    {
        *_errno = EFAULT;
        return -1;
    }

    // try allocating a vblock
    MutexGuard mg(p->user_mem->m);

    auto &reg = p->user_mem->vblocks.IsAllocated((uintptr_t)addr);
    if(reg.b.valid)
    {
        reg.b.write = is_write != 0;
        reg.b.exec = is_exec != 0;
        return 0;
    }
    else
    {
        *_errno = ENOMEM;
        return -1;
    }
}
