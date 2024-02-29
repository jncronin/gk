#include "osnet.h"
#include "thread.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

void net_dhcpd_thread(void *p)
{
    (void)p;

    int lsck = socket(AF_INET, SOCK_DGRAM, 0);
    if(lsck < 0)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: socket failed %d\n", errno);
        return;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = 0;
    saddr.sin_port = htons(67);

    int ret = bind(lsck, reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr));
    if(ret < 0)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "dhcpd: bind failed %d\n", errno);
        return;
    }

    while(true)
    {
        char buf[512];
        sockaddr_in caddr;
        socklen_t caddrlen = sizeof(caddr);
        ret = recvfrom(lsck, buf, 512, 0, reinterpret_cast<sockaddr *>(&caddr), &caddrlen);

        if(ret == -1)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "dhcpd: recvfrom failed %d\n", errno);
            return;
        }
        else
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "dhcpd: received %d bytes\n", ret);
        }
    }
}
