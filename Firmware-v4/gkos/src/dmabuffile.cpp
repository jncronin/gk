#include "osfile.h"
#include "process.h"
#include "pmem.h"

DMABufFile::DMABufFile(PMemBlock _pmb, PProcess _op) : pmb(_pmb), owning_process(_op)
{
    type = FT_DMABuf;
    path = "/dev/mem";
}

DMABufFile::~DMABufFile()
{
    auto p = owning_process.lock();

    /* If process is already destroyed then don't double-free pages */
    if(p)
    {
        Pmem.release(pmb);

        pmb.valid = true;
        CriticalGuard cg(p->owned_pages.sl);
        p->owned_pages.release(pmb);
    }
}

PMemBlock DMABufFile::GetMem() const
{
    return pmb;
}

size_t DMABufFile::Flen(int *_errno)
{
    return pmb.length;
}
