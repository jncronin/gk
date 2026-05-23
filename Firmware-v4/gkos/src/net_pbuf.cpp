#include <osnet.h>
#include <osmutex.h>
#include <stdlib.h>
#include "osspinlock.h"
#include "pmem.h"
#include "vmem.h"
#include "vblock.h"
#include "linux_types.h"
#include "logger.h"

template <unsigned int bsize, unsigned int nbufs> class PBufAllocator
{
    protected:
        Spinlock sl;
        DECLARE_BITMAP(bmp, nbufs);
        PMemBlock pmb;
        VMemBlock vmb;
        size_t nalloc = 0;

    public:
        void init()
        {
            pmb = Pmem.acquire(bsize * nbufs);
            vmb = vblock.Alloc(pmb.length);
            vmem_map(vmb, pmb);

            klog("net: pbufs: %u of size %u at %p virt/%p phys\n",
                nbufs, bsize, (void *)vmb.base, (void *)pmb.base);
        }

        char *alloc()
        {
            CriticalGuard cg(sl);

            auto bmp_id = find_first_zero_bit(bmp, nbufs);
            if(bmp_id == nbufs)
                return nullptr;

            auto ret = vmb.base + bmp_id * bsize;

            set_bit(bmp_id, bmp);
            nalloc++;
            return (char *)ret;
        }

        void free(char *p)
        {
            if((uintptr_t)p < vmb.base)
            {
                klog("net: pbuf: invalid address passed to free: %p\n", p);
                return;
            }
            unsigned int cgid = ((uintptr_t)p - vmb.base) / bsize;
            if(cgid >= nbufs)
            {
                klog("net: pbuf: invalid address passed to free: %p\n", p);
                return;
            }

            CriticalGuard cg(sl);
            clear_bit(cgid, bmp);
            nalloc--;
        }

        size_t nfree()
        {
            CriticalGuard cg(sl);
            return nbufs - nalloc;
        }
};

constexpr const unsigned int npbufs = 4 * PAGE_SIZE / PBUF_SIZE;
constexpr const unsigned int nspbufs = PAGE_SIZE / SPBUF_SIZE;

static PBufAllocator<PBUF_SIZE, npbufs> ba;
static PBufAllocator<SPBUF_SIZE, nspbufs> sba;

void init_pbufs()
{
    ba.init();
    sba.init();
}

pbuf_t net_allocate_pbuf(size_t n, size_t offset)
{
    auto act_size = n + sizeof(PBuf) + offset + NET_SIZE_ETHERNET_FOOTER;
    if(act_size <= SPBUF_SIZE)
    {
        auto ret = sba.alloc();
        if(ret)
        {
            if((uintptr_t)ret & 0x3)
            {
                klog("ERROR: unaligned spbuf\n");
            }
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
            if((uintptr_t)ret & 0x3)
            {
                klog("ERROR: unaligned pbuf\n");
            }
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

void net_dump_pbuf(const std::string &intro, const pbuf_t buf)
{
    char dumpbuf[NET_MAX_PACKET_SIZE * 3 + 256];
    char *dbptr = dumpbuf;
    dbptr += sprintf(dbptr, "%s", intro.c_str());
    for(size_t i = 0u; i < buf->GetSize(); i++)
    {
        dbptr += sprintf(dbptr, "%02x ", *buf->Ptr(i));
    }
    klog("%s\n", dumpbuf);
}
