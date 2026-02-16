#include <osnet.h>
#include <osmutex.h>
#include <stdlib.h>
#include "osspinlock.h"

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

NET_BSS __attribute__((aligned(64))) static char pbufs[npbufs * PBUF_SIZE];
NET_BSS __attribute__((aligned(64))) static char spbufs[nspbufs * SPBUF_SIZE];

NET_BSS static pbuf_data pd[npbufs];
NET_BSS static pbuf_data spd[nspbufs];
SRAM4_DATA static PBufAllocator<PBUF_SIZE, npbufs> ba(pbufs, pd);
SRAM4_DATA static PBufAllocator<SPBUF_SIZE, nspbufs> sba(spbufs, spd);

pbuf_t net_allocate_pbuf(size_t n, size_t offset)
{
    auto act_size = n + sizeof(PBuf) + offset + NET_SIZE_ETHERNET_FOOTER;
    if(act_size <= SPBUF_SIZE)
    {
        auto ret = sba.alloc();
        if(ret)
        {
            auto b = (pbuf_t)ret;
            *b = PBuf();
            b->off = offset + sizeof(PBuf);
            b->len = n;
            b->total_len = SPBUF_SIZE - sizeof(PBuf);
            return b;
        }
    }
    if(n <= PBUF_SIZE)
    {
        auto ret = ba.alloc();
        if(ret)
        {
            auto b = (pbuf_t)ret;
            *b = PBuf();
            b->off = offset + sizeof(PBuf);
            b->len = n;
            b->total_len = PBUF_SIZE - sizeof(PBuf);
            return b;
        }
    }
    return nullptr;
}

void net_deallocate_pbuf(pbuf_t p)
{
    {
        CriticalGuard cg(p->sl);
        if(p->nrefs > 0) p->nrefs--;
        if(p->nrefs > 0)
            return;
    }
    ba.free((char *)p);
    sba.free((char *)p);
}

size_t net_pbuf_nfree()
{
    return ba.nfree();
}

int PBuf::AddReference()
{
    CriticalGuard cg(sl);
    nrefs++;
    return (int)nrefs;
}

size_t PBuf::GetSize()
{
    CriticalGuard cg(sl);
    return len;
}

void *PBuf::Ptr()
{
    CriticalGuard cg(sl);
    return (void *)((uint8_t *)this + off);
}

char *PBuf::Ptr(size_t offset)
{
    return (char *)Ptr() + offset;
}

int PBuf::AdjustStart(ssize_t amount)
{
    CriticalGuard cg(sl);
    if(amount > 0)
    {
        // decrease the space at front i.e. move offset forward
        auto decr_amount = (size_t)amount;
        if(decr_amount > len)
        {
            decr_amount = len;
        }
        off += decr_amount;
        len -= decr_amount;

        return 0;
    }
    else
    {
        auto incr_amount = (size_t)(-amount);
        // increase the space at front, i.e. move offset backwards, increase len
        if(incr_amount > (off - sizeof(PBuf)))
        {
            klog("net: pbuf: adjust_start: add_remove_amount (-%u) too negative\n",
                incr_amount);
            return -1;
        }
        else
        {
            off -= incr_amount;
            len += incr_amount;
            //klog("net: buffer %p: new_front (off: %lu, len: %lu, tot_size: %lu)\n",
            //    cb, cb->off, cb->len, cb->tot_size);

            return 0;
        }
    }
}

int PBuf::SetSize(size_t new_size)
{
    CriticalGuard cg(sl);

    if((off + new_size) > total_len)
    {
        // cannot allocate here.  TODO: reallocate and copy
        klog("net: pbuf: SetSize failed for new size %u\n", new_size);
        return -1;
    }
    else
    {
        len = new_size;
        return 0;
    }
}
