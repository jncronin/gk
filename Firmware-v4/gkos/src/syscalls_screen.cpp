#include "process.h"
#include "syscalls_int.h"
#include "screen.h"

int syscall_getscreenmodeex(int *width, int *height, int *pf, int *refresh, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    p->_init_screen();
    if(width) *width = p->screen.screen_w;
    if(height) *height = p->screen.screen_h;
    if(pf) *pf = p->screen.screen_pf;
    if(refresh) *refresh = p->screen.screen_refresh;
    return 0;
}

static bool scr_width_valid(int w)
{
    if(w < 160 || w > 800)
        return false;
    if(w & 0x3)
        return false;
    return true;
}

static bool scr_height_valid(int h)
{
    if(h < 120 || h > 480)
        return false;
    if(h & 0x3)
        return false;
    return true;
}

static bool scr_pf_valid(int pf)
{
    if(pf < 0 || pf > GK_PIXELFORMAT_MAX)
        return false;
    return true;
}

static bool scr_refresh_valid(int refresh)
{
    if(refresh < 24 || refresh > 60)
        return false;
    return true;
}

int syscall_setscreenmode(int *width, int *height, int *pf, int *refresh, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    p->_init_screen();

    // only update if all provided parameters are valid
    auto new_width = p->screen.screen_w;
    auto new_height = p->screen.screen_h;
    auto new_pf = p->screen.screen_pf;
    auto new_refresh = p->screen.screen_refresh;

    if(width)
    {
        if(!scr_width_valid(*width))
        {
            *_errno = EINVAL;
            return -1;
        }
        new_width = *width;
    }
    if(height)
    {
        if(!scr_height_valid(*height))
        {
            *_errno = EINVAL;
            return -1;
        }
        new_height = *height;
    }
    if(pf)
    {
        if(!scr_pf_valid(*pf))
        {
            *_errno = EINVAL;
            return -1;
        }
        new_pf = *pf;
    }
    if(refresh)
    {
        if(!scr_refresh_valid(*refresh))
        {
            *_errno = EINVAL;
            return -1;
        }
        new_refresh = *refresh;
    }

    p->screen.screen_w = new_width;
    p->screen.screen_h = new_height;
    p->screen.screen_pf = new_pf;
    p->screen.screen_refresh = new_refresh;

    return 0;
}

// old GPUEnqueue methods
int syscall_gpuenqueue(const gpu_message *msgs, size_t nmsg, size_t *nsent, int *_errno)
{
    ADDR_CHECK_BUFFER_R(msgs, nmsg * sizeof(msgs));

    for(auto i = 0U; i < nmsg; i++)
    {
        auto cur_msg = msgs[i];

        switch(cur_msg.type)
        {
            case gpu_message_type::FlipBuffers:
                {
                    auto next_buffer = screen_update();
                    if(cur_msg.dest_addr)
                    {
                        ADDR_CHECK_STRUCT_W((uintptr_t *)cur_msg.dest_addr);

                        *(uintptr_t *)cur_msg.dest_addr = next_buffer;
                    }
                }
                break;

            case gpu_message_type::SignalThread:
                break;

            default:
                klog("gpu: unhandled message type %d\n", (int)cur_msg.type);
        }
    }

    if(nsent)
    {
        ADDR_CHECK_STRUCT_W(nsent);
        *nsent = nmsg;
    }

    return 0;
}
