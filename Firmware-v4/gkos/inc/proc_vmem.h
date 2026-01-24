#ifndef PROC_VMEM_H
#define PROC_VMEM_H

/* This is a rewrite of the VBlock code for use initially in userspace but ideally
    expand to kernel afterwards.
    
    Each process owns a collection of regions handled through a region allocator
    These refer to things like heap, stacks, text, bss, tls, mmap regions etc
    
    The allocator interface needs to allow:
    
        Allocation of a fixed size region at a particular address (or fail if occupied)
        Split an already allocated region into two or three (to allow mprotect on part of a region)
        Allocation of a fixed size region at any address, either lowest first, highest first or anywhere
        For a given address, return whether it is within an allocated region or not
        In-order traversal for dump()ing purposes (less critical - can be sorted later as dump() unlikely
         to be called often.

        For the initial implementation use a std::list.  We can optimise with a tree later.

    Each region then needs a backing store which can be changed on the fly (the region has a
        unique_ptr or similar to the backing store object)

    The backing store can be one of:
        Zero alloc (i.e. heap/stack/bss).
        File/offset (can use for, e.g. text/data/tls segment (read from the appropriate part of
         executable), mmap region or swap file region))

    The backing store needs to handle:
        First-time reads/writes of a page within a region
         - either load with zero or part of a file 
        Subsequent reads/writes of a page that may have been swapped out (swapping TODO)

        Thus, a zero backed region (e.g. heap) may have some pages that read as 0, and
         some that read as part of swap space.  We need to handle this somehow.

         The solution is based on the fact that only writeable pages need to be swapped out
         (the others just reload from zero or the backing file).

         Therefore, for writeable pages we initially map them as read only, and fill with either
         zero or part of a file.  On first write we then flip the writeable bit on but make
         no changes to the underlying file.

         When we come to swap out a page (or indeed msync() a page back to the backing file),
         we only need to write those pages within the region that have the writeable bit set.

         Swapped out pages (thay will need to be reloaded on access) are then set as
         writeable bit set, present bit not set.


         Therefore 3 functions are required: FillFirst, FillSubsequent and Sync:
            ZeroBackedReadOnlyMemory: (unlikely to be used much)
                FillFirst = FillZero
                FillSubsequent = Null
                Sync = Null
            ZeroBackedReadWriteMemory: (heap/stack/bss, mmap anon regions)
                FillFirst = FillZero
                FillSubsequent = ReadSwap
                Sync = WriteSwap
            FileBackedReadOnlyMemory: (text, rodata, mmap ro regions)
                FillFirst = FileRead
                FillSubsequent = Null (won't be called because write bit will never be set)
                Sync = Null
            FileBackedReadWriteMemory (data, mmap rw regions)
                FillFist = FileRead
                FillSubsequent = ReadSwap
                Sync = WriteSwap


    The page fault handler therefore has a lot to do, and may be required to switch processes
    to handle file loads/stores.  We therefore need to re-enable interrupts in the page fault
    handler and use a mutex (rather than a spinlock) to protect the userspace memory structure
    within the Process structure.
    

    */

#include <cstdint>
#include <memory>
#include <list>
#include <map>
#include "ostypes.h"

#if __GK_UNIT_TEST__
#include "unit_test.h"
#else
#include "osmutex.h"
#endif

#define VBLOCK_64k      65536ULL

/* Define a block of memory */
class MemBlock
{
    public:
        VMemBlock b;

        virtual int FillFirst(uintptr_t offset) = 0;
        virtual int FillSubsequent(uintptr_t offset) = 0;
        virtual int Sync(uintptr_t offset) = 0;

        virtual ~MemBlock() = default;
};

class ZeroBackedReadOnlyMemory : public MemBlock
{
    public:
        int FillFirst(uintptr_t offset) = 0;
        int FillSubsequent(uintptr_t offset) = 0;
        int Sync(uintptr_t offset) = 0;
};

class ZeroBackedReadWriteMemory : public MemBlock
{
    public:
        SwapFileIndex si{};

        int FillFirst(uintptr_t offset) = 0;
        int FillSubsequent(uintptr_t offset) = 0;
        int Sync(uintptr_t offset) = 0;
};

class FileBackedReadOnlyMemory : public MemBlock
{
    public:
        id_t fd = 0;

        int FillFirst(uintptr_t offset) = 0;
        int FillSubsequent(uintptr_t offset) = 0;
        int Sync(uintptr_t offset) = 0;
};

class FileBackedReadWriteMemory : public MemBlock
{
    public:
        id_t fd = 0;
        SwapFileIndex si{};
        
        int FillFirst(uintptr_t offset) = 0;
        int FillSubsequent(uintptr_t offset) = 0;
        int Sync(uintptr_t offset) = 0;
};

/* Define the allocator interface

        Allocation of a fixed size region at a particular address (or fail if occupied)
        Split an already allocated region into two or three (to allow mprotect on part of a region)
        Allocation of a fixed size region at any address, either lowest first, highest first or anywhere
        For a given address, return whether it is within an allocated region or not
        In-order traversal for dump()ing purposes.
        Deletion of allocation
*/
class VBlockAllocator
{
    public:
        uintptr_t base = VBLOCK_64k;        // catch null pointer references
        uintptr_t length = LH_END - base;
        
        typedef int (*traversal_function_t)(std::unique_ptr<MemBlock> &mb);

        virtual std::pair<bool, uintptr_t> AllocFixed(std::unique_ptr<MemBlock> &&region) = 0;
        virtual std::unique_ptr<MemBlock> &Split(uintptr_t address) = 0;
        virtual std::pair<bool, uintptr_t> AllocAny(std::unique_ptr<MemBlock> &&region, bool lowest_first = true) = 0;
        virtual std::unique_ptr<MemBlock> &IsAllocated(uintptr_t address) = 0;
        virtual int Traverse(traversal_function_t tf) = 0;
        virtual int Dealloc(std::unique_ptr<MemBlock> &region) = 0;
};

class MapVBlockAllocator : public VBlockAllocator
{
    protected:
        std::map<uintptr_t, std::unique_ptr<MemBlock>> l;
        Mutex m;

        std::tuple<bool, uintptr_t, uintptr_t> fits(uintptr_t len,
            std::unique_ptr<MemBlock> *prev,
            std::unique_ptr<MemBlock> *next);

    public:
        std::pair<bool, uintptr_t> AllocFixed(std::unique_ptr<MemBlock> &&region);
        std::unique_ptr<MemBlock> &Split(uintptr_t address);
        std::pair<bool, uintptr_t> AllocAny(std::unique_ptr<MemBlock> &&region, bool lowest_first = true);
        std::unique_ptr<MemBlock> &IsAllocated(uintptr_t address);
        int Traverse(traversal_function_t tf);
        int Dealloc(std::unique_ptr<MemBlock> &region);
};

#endif
