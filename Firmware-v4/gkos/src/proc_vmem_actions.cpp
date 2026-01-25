#include "proc_vmem.h"
#include <cstring>
#include "vmem.h"
#include "syscalls_int.h"

static int action_zerofill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);
static int action_filefill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);
static int action_tlsfill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);
static int action_null(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);
static int action_filesync(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);
static int action_swapfill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);
static int action_swapsync(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb);

MemBlock MemBlock::ZeroBackedReadOnlyMemory(uintptr_t base,
            uintptr_t length,
            bool user, bool exec, unsigned int guard_type, unsigned int mt)
{
    length = (length + (VBLOCK_64k - 1)) & ~(VBLOCK_64k - 1);

    MemBlock ret;
    ret.b.base = base;
    ret.b.length = length;
    ret.b.user = user;
    ret.b.write = false;
    ret.b.exec = exec;
    ret.b.lower_guard = guard_type;
    ret.b.upper_guard = guard_type;
    ret.b.memory_type = mt;

    ret.FillFirst = action_zerofill;
    ret.FillSubsequent = action_null;
    ret.Sync = action_null;

    return ret;
}

MemBlock MemBlock::ZeroBackedReadWriteMemory(uintptr_t base,
            uintptr_t length,
            bool user, bool exec, unsigned int guard_type, unsigned int mt)
{
    length = (length + (VBLOCK_64k - 1)) & ~(VBLOCK_64k - 1);
    
    MemBlock ret;
    ret.b.base = base;
    ret.b.length = length;
    ret.b.user = user;
    ret.b.write = true;
    ret.b.exec = exec;
    ret.b.lower_guard = guard_type;
    ret.b.upper_guard = guard_type;
    ret.b.memory_type = mt;

    ret.FillFirst = action_zerofill;
    ret.FillSubsequent = action_swapfill;
    ret.Sync = action_swapsync;

    return ret;
}

MemBlock MemBlock::FileBackedReadWriteMemory(uintptr_t base,
            uintptr_t length,
            std::shared_ptr<File> &file,
            size_t file_offset,
            size_t file_len,
            bool user, bool exec, unsigned int guard_type, unsigned int mt)
{
    length = (length + (VBLOCK_64k - 1)) & ~(VBLOCK_64k - 1);
    
    MemBlock ret;
    ret.b.base = base;
    ret.b.length = length;
    ret.b.user = user;
    ret.b.write = true;
    ret.b.exec = exec;
    ret.b.lower_guard = guard_type;
    ret.b.upper_guard = guard_type;
    ret.b.memory_type = mt;

    ret.f = file;
    ret.foffset = file_offset;
    ret.flen = file_len;

    ret.FillFirst = action_filefill;
    ret.FillSubsequent = action_filefill;
    ret.Sync = action_filesync;

    return ret;
}

MemBlock MemBlock::FileBackedReadOnlyMemory(uintptr_t base,
            uintptr_t length,
            std::shared_ptr<File> &file,
            size_t file_offset,
            size_t file_len,
            bool user, bool exec, unsigned int guard_type, unsigned int mt)
{
    length = (length + (VBLOCK_64k - 1)) & ~(VBLOCK_64k - 1);
    
    MemBlock ret;
    ret.b.base = base;
    ret.b.length = length;
    ret.b.user = user;
    ret.b.write = false;
    ret.b.exec = exec;
    ret.b.lower_guard = guard_type;
    ret.b.upper_guard = guard_type;
    ret.b.memory_type = mt;

    ret.f = file;
    ret.foffset = file_offset;
    ret.flen = file_len;

    ret.FillFirst = action_filefill;
    ret.FillSubsequent = action_null;
    ret.Sync = action_null;

    return ret;
}

MemBlock MemBlock::TLSMemory(uintptr_t src_len,
            uintptr_t src_addr)
{
    auto length = (src_len + (VBLOCK_64k - 1)) & ~(VBLOCK_64k - 1);

    MemBlock ret;
    ret.b.base = 0;
    ret.b.length = length;
    ret.b.user = true;
    ret.b.write = true;
    ret.b.exec = false;
    ret.b.lower_guard = 0;
    ret.b.upper_guard = 0;
    ret.b.memory_type = MT_NORMAL;

    ret.foffset = src_addr;
    ret.flen = src_len;

    ret.FillFirst = action_tlsfill;
    ret.FillSubsequent = action_swapfill;
    ret.Sync = action_swapsync;

    return ret;
}

static int action_zerofill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    memset((void *)PMEM_TO_VMEM(page_paddr), 0, VBLOCK_64k);
    return 0;
}

static int action_filefill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    auto block_offset = page_vaddr - mb.b.data_start();
    auto file_offset = block_offset + mb.foffset;
    auto file_to_read = (mb.flen > block_offset) ? std::min(PAGE_SIZE, mb.flen - block_offset) : 0;
    
    if(file_to_read)
    {
        int cerrno;

        MutexGuard mg(mb.f->m);
        auto old_offset = mb.f->Lseek(0, SEEK_CUR, &cerrno);
        mb.f->Lseek(file_offset, SEEK_SET, &cerrno);
        auto fret = mb.f->Read((char *)PMEM_TO_VMEM(page_paddr), file_to_read, &cerrno);
        mb.f->Lseek(old_offset, SEEK_SET, &cerrno);
        if((fret < 0) || ((size_t)fret != file_to_read))
        {
            klog("filefill: read failed %d\n", fret);
            return -1;
        }
    }

    if(file_to_read != PAGE_SIZE)
    {
        auto zero_to_fill = PAGE_SIZE - file_to_read;
        memset((void *)PMEM_TO_VMEM(page_paddr + file_to_read), 0, zero_to_fill);
    }
    return 0;
}

static int action_tlsfill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    auto block_offset = page_vaddr - mb.b.data_start();
    auto toffset = block_offset;
    auto page_size = PAGE_SIZE;

    if(block_offset == 0)
    {
        // first 16 bytes are zero
        memset((void *)PMEM_TO_VMEM(page_paddr), 0, 16);
        page_size -= 16;
        page_paddr += 16;
    }
    else
    {
        toffset -= 16;
    }

    auto tls_to_read = (mb.flen > toffset) ? std::min(page_size, mb.flen - toffset) : 0;

    if(tls_to_read)
    {
        memcpy((void *)PMEM_TO_VMEM(page_paddr), (const void *)(mb.foffset + toffset), tls_to_read);
    }
    if(tls_to_read != page_size)
    {
        memset((void *)PMEM_TO_VMEM(page_paddr + tls_to_read), 0, page_size - tls_to_read);
    }

    return 0;
}

static int action_null(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    klog("action: null called\n");
    return -1;
}

static int action_filesync(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    klog("action: filesync not yet implemented\n");
    return -1;
}


static int action_swapfill(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    klog("action: swapfill not yet implemented\n");
    return -1;
}

static int action_swapsync(uintptr_t page_vaddr, uintptr_t page_paddr, MemBlock &mb)
{
    klog("action: swapsync not yet implemented\n");
    return -1;
}
