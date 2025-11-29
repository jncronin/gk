#include "gk_conf.h"
#include "syscalls_int.h"
#include "screen.h"
#include "pmem.h"
#include "util.h"
#include "vmem.h"
#include "process.h"

class TripleBufferScreenLayer
{
    protected:
        Spinlock sl;
        PMemBlock pm[3] = { InvalidPMemBlock(), InvalidPMemBlock(), InvalidPMemBlock() };
        unsigned int cur_display = 0;
        unsigned int last_updated = 0;
        unsigned int cur_update = 0;

    public:
        unsigned int update();

        int map_for_process(const VMemBlock &vmem, unsigned int buf, uintptr_t ttbr0);

        friend void init_screen();
};

static TripleBufferScreenLayer scrs[2] = { TripleBufferScreenLayer{}, TripleBufferScreenLayer{} };

void init_screen()
{
    // prepare screen backbuffers for layers 1 and 2
    for(unsigned int layer = 0; layer < 2; layer++)
    {
        for(unsigned int buf = 0; buf < 3; buf++)
        {
            scrs[layer].pm[buf] = Pmem.acquire(scr_layer_size_bytes);
            klog("screen: layer %u, buffer %u @ %llx\n", layer, buf, scrs[layer].pm[buf].base);

            // clear to zero
            if(scrs[layer].pm[buf].valid)
                memset((void *)PMEM_TO_VMEM(scrs[layer].pm[buf].base), 0, scr_layer_size_bytes);
        }
    }
}

uintptr_t screen_update()
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    auto layer = p->screen.screen_layer;
    if(layer >= 2)
        return 0;
    auto buf = scrs[layer].update();
    if(buf >= 3)
        return 0;
    auto vb = p->screen.bufs[buf];
    if(vb.valid)
        return vb.data_start();
    return 0;
}

int screen_map_for_process(const VMemBlock &vmem, unsigned int layer, unsigned int buf,
    uintptr_t ttbr0)
{
    if(layer >= 2)
        return -1;
    return scrs[layer].map_for_process(vmem, buf, ttbr0);
}

int TripleBufferScreenLayer::map_for_process(const VMemBlock &vmem, unsigned int buf,
    uintptr_t ttbr0)
{
    if(buf >= 3)
        return -1;
    if(!vmem.valid || vmem.data_length() < scr_layer_size_bytes)
        return -2;
    CriticalGuard cg(sl);
    return vmem_map(vmem, pm[buf], ttbr0);
}

unsigned int TripleBufferScreenLayer::update()
{
    CriticalGuard cg(sl);
    last_updated = cur_update;

    cur_update = 0;
    // select a new screen to write to
    while(cur_update != last_updated && cur_update != cur_display)
        cur_update++;

    return cur_update;
}
