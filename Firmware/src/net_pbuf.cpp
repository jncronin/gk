#include <osnet.h>
#include <osmutex.h>
#include <stdlib.h>

#define LWIP_DATA __attribute__((section(".lwip_data")))

constexpr const unsigned int npbufs = 32;

LWIP_DATA static char pbufs[npbufs * PBUF_SIZE];

struct pbuf_data
{
    unsigned int id;
    bool is_allocated = false;
};

LWIP_DATA static pbuf_data pd[npbufs];
unsigned int next_pbuf = 0;

__attribute__((section(".sram4"))) Spinlock s_pbuf;

char *net_allocate_pbuf()
{
    CriticalGuard cg(s_pbuf);
    for(unsigned int i = 0; i < npbufs; i++)
    {
        auto act_id = (i + next_pbuf) % npbufs;
        if(pd[act_id].is_allocated == false)
        {
            pd[act_id].id = act_id;
            pd[act_id].is_allocated = true;
            next_pbuf++;
            return &pbufs[act_id * PBUF_SIZE];
        }
    }
    return nullptr;
}

void net_deallocate_pbuf(char *p)
{
    CriticalGuard cg(s_pbuf);
    unsigned int cgid = (p - pbufs) / PBUF_SIZE;
    pd[cgid].is_allocated = false;
}
