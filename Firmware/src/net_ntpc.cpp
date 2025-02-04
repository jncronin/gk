/* NTP client for GK */
#include "osnet.h"
#include "thread.h"
#include "scheduler.h"
#include "clocks.h"
#include <limits>

const unsigned long UNIX_NTP_OFFSET = 3124137599 - 915148799;

void *net_ntpc_thread(void *p)
{
    in_addr_t my_addr = 0;
    if(p)
    {
        my_addr = reinterpret_cast<in_addr_t>(p);
    }

    int lsck = socket(AF_INET, SOCK_DGRAM, 0);
    if(lsck < 0)
    {
        CriticalGuard cg;
        klog("ntpc: socket failed %d\n", errno);
        return nullptr;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = my_addr;
    saddr.sin_port = htons(123);

    int ret = bind(lsck, reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr));
    if(ret < 0)
    {
        klog("ntpc: bind failed %d\n", errno);
        return nullptr;
    }

    while(true)
    {
        /* Send to chronos.csr.net = 194.35.252.7 */
        IP4Addr ntp_server(194, 35, 252, 7);
        struct sockaddr_in ntp_dest;
        ntp_dest.sin_addr.s_addr = ntp_server.get();
        ntp_dest.sin_family = AF_INET;
        ntp_dest.sin_port = htons(123);

        /* Build request packet */
        uint32_t req[12];
        req[0] = (3UL << 0) | (3UL << 3) | (3UL << 6) |  // NTPv3, client, unknown leap indicator
            (0x11UL << 16) | // peer polling interval 131072 seconds
            (0xeeUL << 24);  // clock precision -18 (~1 us)
        req[1] = 0;
        req[2] = 0x100;     // root dispersion
        req[3] = 0;

        req[4] = 0;
        req[5] = 0;

        req[6] = 0;
        req[7] = 0;

        req[8] = 0;
        req[9] = 0;

        timespec ctime;
        clock_get_now(&ctime);

        req[10] = htonl(ctime.tv_sec + UNIX_NTP_OFFSET);
        req[11] = htonl((uint32_t)((((uint64_t)ctime.tv_nsec) * (uint64_t)std::numeric_limits<uint32_t>::max()) /
            1000000000ULL));

        auto sent = sendto(lsck, req, sizeof(req), 0, (sockaddr *)&ntp_dest, sizeof(ntp_dest));
        klog("ntpc: sent %d bytes\n", sent);

        /* Await response */
        sockaddr_in dfrom;
        socklen_t dfrom_len = sizeof(sockaddr_in);
        auto received = recvfrom(lsck, req, sizeof(req), 0, (sockaddr *)&dfrom, &dfrom_len);
        klog("ntpc: received %d bytes\n", received);

        BKPT();


        Block(clock_cur() + kernel_time::from_ms(10000));
    }
}
