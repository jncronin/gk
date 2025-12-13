#include "gk_conf.h"
#include "syscalls_int.h"
#include "screen.h"
#include "pmem.h"
#include "util.h"
#include "vmem.h"
#include "process.h"
#include "process_interface.h"

#if GK_GPU_SHOW_FPS
class FPSCounter
{
    protected:
        Spinlock sl;
        kernel_time last_dump;
        unsigned int nframes = 0;
        int layer;

    public:
        FPSCounter(int _layer) : layer(_layer) {}

        void Tick()
        {
            CriticalGuard cg(sl);
            auto now = clock_cur();
            nframes++;

            auto tdiff = now - last_dump;

            if(tdiff >= kernel_time_from_ms(1000))
            {
                auto fps = (nframes * 10000000ULL) /  kernel_time_to_us(tdiff);
                klog("screen: layer %d: FPS: %lu\n", layer, fps);

                nframes = 0;
                last_dump = now;
            }
        }
};

static FPSCounter fpsc[scr_n_layers]
{
    FPSCounter(0),
    FPSCounter(1)
};

#endif

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
        std::pair<unsigned int, unsigned int> current();

        friend void init_screen();
        friend PMemBlock screen_get_buf(unsigned int, unsigned int);
};

static TripleBufferScreenLayer scrs[2] = { TripleBufferScreenLayer{}, TripleBufferScreenLayer{} };
VMemBlock l1_priv[3] = { InvalidVMemBlock(), InvalidVMemBlock(), InvalidVMemBlock() };

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

            if(layer == 1)
            {
                // map for kernel
                l1_priv[buf] = vblock_alloc(vblock_size_for(scr_layer_size_bytes),
                    false, true, false);
                if(l1_priv[buf].valid == false)
                {
                    klog("screen: l1 unable to allocate kernel buffer\n");
                    while(true);
                }
                for(uintptr_t offset = 0; offset < scr_layer_size_bytes; offset += VBLOCK_64k)
                {
                    vmem_map(l1_priv[buf].data_start() + offset,
                        scrs[layer].pm[buf].base + offset, false, true, false, ~0ULL, ~0ULL,
                        nullptr, MT_NORMAL_WT);
                }
            }
        }
    }
}

static uintptr_t screen_buf_to_vaddr(unsigned int layer, unsigned int buf)
{
    if(layer > 1)
        return 0;
    if(buf > 3)
        return 0;

    if(layer == 0)
    {
        // userspace mapping
        switch(buf)
        {
            case 0:
                return GK_SCR_L1_B0;
            case 1:
                return GK_SCR_L1_B1;
            case 2:
                return GK_SCR_L1_B2;
        }
    }
    else
    {
        return l1_priv[buf].data_start();
    }

    return 0;
}

std::pair<uintptr_t, uintptr_t> screen_current()
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    return _screen_current();
}

std::pair<uintptr_t, uintptr_t> _screen_current()
{
    auto p = GetCurrentProcessForCore();
    auto layer = p->screen.screen_layer;
    auto buf = scrs[layer].current();
    if(buf.first >= 3)
        return std::make_pair(0, 0);

    return std::make_pair(screen_buf_to_vaddr(layer, buf.first),
        screen_buf_to_vaddr(layer, buf.second));
}

uintptr_t screen_update()
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    auto layer = p->screen.screen_layer;
    auto buf = scrs[layer].update();
#if GK_GPU_SHOW_FPS
    fpsc[layer].Tick();
#endif
    return screen_buf_to_vaddr(layer, buf);
}

PMemBlock screen_get_buf(unsigned int layer, unsigned int buf)
{
    if(layer >= 2 || buf >= 3)
        return InvalidPMemBlock();
    CriticalGuard cg(scrs[layer].sl);
    return scrs[layer].pm[buf];
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

std::pair<unsigned int, unsigned int> TripleBufferScreenLayer::current()
{
    CriticalGuard cg(sl);
    return std::make_pair(cur_update, last_updated);
}
