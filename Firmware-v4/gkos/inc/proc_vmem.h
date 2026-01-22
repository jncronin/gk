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

    The page fault handler therefore has a lot to do, and may be required to switch processes
    to handle file loads/stores.  We therefore need to re-enable interrupts in the page fault
    handler and use a mutex (rather than a spinlock) to protect the userspace memory structure
    within the Process structure.
    

    */









#endif
