#include <cstdint>
#include "gkos_boot_interface.h"
#include "logger.h"
#include "osspinlock.h"
#include "vmem.h"
#include "vblock.h"
#include "sm_clocks.h"
#include "clocks.h"

#define STACK_SIZE  65536

uint64_t mp_stack_vaddr;
uint64_t ap_stack_vaddr;

uint64_t ddr_start;
uint64_t ddr_end;

uint64_t vaddr_ptr;

static inline uint64_t align(uint64_t v)
{
    return (v + 65535ULL) & ~65535ULL;
}

extern "C" uint64_t mp_preinit(const gkos_boot_interface *gbi)
{
    /* This is called prior to __libc_init_array so cannot rely on global objects being inited */

    // Get end of virtual space
    extern int _ebss;
    vaddr_ptr = (uint64_t)&_ebss;
    vaddr_ptr = align(vaddr_ptr);

    // Get stack space for mp and ap with guard pages before, between, after
    auto stack_alloc = vmem_alloc(3 * 65536 + STACK_SIZE * 2);
    mp_stack_vaddr = stack_alloc + 65536;
    ap_stack_vaddr = stack_alloc + 65536 * 2 + STACK_SIZE;

    // pointers for the pmem alloc
    ddr_start = gbi->ddr_start;
    ddr_end = gbi->ddr_end;

    pmem_map_region(mp_stack_vaddr, STACK_SIZE, true, true);
    pmem_map_region(ap_stack_vaddr, STACK_SIZE, true, true);

    return mp_stack_vaddr + STACK_SIZE;
}

extern "C" void mp_kmain(const gkos_boot_interface *gbi, uint64_t magic)
{
    klog("SM: startup\n");

    clock_takeover();

    while(true)
    {
        klog("tick\n");
        udelay(1000000);
    }
}

