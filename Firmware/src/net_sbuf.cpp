#include <osnet.h>
#include <osmutex.h>
#include <stdlib.h>

constexpr const unsigned int nsbufs = 32;

NET_BSS static char sbufs[nsbufs * GK_NET_SOCKET_BUFSIZE];

struct sbuf_data
{
    unsigned int id;
    bool is_allocated = false;
};

NET_BSS static sbuf_data sd[nsbufs];
SRAM4_DATA unsigned int next_sbuf = 0;

__attribute__((section(".sram4"))) Spinlock s_sbuf;

char *net_allocate_sbuf()
{
    CriticalGuard cg(s_sbuf);
    for(unsigned int i = 0; i < nsbufs; i++)
    {
        auto act_id = (i + next_sbuf) % nsbufs;
        if(sd[act_id].is_allocated == false)
        {
            sd[act_id].id = act_id;
            sd[act_id].is_allocated = true;
            next_sbuf++;
            return &sbufs[act_id * GK_NET_SOCKET_BUFSIZE];
        }
    }
    return nullptr;
}

void net_deallocate_sbuf(char *p)
{
    CriticalGuard cg(s_sbuf);
    unsigned int cgid = (p - sbufs) / GK_NET_SOCKET_BUFSIZE;
    sd[cgid].is_allocated = false;
}
