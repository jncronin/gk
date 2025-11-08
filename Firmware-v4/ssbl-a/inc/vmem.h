#ifndef VMEM_H
#define VMEM_H

#include <cstdint>

/* Call to initialise a page directory in DDR and set up EL1 paging structures */
void init_vmem();

/* Call to generate appropriate mappings for a block of memory */
void pmem_map_region(uint64_t base, uint64_t size, bool writeable, bool xn);

/* Call to return the physical address for a particular vaddress mapped by
    pmem_map_region().  If not already mapped will map it. */
uint64_t pmem_vaddr_to_paddr(uint64_t vaddr, bool writeable = false, bool xn = false);

#endif
