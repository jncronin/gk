#ifndef VMEM_H
#define VMEM_H

#include <cstdint>
#include <cstddef>

/* Call to initialise a page directory in DDR and set up EL1 paging structures */
void init_vmem();

/* Call to generate appropriate mappings for a block of memory */
void pmem_map_region(uint64_t base, uint64_t size, bool writeable, bool xn);

/* Call to return the physical address for a particular vaddress mapped by
    pmem_map_region().  If not already mapped will map it. */
uint64_t pmem_vaddr_to_paddr(uint64_t vaddr, bool writeable = false, bool xn = false);

/* Quick clear/copy, assumes multiples of 64 bytes, 16 byte alignment */
extern "C" void quick_clear_64(void *dest, size_t n = 65536);
extern "C" void quick_copy_64(void *dest, const void *src, size_t n = 65536);

/* Get used physical memory size */
uint64_t pmem_get_cur_brk();

/* Get a usable data pointer vmem address */
uint64_t pmem_paddr_to_vaddr(uint64_t paddr, int el);

#ifndef PMEM_OFFSET
#error PMEM_OFFSET not defined
#endif

#define PMEM_TO_VMEM(x) ((x) + PMEM_OFFSET)

#endif
