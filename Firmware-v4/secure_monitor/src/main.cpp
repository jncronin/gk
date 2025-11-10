#include <cstdint>
#include "gkos_boot_interface.h"
#include "logger.h"

extern "C" void mp_kmain(const gkos_boot_interface *gbi, uint64_t magic)
{
    klog("SM: startup\n");
    while(true);
}
