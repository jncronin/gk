#ifndef SG_TABLE_H
#define SG_TABLE_H

#include <cstdint>
#include <vector>

/* SG table is a list of memory blocks mapping virtual addresses to "dma" (physical) addresses.
    The linux versions allow the dma addresses to be merged if adjacent so there is a different
    'n' count for virtual and physical, and the table can be iterated in different ways.

    We don't support this, and instead provide a single mapping for each element of the
    table.
*/

struct sg_entry
{
    void *vaddr;
    uintptr_t paddr;
    size_t len;
    unsigned int mt;
};

using sg_table = std::vector<sg_entry>;

#endif
