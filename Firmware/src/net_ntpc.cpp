/* NTP client for GK */
#include "osnet.h"
#include "thread.h"
#include "scheduler.h"
#include "clocks.h"
#include <limits>

const unsigned long UNIX_NTP_OFFSET = 3124137599 - 915148799;

static void ntpc_thread_cleanup(void *p)
{
    klog("ntpc: cleanup\n");
    if(p)
    {
        close((int)p);
    }
}

void *net_ntpc_thread(void *_p)
{
    auto p = reinterpret_cast<struct net_ntpc_thread_params *>(_p);

    in_addr_t my_addr = 0;
    if(p && p->myaddr.get())
    {
        my_addr = reinterpret_cast<in_addr_t>(p->myaddr.get());
    }

    int lsck = (p && p->sockfd && *p->sockfd >= 0) ? *p->sockfd : socket(AF_INET, SOCK_DGRAM, 0);
    if(lsck < 0)
    {
        CriticalGuard cg;
        klog("ntpc: socket failed %d\n", errno);
        return nullptr;
    }
    if(p && p->sockfd) *p->sockfd = lsck;
    if(p) delete p;
    int syscall_pthread_cleanup_push(void (*)(void *), void *, int *);
    int _errno;
    syscall_pthread_cleanup_push(ntpc_thread_cleanup, (void *)lsck, &_errno);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = my_addr;
    saddr.sin_port = htons(123);

    int ret = bind(lsck, reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr));
    if(ret < 0)
    {
        klog("ntpc: bind failed %d\n", errno);
        close(lsck);
        return nullptr;
    }

    kernel_time last_ntp_update;

    while(true)
    {
        if(!last_ntp_update.is_valid() || clock_cur() >= last_ntp_update + kernel_time::from_ms(1000 * 60 * 60))
        {
            // update time every hour
            /* Send to time.windows.com = 51.145.123.29 */
            IP4Addr ntp_server(51, 145, 123, 29);
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

            if(sent >= (ssize_t)sizeof(req))
            {
                /* Await response */
                sockaddr_in dfrom;
                socklen_t dfrom_len = sizeof(sockaddr_in);
                auto received = recvfrom(lsck, req, sizeof(req), 0, (sockaddr *)&dfrom, &dfrom_len,
                    clock_cur() + kernel_time::from_ms(2000));
                if(received > 0)
                {
                    klog("ntpc: received %d bytes\n", received);
                    for(int i = 0; i < 12; i++)
                    {
                        klog("ntpc: %02d: %08x\n", i, req[i]);
                    }

                    if(received >= (ssize_t)sizeof(req))
                    {
                        timespec ttstamp;
                        ttstamp.tv_sec = ntohl(req[10]) - UNIX_NTP_OFFSET;
                        ttstamp.tv_nsec = (uint32_t)(((uint64_t)ntohl(req[11]) * 1000000000ULL) /
                            (uint64_t)std::numeric_limits<uint32_t>::max());
                        char buf[64];
                        auto t = localtime(&ttstamp.tv_sec);
                        strftime(buf, 63, "%F %T", t);
                        klog("ntp: current time: %s\n", buf);

                        // set our time to this - TODO ideally use round trip time etc, but we only want a couple
                        //  of seconds accuracy for gk
                        auto tdiff = ttstamp - ctime;
                        timespec toffset;
                        clock_get_timebase(&toffset);
                        toffset = toffset + tdiff;
                        clock_set_timebase(&toffset);
                        clock_set_rtc_from_timespec(&ttstamp);

                        last_ntp_update = clock_cur();
                    }
                }
            }
        }

        // try again in a bit
        Block(clock_cur() + kernel_time::from_ms(1000));

        // report deviations between system clock and rtc
        timespec sysclk, rtc;
        clock_get_now(&sysclk);
        clock_get_timespec_from_rtc(&rtc);

        {
            char buf1[64], buf2[64];
            auto t1 = localtime(&sysclk.tv_sec);
            strftime(buf1, 63, "%F %T", t1);
            auto t2 = localtime(&rtc.tv_sec);
            strftime(buf2, 63, "%F %T", t2);

            auto diff = sysclk - rtc;
            klog("ntpc: sysclk: %s.%09d, rtc: %s.%09d, diff: %d.%09d\n",
                buf1, sysclk.tv_nsec,
                buf2, rtc.tv_nsec,
                (long)diff.tv_sec, diff.tv_nsec);
        }
    }
}
