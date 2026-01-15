#include "process.h"
#include "syscalls_int.h"
#include "screen.h"

#define DEBUG_GPU 0

int syscall_getscreenmodeex(int *width, int *height, int *pf, int *refresh, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    if(width) *width = p->screen.screen_w;
    if(height) *height = p->screen.screen_h;
    if(pf) *pf = p->screen.screen_pf;
    if(refresh) *refresh = p->screen.screen_refresh;
    return 0;
}

static bool scr_width_valid(int w)
{
    if(w < 160 || w > GK_MAX_SCREEN_WIDTH)
        return false;
    if(w & 0x3)
        return false;
    return true;
}

static bool scr_height_valid(int h)
{
    if(h < 120 || h > GK_MAX_SCREEN_HEIGHT)
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
    if(refresh < GK_MIN_SCREEN_REFRESH || refresh > GK_MAX_SCREEN_REFRESH)
        return false;
    return true;
}

int syscall_setscreenmode(int *width, int *height, int *pf, int *refresh, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);

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

    klog("screen: set %ux%u, pf: %u, rr: %u\n", new_width, new_height, new_pf, new_refresh);

    return 0;
}

int syscall_setpalette(unsigned int ncols, const uint32_t *cols, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    //klog("syscall_setpalette: ncols: %u\n", ncols);
    CriticalGuard cg(p->screen.sl);

    ncols = std::min(ncols, 256U);

    p->screen.clut.clear();
    for(auto i = 0U; i < ncols; i++)
        p->screen.clut.push_back(cols[i]);
    
    p->screen.new_clut = true;

    return 0;
}

// old GPUEnqueue methods
static int handle_blitcolor(gpu_message &msg);
static int handle_blitnoblend(gpu_message &msg);
static int fill_missing(gpu_message &msg, bool fill_src, Process::screen_t &cs);
static uint32_t color_convert(unsigned int pf_from, uint32_t from,
    unsigned int pf_to, Process::screen_t &cs);
static inline size_t get_bpp(int pf);
static inline void fill3(char *dest, uint32_t c, size_t len);

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

            case gpu_message_type::CleanCache:
            case gpu_message_type::BlitImageNoBlendIf:
                break;

            case gpu_message_type::BlitColor:
                handle_blitcolor(cur_msg);
                break;

            case gpu_message_type::BlitImageNoBlend:
                handle_blitnoblend(cur_msg);
                break;

            case gpu_message_type::SetScreenMode:
                {
                    int new_w = (int)cur_msg.w;
                    int new_h = (int)cur_msg.h;
                    int new_pf = (int)cur_msg.dest_pf;
                    int new_refr = (int)cur_msg.sx;

                    syscall_setscreenmode(new_w ? &new_w : nullptr,
                        new_h ? &new_h : nullptr,
                        new_pf ? &new_pf : nullptr,
                        new_refr ? &new_refr : nullptr,
                        _errno);
                }
                break;

            case gpu_message_type::SetBlitPalette:
                syscall_setpalette(cur_msg.w, (uint32_t *)cur_msg.src_addr_color, _errno);
                break;

            default:
                klog("gpu: unhandled message type %d\n", (int)cur_msg.type);
                break;
        }
    }

    if(nsent)
    {
        ADDR_CHECK_STRUCT_W(nsent);
        *nsent = nmsg;
    }

    return 0;
}

int handle_blitcolor(gpu_message &m)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    auto &cs = p->screen;
    fill_missing(m, false, cs);

    auto c = color_convert(GK_PIXELFORMAT_ARGB8888, m.src_addr_color, m.dest_pf, cs);
    auto bpp = get_bpp(m.dest_pf);

#if DEBUG_GPU
    klog("gpu: blitcolor: dest_addr: %llx, dx: %u, dy: %u, dw: %u, dh: %u, dp: %u, c: %x, bpp: %u\n",
        m.dest_addr, m.dx, m.dy, m.dw, m.dh, m.dp, c, bpp);
#endif

    for(auto y = 0U; y < m.dh; y++)
    {
        auto line_addr = m.dest_addr + (y + m.dy) * m.dp +
            m.dx * bpp;

        switch(bpp)
        {
            case 4:
                {
                    auto dest = (uint32_t *)line_addr;
                    std::fill(dest, dest + m.dw, c);
                }
                break;
            case 3:
                fill3((char *)line_addr, c, m.dw);
                break;
            case 2:
                {
                    auto dest = (uint16_t *)line_addr;
                    std::fill(dest, dest + m.dw, c);
                }
                break;
            case 1:
                {
                    auto dest = (uint8_t *)line_addr;
                    std::fill(dest, dest + m.dw, c);
                }
                break;
        }
    }

    return 0;
}

int handle_blitnoblend(gpu_message &m)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->screen.sl);
    auto &cs = p->screen;
    fill_missing(m, true, cs);

#if DEBUG_GPU
    klog("gpu: blitnoblend: dest_addr: %llx, dx: %u, dy: %u, dw: %u, dh: %u, dp: %u, src_addr: %llx, sx: %u, sy: %u, sw: %u, sh: %u, sp: %u\n",
        m.dest_addr, m.dx, m.dy, m.dw, m.dh, m.dp, m.src_addr_color, m.sx, m.sy, m.w, m.h, m.sp);
#endif

    if(m.dest_pf == m.src_pf)
    {
        auto bpp = get_bpp(m.dest_pf);

        for(auto y = 0U; y < m.dh; y++)
        {
            auto line_addr = m.dest_addr + (y + m.dy) * m.dp +
                m.dx * bpp;
            auto src_addr = m.src_addr_color + (y + m.sy) * m.sp +
                m.sx * bpp;

            memcpy((void *)line_addr, (const void *)src_addr, m.dw * bpp);
        }
    }
    else
    {
        // pf conversion
        auto dbpp = get_bpp(m.dest_pf);
        auto sbpp = get_bpp(m.src_pf);

        for(auto y = 0U; y < m.dh; y++)
        {
            for(auto x = 0U; x < m.dw; x++)
            {
                auto src_addr = m.src_addr_color + (y + m.sy) * m.sp +
                    (x +m.sx) * sbpp;
                auto dest_addr = m.src_addr_color + (y + m.sy) * m.sp +
                    (x +m.sx) * sbpp;
                auto srcc = *(uint32_t *)src_addr;
                auto destc = color_convert(m.src_pf, srcc, m.dest_pf, cs);

                switch(dbpp)
                {
                    case 4:
                        *(uint32_t *)dest_addr = destc;
                        break;
                    case 3:
                        *(uint8_t *)(dest_addr + 0) = destc & 0xffU;
                        *(uint8_t *)(dest_addr + 1) = (destc >> 8) & 0xffU;
                        *(uint8_t *)(dest_addr + 2) = (destc >> 16) & 0xffU;
                        break;
                    case 2:
                        *(uint16_t *)dest_addr = destc;
                        break;
                    case 1:
                        *(uint8_t *)dest_addr = destc;
                        break;
                }
            }
        }
    }

    return 0;
}

static inline size_t get_bpp(int pf)
{
    switch(pf)
    {
        case GK_PIXELFORMAT_ARGB8888:
        case GK_PIXELFORMAT_XRGB8888:
        case GK_PIXELFORMAT_ABGR8888:
        case GK_PIXELFORMAT_RGBA8888:
        case GK_PIXELFORMAT_BGRA8888:
            return 4;
        case GK_PIXELFORMAT_RGB888:
            return 3;
        case GK_PIXELFORMAT_RGB565:
        case GK_PIXELFORMAT_BGR565:
        case GK_PIXELFORMAT_A8L8:
            return 2;
        case GK_PIXELFORMAT_L8:
        case GK_PIXELFORMAT_A4L4:
        case GK_PIXELFORMAT_ARGB1555:
            return 1;
        default:
            return 0;
    }
}

size_t screen_get_bpp_for_pf(unsigned int pf)
{
    return get_bpp(pf);
}

static int fill_missing(gpu_message &msg, bool fill_src, Process::screen_t &cs)
{
    auto bufs = _screen_current();

    if(msg.dest_addr == 0)
    {
        msg.dest_addr = bufs.first;
        msg.dest_pf = cs.screen_pf;
    }
    if(msg.dw == 0)
    {
        msg.dw = cs.screen_w;
    }
    if(msg.dh == 0)
    {
        msg.dh = cs.screen_h;
    }
    if(msg.dp == 0)
    {
        msg.dp = cs.screen_w * get_bpp(msg.dest_pf);
    }

    if(fill_src)
    {
        if(msg.src_addr_color == 0)
        {
            msg.src_addr_color = bufs.second;
            msg.src_pf = cs.screen_pf;
        }
        if(msg.w == 0)
        {
            msg.w = cs.screen_w;
        }
        if(msg.h == 0)
        {
            msg.h = cs.screen_h;
        }
        if(msg.sp == 0)
        {
            msg.sp = cs.screen_w * get_bpp(msg.src_pf);
        }
    }

    return 0;
}

struct col
{
    uint32_t r, g, b, a;
};

static inline col to_col(unsigned int pf_from, uint32_t from, Process::screen_t &cs)
{
    uint32_t a = 0, r = 0, g = 0, b = 0, l = 0;
    bool is_indexed = false;
    bool indexed_a = false;

    switch(pf_from)
    {
        case GK_PIXELFORMAT_ARGB8888:
            b = from & 0xffU;
            g = (from >> 8) & 0xffU;
            r = (from >> 16) & 0xffU;
            a = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_XRGB8888:
            b = from & 0xffU;
            g = (from >> 8) & 0xffU;
            r = (from >> 16) & 0xffU;
            a = 255;
            break;
        case GK_PIXELFORMAT_ABGR8888:
            r = from & 0xffU;
            g = (from >> 8) & 0xffU;
            b = (from >> 16) & 0xffU;
            a = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_RGBA8888:
            a = from & 0xffU;
            b = (from >> 8) & 0xffU;
            g = (from >> 16) & 0xffU;
            r = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_BGRA8888:
            a = from & 0xffU;
            r = (from >> 8) & 0xffU;
            g = (from >> 16) & 0xffU;
            b = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_RGB888:
            a = 255;
            b = from & 0xffU;
            g = (from >> 8) & 0xffU;
            r = (from >> 16) & 0xffU;
            break;
        case GK_PIXELFORMAT_RGB565:
            a = 255;
            b = (from & 0x1fU) << 3;
            g = ((from >> 5) & 0x3fU) << 2;
            r = ((from >> 11) & 0x1fU) << 3;
            break;
        case GK_PIXELFORMAT_BGR565:
            a = 255;
            r = (from & 0x1fU) << 3;
            g = ((from >> 5) & 0x3fU) << 2;
            b = ((from >> 11) & 0x1fU) << 3;
            break;
        case GK_PIXELFORMAT_L8:
            a = 255;
            l = from & 0xffU;
            is_indexed = true;
            indexed_a = true;
            break;
        case GK_PIXELFORMAT_A8L8:
            a = (from >> 8) & 0xffU;
            l = from & 0xffU;
            is_indexed = true;
            indexed_a = false;
            break;
        case GK_PIXELFORMAT_A4L4:
            a = from & 0xf0U;
            l = from & 0xfU;
            is_indexed = true;
            indexed_a = false;
            break;
        default:
            break;
    }

    if(is_indexed)
    {
        if(l < cs.clut.size())
        {
            b = cs.clut[l] & 0xffU;
            g = (cs.clut[l] >> 8) & 0xffU;
            r = (cs.clut[l] >> 16) & 0xffU;
            if(indexed_a)
                a = (cs.clut[l] >> 24) & 0xffU;
        }
    }

    return { r, g, b, a };
}

static inline uint32_t from_col(unsigned int pf_to, col &from, Process::screen_t &cs)
{
    switch(pf_to)
    {
        case GK_PIXELFORMAT_ARGB8888:
            return from.b ||
                (from.g << 8) ||
                (from.r << 16) ||
                (from.a << 24);
        case GK_PIXELFORMAT_XRGB8888:
            return from.b ||
                (from.g << 8) ||
                (from.r << 16);
        case GK_PIXELFORMAT_ABGR8888:
            return from.r ||
                (from.g << 8) ||
                (from.b << 16) ||
                (from.a << 24);
        case GK_PIXELFORMAT_RGBA8888:
            return from.a ||
                (from.b << 8) ||
                (from.g << 16) ||
                (from.r << 24);
        case GK_PIXELFORMAT_BGRA8888:
            return from.a ||
                (from.r << 8) ||
                (from.g << 16) ||
                (from.b << 24);
        case GK_PIXELFORMAT_RGB888:
            return from.b ||
                (from.g << 8) ||
                (from.r << 16);
        case GK_PIXELFORMAT_RGB565:
            return (from.b >> 3) ||
                ((from.g & 0xfcU) << 3) ||
                ((from.r & 0xf8U << 8));
        case GK_PIXELFORMAT_BGR565:
            return (from.r >> 3) ||
                ((from.g & 0xfcU) << 3) ||
                ((from.b & 0xf8U << 8));
        default:
            return 0;
    }
}

uint32_t color_convert(unsigned int pf_from, uint32_t from,
    unsigned int pf_to, Process::screen_t &cs)
{
    auto intermediate = to_col(pf_from, from, cs);
    return from_col(pf_to, intermediate, cs);
}

void fill3(char *dest, uint32_t c, size_t len)
{
    auto b = (uint8_t)(c & 0xffU);
    auto g = (uint8_t)((c >> 8) & 0xffU);
    auto r = (uint8_t)((c >> 16) & 0xffU);

    while(len--)
    {
        dest[0] = b;
        dest[1] = g;
        dest[2] = r;
        dest += 3;
    }
}
