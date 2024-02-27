#include <osnet.h>
#include <osmutex.h>
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

int net_handle_ip4_packet(const char *buf, size_t n,
    const HwAddr &hw_src, const HwAddr &hw_dest,
    const NetInterface &iface)
{
    auto protocol = buf[9];
    auto version = buf[0];
    auto src = *reinterpret_cast<const uint32_t *>(&buf[12]);
    auto dest = *reinterpret_cast<const uint32_t *>(&buf[16]);

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "net: ip: version: %u, protocol: %u, src: %u, dest: %u\n",
            version, protocol, src, dest);
    }

    return 0;
}
