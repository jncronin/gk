#include "osnet.h"
#include "SEGGER_RTT.h"
#include "unistd.h"

extern Spinlock s_rtt;

void net_telnet_thread(void *p)
{
    (void)p;

    int lsck = socket(AF_INET, SOCK_STREAM, 0);
    if(lsck < 0)
    {
        CriticalGuard cg(s_rtt);
        klog("telnetd: socket failed\n");
        return;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = 0;
    saddr.sin_port = htons(23);

    int ret = bind(lsck, (sockaddr *)&saddr, sizeof(saddr));
    if(ret < 0)
    {
        CriticalGuard cg(s_rtt);
        klog("telnetd: bind failed %d\n", errno);
        return;
    }

    ret = listen(lsck, 0);
    if(ret < 0)
    {
        CriticalGuard cg(s_rtt);
        klog("telnetd: listen failed %d\n", errno);
        return;
    }
    while(true)
    {
        sockaddr_in caddr;
        socklen_t caddrlen = sizeof(caddr);
        ret = accept(lsck, (sockaddr *)&caddr, &caddrlen);
        if(ret < 0)
        {
            CriticalGuard cg(s_rtt);
            klog("telnetd: accept failed %d\n", errno);
            return;
        }
        else
        {
            CriticalGuard cg(s_rtt);
            klog("telnetd: accept succeeded %d\n", ret);
        }

        while(true)
        {
            char buf[32];
            auto br = recv(ret, buf, 31, 0);
            if(br > 0)
            {
                buf[br] = 0;
                {
                    CriticalGuard cg(s_rtt);
                    klog("telnetd: received %d bytes: %s\n", br, buf);
                }

                // echo
                send(ret, buf, br, 0);
            }
            else if(br < 0)
            {
                CriticalGuard cg(s_rtt);
                klog("telnetd: recv failed %d\n", errno);
                break;
            }
            else
            {
                CriticalGuard cg(s_rtt);
                klog("telnetd: socket closed by peer\n");
                // graceful close
                close(ret);
                break;
            }
        }
    }
}