#include "logger.h"
#include "clocks.h"
#include "pmem.h"
#include "gkos_boot_interface.h"

extern "C" int mp_kmain(const gkos_boot_interface *gbi, uint64_t magic)
{
    init_clocks(gbi);
    klog("gkos: startup\n");

    uint64_t magic_str[2] = { magic, 0 };

    klog("gkos: magic: %s, ddr: %llx - %llx\n", (const char *)(&magic_str[0]), gbi->ddr_start, gbi->ddr_end);
    
    init_pmem(gbi->ddr_start, gbi->ddr_end);

    while(true)
    {
        udelay(1000000);
        klog("gkos: tick\n");
    }

    return 0;
}
