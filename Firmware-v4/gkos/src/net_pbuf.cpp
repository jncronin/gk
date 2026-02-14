#include <osnet.h>
#include <osmutex.h>
#include <stdlib.h>

struct pbuf_data
{
    unsigned int id;
    bool is_allocated = false;
};

template <unsigned int bsize, unsigned int nbufs> class PBufAllocator
{
    protected:
        char *pbufs;
        pbuf_data *pd;
        unsigned int next_pbuf = 0;
        Spinlock sl;
        unsigned int nalloc = 0;

    public:
        PBufAllocator(char *_pbufs, pbuf_data *_pd)
        {
            pbufs = _pbufs;
            pd = _pd;
            for(unsigned int i = 0; i < nbufs; i++)
            {
                new (&pd[i]) pbuf_data();
            }
        }

        char *alloc()
        {
            CriticalGuard cg(sl);
            if(nalloc == nbufs)
                return nullptr;
            
            for(unsigned int i = 0; i < nbufs; i++)
            {
                auto act_id = (i + next_pbuf) % nbufs;
                if(pd[act_id].is_allocated == false)
                {
                    pd[act_id].id = act_id;
                    pd[act_id].is_allocated = true;
                    next_pbuf++;
                    nalloc++;
                    return &pbufs[act_id * bsize];
                }
            }
            return nullptr;
        }

        void free(char *p)
        {
            if(p < pbufs) return;
            unsigned int cgid = (p - pbufs) / bsize;
            if(cgid < nbufs)
            {
                CriticalGuard cg(sl);
                pd[cgid].is_allocated = false;
                nalloc--;
            }
        }

        size_t nfree()
        {
            CriticalGuard cg(sl);
            return nbufs - nalloc;
        }
};

constexpr const unsigned int npbufs = 32;
constexpr const unsigned int nspbufs = 256;

NET_BSS __attribute__((aligned(32))) static char pbufs[npbufs * PBUF_SIZE];
NET_BSS __attribute__((aligned(32))) static char spbufs[nspbufs * SPBUF_SIZE];

NET_BSS static pbuf_data pd[npbufs];
NET_BSS static pbuf_data spd[nspbufs];
SRAM4_DATA static PBufAllocator<PBUF_SIZE, npbufs> ba(pbufs, pd);
SRAM4_DATA static PBufAllocator<SPBUF_SIZE, nspbufs> sba(spbufs, spd);

char *net_allocate_pbuf(size_t n)
{
    if(n <= SPBUF_SIZE)
    {
        auto ret = sba.alloc();
        if(ret) return ret;
    }
    if(n <= PBUF_SIZE)
    {
        auto ret = ba.alloc();
        if(ret) return ret;
    }
    return nullptr;
}

void net_deallocate_pbuf(char *p)
{
    ba.free(p);
    sba.free(p);
}

size_t net_pbuf_nfree()
{
    return ba.nfree();
}
