#include <stm32h7rsxx.h>
#include "gpu.h"
#include "osqueue.h"
#include "screen.h"
#include "cache.h"
//#include "mdma.h"
#include "process.h"
#include "gk_conf.h"

#define GPU_DEBUG 0

SRAM4_DATA static BinarySemaphore gpu_ready;
SRAM4_DATA static BinarySemaphore mdma_ready;
extern Process p_supervisor;

struct gpu_message_int
{
    gpu_message g;
    Thread *sending_thread;
};

SRAM4_DATA static FixedQueue<gpu_message_int, 64> gpu_msg_list;

extern Condition scr_vsync;

SRAM4_DATA static void *scaling_bb[2];
SRAM4_DATA static int scaling_bb_idx = 0;

static constexpr const unsigned int gpu_mdma_channel = 3;
//static const auto mdma = ((MDMA_Channel_TypeDef *)(MDMA_BASE + 0x40U + 0x40U * gpu_mdma_channel));

#if GK_GPU_SHOW_FPS
SRAM4_DATA static uint64_t last_flip_time = 0ULL;
#endif

extern bool screen_flip_in_progress;

static inline void *get_scaling_bb(void **old_buf = nullptr)
{
    if(old_buf)
    {
        *old_buf = scaling_bb[(scaling_bb_idx + 1) & 1];
    }
    return scaling_bb[scaling_bb_idx & 1];
}

//static void mdma_handler();

static inline void wait_dma2d()
{
    while(DMA2D->CR & DMA2D_CR_START)
    {
        gpu_ready.Wait(/*clock_cur_ms() + 5ULL*/);
    }
    //while(mdma->CCR & MDMA_CCR_EN)
    //{
    //    mdma_ready.Wait(/*clock_cur_ms() + 5ULL*/);
    //}
}

static inline void wait_nema(const Process *cur_process)
{
    extern Mutex m_ehold;
    /* Ignore cases where we've already got the mutex - its only released on 
        LTDC register updates which occasionally does happen 1:1 with wait_nema() */
    m_ehold.lock(true);

    /* Don't stop process using GPU2D during screen update if ignoring vsync */
    if(cur_process->screen_ignore_vsync)
        m_ehold.unlock();
}

static inline size_t get_bpp(int pf)
{
    switch(pf)
    {
        case GK_PIXELFORMAT_ARGB8888:
            return 4;
        case GK_PIXELFORMAT_RGB888:
            return 3;
        case GK_PIXELFORMAT_RGB565:
            return 2;
        case GK_PIXELFORMAT_L8:
            return 1;
        default:
            if((pf & 0xf) == 11)
            {
                // YCbCr - interpret CSS
                switch((pf & DMA2D_FGPFCCR_CSS_Msk) >> DMA2D_FGPFCCR_CSS_Pos)
                {
                    case 0:
                        return 3;
                    case 1:
                        return 2;
                    case 2:
                        return 2;   // 1.5 really...
                }
            }
            return 0;
    }
}

static inline uint32_t color_encode(uint32_t col, uint32_t pf)
{
    switch(pf)
    {
        case GK_PIXELFORMAT_ARGB8888:
            return col;
        case GK_PIXELFORMAT_RGB888:
            return col & 0xffffffU;
        case GK_PIXELFORMAT_RGB565:
            {
                uint32_t r = (col >> 3) & 0x1f;
                uint32_t g = (col >> 10) & 0x3f;
                uint32_t b = (col >> 14) & 0x1f;
                return r | (g << 5) | (b << 11);
            }
        default:
            return 0;
    }
}

[[maybe_unused]]static void handle_flipbuffers(const gpu_message_int &curmsg, gpu_message_int *newmsgs, size_t *nnewmsgs)
{
    const auto cur_process = &curmsg.sending_thread->p;

    // get data members fron newmsgs[0] prior to overwriting them
    newmsgs[1].g.type = FlipScaleBuffers;
    newmsgs[1].g.dest_addr = curmsg.g.dest_addr;
    newmsgs[1].g.src_addr_color = curmsg.g.src_addr_color;
    newmsgs[1].sending_thread = curmsg.sending_thread;

    newmsgs[2].g.type = FlipBuffers;
    newmsgs[2].g.dest_addr = 0;
    newmsgs[2].g.src_addr_color = 0;
    newmsgs[2].sending_thread = curmsg.sending_thread;

    // we need to copy + scale from scale_bb and then flip the buffers
    newmsgs[0].g.type = BlitImageNoBlend;
    newmsgs[0].g.dest_addr = (uint32_t)(uintptr_t)screen_get_frame_buffer();
    newmsgs[0].g.dx = 0;
    newmsgs[0].g.dy = 0;
    newmsgs[0].g.dest_pf = cur_process->screen_pf;
    newmsgs[0].g.dp = 640 * get_bpp(newmsgs[0].g.dest_pf);
    newmsgs[0].g.sx = 0;
    newmsgs[0].g.sy = 0;
    newmsgs[0].g.sp = cur_process->screen_w * get_bpp(newmsgs[0].g.dest_pf);
    newmsgs[0].g.src_pf = cur_process->screen_pf;
    newmsgs[0].g.w = cur_process->screen_w;
    newmsgs[0].g.h = cur_process->screen_h;
    newmsgs[0].g.dw = 640;
    newmsgs[0].g.dh = 480;
    newmsgs[0].g.src_addr_color = (uint32_t)(uintptr_t)get_scaling_bb();
    newmsgs[0].sending_thread = curmsg.sending_thread;

    *nnewmsgs = 3;
}

static inline uint32_t pfc_convert(uint32_t c, uint32_t from_pf, uint32_t to_pf)
{
    if(from_pf == to_pf)
        return c;
    uint32_t argb = 0;
    switch(from_pf)
    {
        case GK_PIXELFORMAT_ARGB8888:
            argb = c;
            break;
        case GK_PIXELFORMAT_RGB565:
            argb = 0xff000000 |
                ((c & 0x1fU) << 3) |
                ((c & 0x7e0U) << 5) |
                ((c & 0xf800U) << 8);
            break;
        case GK_PIXELFORMAT_RGB888:
            argb = 0xff000000 | c;
            break;
        case GK_PIXELFORMAT_L8:
            argb = 0xff000000 |
                (c & 0xffU) |
                ((c & 0xffU) << 8) |
                ((c & 0xffU) << 16);
            break;
    }

    switch(to_pf)
    {
        case GK_PIXELFORMAT_ARGB8888:
            return argb;
        case GK_PIXELFORMAT_RGB888:
            return argb & 0x00ffffffU;
        case GK_PIXELFORMAT_RGB565:
            {
                auto r = (argb & 0xf8U) >> 3;
                auto g = (argb & 0xfc00U) >> 5;
                auto b = (argb & 0xf80000U) >> 8;
                return r | g | b;
            }
        case GK_PIXELFORMAT_L8:
            return argb & 0xffU;
    }
    return 0U;
}

static void handle_scale_blit_cpu(const gpu_message &g)
{
    // slow implementation on CPU - manages ~20fps for 160x120 -> 640x480 without PFC
    auto sbpp = get_bpp(g.src_pf);
    auto dbpp = get_bpp(g.dest_pf);
    if(g.src_addr_color < 0x90000000 || g.src_addr_color >= 0x90400000)
    {
        CleanAndInvalidateM7Cache(g.src_addr_color + g.sy * g.sp + g.sx * sbpp,
            g.h * g.sp, CacheType_t::Data);
    }

    for(unsigned int y = 0; y < g.dh; y++)
    {
        for(unsigned int x = 0; x < g.dw; x++)
        {
            auto daddr = g.dest_addr + (g.dy + y) * g.dp + (g.dx + x) * dbpp;
            auto sx = (x * g.w) / g.dw;
            auto sy = (y * g.h) / g.dh;
            auto saddr = g.src_addr_color + (g.sy + sy) * g.sp + (g.sx + sx) * sbpp;

            uint32_t c = *(uint32_t *)saddr;
            c = pfc_convert(c, g.src_pf, g.dest_pf);

            switch(dbpp)
            {
                case 4:
                    *(uint32_t *)daddr = c;
                    break;
                case 3:
                    for(int i = 0; i < 3; i++)
                    {
                        *(uint8_t *)(daddr + i) = c;
                        c >>= 8;
                    }
                    break;
                case 2:
                    *(uint16_t *)daddr = c;
                    break;
                case 1:
                    *(uint8_t *)daddr = c;
                    break;
            }
        }
    }

    if(g.dest_addr < 0x90000000 || g.dest_addr >= 0x90400000)
    {
        CleanM7Cache(g.dest_addr + g.dy * g.dp + g.dx * dbpp, g.dh * g.dp, CacheType_t::Data);
    }
}

uint32_t mdma_ll[16*16] __attribute__((aligned(32)));

#if 0
[[maybe_unused]] static bool handle_scale_blit_dma(const gpu_message &g)
{
    auto bpp = get_bpp(g.src_pf);

    auto sx = (uint32_t)g.dw / g.w;
    auto sy = (uint32_t)g.dh / g.h;

    if(bpp != 2 && bpp != 4)
        return false;

    if(bpp == 4 && (sx > 2 || sy > 2))
        return false;

    if(sx * sy > 16)
        return false;

    unsigned int size = 0;
    switch(bpp)
    {
        case 2:
            size = 1U;
            break;
        case 4:
            size = 2U;
            break;
    }
    unsigned int sincos = size;
    unsigned int dincos = 0;
    switch(bpp * sx)
    {
        case 2:
            dincos = 1;
            break;
        case 4:
            dincos = 2;
            break;
        case 8:
            dincos = 3;
            break;
    }

#if GPU_DEBUG
    {
        klog("gpu: mdma scale: dest: %x, src: %x, sx: %d, sy: %d, destx: %d, desty: %d, srcx: %d, srcy: %d, dincos: %d, sincos: %d, bpp: %d\n",
            (uint32_t)g.dest_addr, (uint32_t)g.src_addr_color, sx, sy, g.dx, g.dy, g.sx, g.sy, dincos, sincos, bpp);
    }
#endif

    // build linked lists for MDMA
    int mdma_ll_idx = 0;

    for(unsigned int csy = 0; csy < sy; csy++)
    {
        for(unsigned int csx = 0; csx < sx; csx++)
        {
            auto cll = &mdma_ll[mdma_ll_idx];
            mdma_ll_idx += 16;

            auto xaddr = g.dx + csx;
            auto yaddr = g.dy + csy;

            // CTCR
            // BWM - minimal difference but appears not harmful
            cll[0] = MDMA_CTCR_SWRM |
                MDMA_CTCR_BWM |
                (3U << MDMA_CTCR_TRGM_Pos) |            // repeated block transfer, follow linked list
                (31U << MDMA_CTCR_TLEN_Pos) |
                (5U << MDMA_CTCR_DBURST_Pos) |
                (5U << MDMA_CTCR_SBURST_Pos) |
                (dincos << MDMA_CTCR_DINCOS_Pos) |
                (sincos << MDMA_CTCR_SINCOS_Pos) |
                (size << MDMA_CTCR_DSIZE_Pos) |
                (size << MDMA_CTCR_SSIZE_Pos) |
                (2U << MDMA_CTCR_DINC_Pos) |
                (2U << MDMA_CTCR_SINC_Pos);

            // CBNDTR
            cll[1] = ((uint32_t)g.h << MDMA_CBNDTR_BRC_Pos) |
                ((bpp * (uint32_t)g.w) << MDMA_CBNDTR_BNDT_Pos);

            // CSAR
            cll[2] = g.src_addr_color + (uint32_t)g.sy * g.sp + (uint32_t)g.sx * bpp;

            // CDAR
            cll[3] = g.dest_addr + yaddr * g.dp + xaddr * bpp;

            // CBRUR
            cll[4] = ((sy - 1) * (uint32_t)g.dp) << MDMA_CBRUR_DUV_Pos;

            // CLAR
            cll[5] = ((csx == sx - 1) && (csy == sy - 1)) ? 0U : (uint32_t)(uintptr_t)&mdma_ll[mdma_ll_idx];

            // CTBR
            cll[6] = 0U;

            // Reserved
            cll[7] = 0U;

            // CMAR
            cll[8] = 0U;

            // CMDR
            cll[9] = 0U;
        }
    }
    // load first set of registers
    for(int i = 0; i < 10; i++)
    {
        (&mdma->CTCR)[i] = mdma_ll[i];
    }
    CleanM7Cache((uint32_t)(uintptr_t)mdma_ll, 4*16*16, CacheType_t::Data);

    mdma->CIFCR = 0x1fU;
    mdma->CCR = MDMA_CCR_EN;
    mdma->CCR = MDMA_CCR_EN | MDMA_CCR_SWRQ | MDMA_CCR_CTCIE | MDMA_CCR_TEIE;

    return true;
}
#endif

static void handle_scale_blit(const gpu_message &g)
{
    /*if(g.dw > g.w && g.dh > g.h)
    {
        if((g.dw % g.w == 0) && (g.dh % g.h == 0) && (g.dest_pf == g.src_pf))
        {
            if(handle_scale_blit_dma(g))
                return;
        }
    }*/
    handle_scale_blit_cpu(g);
}

static void handle_clearscreen(const gpu_message_int &curmsg, gpu_message_int *newmsgs, size_t *nnewmsgs)
{
    newmsgs[0].g.type = BlitColor;
    newmsgs[0].g.dest_addr = 0;
    newmsgs[0].g.dx = 0;
    newmsgs[0].g.dy = 0;
    newmsgs[0].g.w = 0;
    newmsgs[0].g.h = 0;
    newmsgs[0].g.dp = 0;

    *nnewmsgs = 1;
}

void *gpu_thread(void *p)
{
    (void)p;

    void init_nema();
    init_nema();

    // Set up our scaling backbuffers (for 320x240 screen and 160x120 screen) - 32bpp
    scaling_bb[0] = (void *)0x9012c000;
    scaling_bb[1] = (void *)0x9032c000;
    scaling_bb_idx = 0;

    RCC->AHB5ENR |= (RCC_AHB5ENR_DMA2DEN);
    (void)RCC->AHB5ENR;
    //RCC->AHB3ENR |= (RCC_AHB3ENR_DMA2DEN | RCC_AHB3ENR_MDMAEN);
    //(void)RCC->AHB3ENR;

    NVIC_EnableIRQ(DMA2D_IRQn);

    //mdma_register_handler(mdma_handler, gpu_mdma_channel);

    while(true)
    {
        gpu_message_int curmsgs[8];
        if(!gpu_msg_list.Pop(&curmsgs[0]))
            continue;

        const auto cur_process = &curmsgs[0].sending_thread->p;

        // Some messages decompose to more than one - handle this here
        size_t nmsgs = sizeof(curmsgs) / sizeof(gpu_message_int);

        // Handle messages which decompose to others
        switch(curmsgs[0].g.type)
        {
            case ClearScreen:
                handle_clearscreen(curmsgs[0], curmsgs, &nmsgs);
                break;
            /*case FlipBuffers:
                if(curmsgs[0].sending_thread->p.screen_w == 640 &&
                    curmsgs[0].sending_thread->p.screen_h == 480)
                {
                    nmsgs = 1;
                }
                else
                {
                    handle_flipbuffers(curmsgs[0], curmsgs, &nmsgs);
                }
                break;*/
            default:
                nmsgs = 1;
                break;
        }

        for(size_t i = 0; i < nmsgs; i++)
        {
            auto &g = curmsgs[i];
#if GPU_DEBUG
            auto ltdc_curfb = LTDC_Layer1->CFBAR;
            auto scr_curfb = (uint32_t)(uintptr_t)screen_get_frame_buffer();
#endif

            // We should never be able to write to the current framebuffer
#if 0
            {
                while(LTDC_Layer1->CFBAR == (uint32_t)(uintptr_t)screen_get_frame_buffer())
                    Yield();
            }
#endif

            /* get details on pixel formats, strides etc */
            uint32_t dest_pf = g.g.dest_addr ? g.g.dest_pf : cur_process->screen_pf;
            uint32_t src_pf = g.g.src_addr_color ? g.g.src_pf : cur_process->screen_pf;
            uint32_t src_pf_for_psize = src_pf & 0xff;
            uint32_t src_pf_for_blend = src_pf >> 8;
            uint32_t dest_pf_for_psize = dest_pf & 0xff;
            uint32_t dest_pf_for_blend = dest_pf >> 8;
            if(!src_pf_for_blend) src_pf_for_blend = src_pf_for_psize;
            if(!dest_pf_for_blend) dest_pf_for_blend = dest_pf_for_psize;
            int bpp = get_bpp(dest_pf_for_psize);
            //uint32_t scr_fbuf = (uint32_t)(uintptr_t)screen_get_frame_buffer();
            uint32_t dest_addr;
            uint32_t dest_pitch;
            uint32_t dest_w, dest_h;
            uint32_t src_addr;
            uint32_t src_pitch;
            uint32_t src_w, src_h;
            if(g.g.dest_addr == 0)
            {
                if(cur_process == &p_supervisor)
                {
                    dest_addr = (uint32_t)(uintptr_t)screen_get_overlay_frame_buffer();
                    dest_pitch = 640 * bpp;
                    if(g.g.dw == 0 && g.g.dh == 0)
                    {
                        dest_w = 640;
                        dest_h = 480;
                    }
                    else
                    {
                        dest_w = g.g.dw;
                        dest_h = g.g.dh;
                    }
                }
                else if((cur_process->screen_w == 640 && cur_process->screen_h == 480) ||
                    cur_process->screen_software_scale == false)
                {
                    dest_addr = (uint32_t)(uintptr_t)screen_get_frame_buffer();
                    dest_pitch = cur_process->screen_w * bpp;
                    if(g.g.dw == 0 && g.g.dh == 0)
                    {
                        dest_w = cur_process->screen_w;
                        dest_h = cur_process->screen_h;
                    }
                    else
                    {
                        dest_w = g.g.dw;
                        dest_h = g.g.dh;
                    }
                }
                else
                {
                    dest_addr = (uint32_t)(uintptr_t)get_scaling_bb();
                    dest_pitch = cur_process->screen_w * bpp;
                    if(g.g.dw == 0 && g.g.dh == 0)
                    {
                        dest_w = cur_process->screen_w;
                        dest_h = cur_process->screen_h;
                    }
                    else
                    {
                        dest_w = g.g.dw;
                        dest_h = g.g.dh;
                    }
                }
            }
            else
            {
                dest_addr = g.g.dest_addr;
                dest_pitch = g.g.dp;
                dest_w = g.g.dw;
                dest_h = g.g.dh;
            }

            if(g.g.src_addr_color == 0)
            {
                if(cur_process == &p_supervisor)
                {
                    src_addr = (uint32_t)(uintptr_t)screen_get_overlay_frame_buffer(false);
                    src_pitch = 640 * bpp;
                    if(g.g.w == 0 && g.g.h == 0)
                    {
                        src_w = 640;
                        src_h = 480;
                    }
                    else
                    {
                        src_w = g.g.w;
                        src_h = g.g.h;
                    }
                }
                else if((cur_process->screen_w == 640 && cur_process->screen_h == 480) ||
                    cur_process->screen_software_scale == false)
                {
                    src_addr = (uint32_t)(uintptr_t)screen_get_frame_buffer(false);
                    src_pitch = cur_process->screen_w * bpp;
                    if(g.g.w == 0 && g.g.h == 0)
                    {
                        src_w = cur_process->screen_w;
                        src_h = cur_process->screen_h;
                    }
                    else
                    {
                        src_w = g.g.w;
                        src_h = g.g.h;
                    }
                }
                else
                {
                    get_scaling_bb((void **)&src_addr);
                    src_pitch = cur_process->screen_w * bpp;
                    if(g.g.w == 0 && g.g.h == 0)
                    {
                        src_w = cur_process->screen_w;
                        src_h = cur_process->screen_h;
                    }
                    else
                    {
                        src_w = g.g.w;
                        src_h = g.g.h;
                    }
                }
            }
            else
            {
                src_addr = g.g.src_addr_color;
                src_pitch = g.g.sp;
                src_w = g.g.w;
                src_h = g.g.h;
            }


#if GPU_DEBUG
            {
                klog("gpu: @%u type: %d, dest_addr: %x, src_addr_color: %x, dw: %x, dh: %x, w: %x, h: %x\n",
                    (unsigned int)clock_cur_us(), g.g.type, dest_addr, src_addr, dest_w, dest_h, src_w, src_h);
                klog("gpu: ltdc_fb: %x, scr_fb: %x\n", ltdc_curfb, scr_curfb);
            }
#endif
            
            switch(g.g.type)
            {
                case gpu_message_type::FlipBuffers:
                    //if(!cur_process->screen_ignore_vsync)
                    {
                        wait_dma2d();
                        wait_nema(cur_process);
                    }
                    {
                        void *old_buf, *new_buf;
                        if(cur_process == &p_supervisor)
                        {
                            extern bool is_overlay_visible();
                            new_buf = screen_flip_overlay(&old_buf, true, is_overlay_visible() ? 255 : 0);
                        }
                        else
                        {
                            new_buf = screen_flip(&old_buf);
                        }
                        if(g.g.dest_addr)
                        {
                            *(void **)g.g.dest_addr = new_buf;
                        }
                        if(g.g.src_addr_color)
                        {
                            *(void **)g.g.src_addr_color = old_buf;
                        }
                    }
                    if(!cur_process->screen_ignore_vsync)
                    {
                        //while(screen_flip_in_progress);
                        //scr_vsync.Wait();
                    }

#if GK_GPU_SHOW_FPS
                    {
                        auto curt = clock_cur_us();
                        auto cticks = curt - last_flip_time;

                        static uint64_t cticks_arr[32];
                        static int cticks_arr_idx = 0;

                        cticks_arr[cticks_arr_idx++] = cticks;
                        if(cticks_arr_idx >= 32) cticks_arr_idx = 0;

                        uint64_t ticks_avg = 0;
                        for(int j = 0; j < 32; j++)
                            ticks_avg += cticks_arr[j];
                        ticks_avg /= 32;

                        last_flip_time = curt;
                        {
                            klog("gpu: cticks: %u, fps: %u (%u)\n",
                                (uint32_t)cticks, 1000000U / (uint32_t)cticks,
                                1000000U / (uint32_t)ticks_avg);
                        }
                    }
#endif
                    break;

                case gpu_message_type::FlipScaleBuffers:
                    if(!cur_process->screen_ignore_vsync)
                    {
                        wait_dma2d();
                    }
                    scaling_bb_idx++;
                    {
                        void *old_buf;
                        if(g.g.dest_addr)
                        {
                            *(void **)g.g.dest_addr = get_scaling_bb(&old_buf);
                            if(g.g.src_addr_color)
                            {
                                *(void **)g.g.src_addr_color = old_buf;
                            }
                        }
                    }
                    break;

                case gpu_message_type::SetBuffers:
                    screen_set_frame_buffer((void *)g.g.dest_addr, (void*)g.g.src_addr_color);
                    break;

                case gpu_message_type::SetScreenMode:
                    {
                        // sanity check
                        if((g.g.dest_pf != GK_PIXELFORMAT_ARGB8888) &&
                            (g.g.dest_pf != GK_PIXELFORMAT_RGB888) &&
                            (g.g.dest_pf != GK_PIXELFORMAT_RGB565))
                        {
                            break;
                        }
                        if(g.g.w == 0 || g.g.h == 0)
                        {
                            cur_process->screen_pf = g.g.dest_pf;
                        }
                        else if(g.g.w <= 160 && g.g.h <= 120)
                        {
                            cur_process->screen_w = 160;
                            cur_process->screen_h = 120;
                            cur_process->screen_pf = g.g.dest_pf;
                            cur_process->screen_software_scale = true;
                            screen_set_hardware_scale(x1, x1);                            
                        }
                        else if(g.g.w <= 320 && g.g.h <= 240)
                        {
                            cur_process->screen_w = 320;
                            cur_process->screen_h = 240;
                            cur_process->screen_pf = g.g.dest_pf;
                            cur_process->screen_software_scale = false;
                            screen_set_hardware_scale(x2, x2);
                        }
                        else if(g.g.w <= 640 && g.g.h <= 480)
                        {
                            cur_process->screen_w = 640;
                            cur_process->screen_h = 480;
                            cur_process->screen_pf = g.g.dest_pf;
                            screen_set_hardware_scale(x1, x1);
                        }

                        extern Process p_supervisor;
                        p_supervisor.events.Push({ .type = Event::CaptionChange });
                    }
                    break;

                case gpu_message_type::CleanCache:
                    {
                        auto start_addr = dest_addr + g.g.dx * bpp + g.g.dy * dest_pitch;
                        auto len = g.g.h * dest_pitch + g.g.w * bpp;

                        auto cache_line_start = start_addr & ~0x1fU;
                        auto cache_line_end = (start_addr + len + 0x1fU) & ~0x1fU;

                        auto need_to_clean = true;
                        if((cache_line_start >= 0x90000000) && (cache_line_end < 0x90400000))
                        {
                            need_to_clean = false;
                        }
                        else
                        {
                            for(const auto &mr : cur_process->mmap_regions)
                            {
                                if(mr.second.is_sync &&
                                    (cache_line_start >= mr.second.mr.address) &&
                                    (cache_line_end < (mr.second.mr.address + mr.second.mr.length)))
                                {
                                    need_to_clean = false;
                                    break;
                                }
                            }
                        }

                        if(need_to_clean)
                        {
                            CleanM7Cache(cache_line_start, cache_line_end - cache_line_start, CacheType_t::Data);
                        }
                    }
                    break;

                case gpu_message_type::SignalThread:
                    if(!cur_process->screen_ignore_vsync)
                    {
                        wait_dma2d();
                        while(screen_flip_in_progress);
                        scr_vsync.Wait();
                    }
                    {
                        auto t = g.sending_thread;
                        {
                            CriticalGuard cg(t->sl);
                            t->ss_p.ival1 = 0;
                            t->ss.Signal();
                        }
                    }
                    break;

                case gpu_message_type::BlitColor:
                    if(!dest_w || !dest_h)
                        break;
                    if(!cur_process->screen_ignore_vsync)
                    {
                        wait_dma2d();
                    }
                    DMA2D->OPFCCR = dest_pf;
                    DMA2D->OCOLR = color_encode(g.g.src_addr_color, dest_pf);
                    DMA2D->OMAR = dest_addr + g.g.dx * bpp + g.g.dy * dest_pitch;
                    DMA2D->OOR = (dest_pitch / bpp) - dest_w;
                    DMA2D->NLR = (dest_w << DMA2D_NLR_PL_Pos) | (dest_h << DMA2D_NLR_NL_Pos);
                    DMA2D->CR = DMA2D_CR_TCIE |
                        DMA2D_CR_TEIE |
                        (3UL << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                    break;

                case gpu_message_type::BlitImage:
                case gpu_message_type::BlitImageNoBlend:
                    if(!dest_w || !dest_h)
                        break;
                    if(!cur_process->screen_ignore_vsync)
                    {                    
                        wait_dma2d();
                    }
                    if(dest_w == src_w && dest_h == src_h)
                    {
                        // can do DMA2D copy
                        DMA2D->OPFCCR = dest_pf_for_psize;
                        DMA2D->OMAR = dest_addr + g.g.dx * bpp + g.g.dy * dest_pitch;
                        DMA2D->OOR = (dest_pitch / bpp) - dest_w;
                        DMA2D->NLR = (dest_w << DMA2D_NLR_PL_Pos) | (dest_h << DMA2D_NLR_NL_Pos);

                        // configure source, +/- pixel format correction
                        auto src_bpp = get_bpp(src_pf_for_psize);
                        DMA2D->FGMAR = src_addr + g.g.sx * src_bpp + g.g.sy * src_pitch;
                        DMA2D->FGPFCCR = src_pf_for_psize;
                        DMA2D->FGOR = (src_pitch / src_bpp) - dest_w;

                        // does it blend?
                        uint32_t mode = 0;
                        if(src_pf_for_blend == GK_PIXELFORMAT_ARGB8888 && g.g.type == BlitImage)
                        {
                            mode = 2U;
                            
                            // set background as scratch to allow blend
                            DMA2D->BGMAR = dest_addr + g.g.dx * bpp + g.g.dy * dest_pitch;
                            DMA2D->BGOR = (dest_pitch / bpp) - dest_w;
                            DMA2D->BGPFCCR = dest_pf_for_psize;
                        }
                        else if(src_pf_for_psize != dest_pf_for_psize)
                        {
                            // need pixel format conversion, but no blending
                            mode = 1U;
                        }
                        DMA2D->CR = DMA2D_CR_TCIE | 
                            DMA2D_CR_TEIE |
                            (mode << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                    }
                    else
                    {
                        g.g.dest_addr = dest_addr;
                        g.g.dest_pf = dest_pf_for_psize;
                        g.g.dp = dest_pitch;
                        g.g.dw = dest_w;
                        g.g.dh = dest_h;
                        g.g.src_addr_color = src_addr;
                        g.g.src_pf = src_pf_for_psize;
                        g.g.sp = src_pitch;
                        g.g.w = src_w;
                        g.g.h = src_h;
                        handle_scale_blit(g.g);
                    }
                    break;

                case ClearScreen:
                    // shouldn't get here
                    break;
            }
#if GPU_DEBUG
            {
                klog("gpu: @%u complete\n",
                    (unsigned int)clock_cur_us());
            }
#endif
        }

    }
}

void GPUEnqueueFBColor(uint32_t c)
{
    GPUEnqueueMessage(GPUMessageFBColor(c));
}

void GPUEnqueueBlitRectangle(void *src, int x, int y, int width, int height, int dest_x, int dest_y)
{
    GPUEnqueueMessage(GPUMessageBlitRectangle(src, x, y, width, height, dest_x, dest_y));
}

void GPUEnqueueFlip()
{
    GPUEnqueueMessage(GPUMessageFlip());
}

void GPUEnqueueMessage(const gpu_message &g)
{
    gpu_message_int gmsg { .g = g, .sending_thread = GetCurrentThreadForCore() };
    gpu_msg_list.Push(gmsg);
}

bool GPUBusy()
{
    return !gpu_msg_list.empty();
}

size_t GPUEnqueueMessages(const gpu_message *msgs, size_t nmsg)
{
    size_t nsent = 0;
    if(!msgs) return nsent;
    auto cpsr = DisableInterrupts();
    for(size_t i = 0; i < nmsg; i++)
    {
        auto t = GetCurrentThreadForCore();
        gpu_message_int msg { .g = msgs[i], .sending_thread = t };
        if(!gpu_msg_list.Push(msg))
        {
            RestoreInterrupts(cpsr);
            return nsent;
        }
        nsent++;
    }
    RestoreInterrupts(cpsr);
    return nsent;
}

extern "C" void DMA2D_IRQHandler()
{
#if GPU_DEBUG
    klog("gpuint: @%u\n", clock_cur_us());
#endif
    DMA2D->IFCR = DMA2D_IFCR_CTCIF | DMA2D_IFCR_CTEIF;
    gpu_ready.Signal();
}

/* static void mdma_handler()
{
    mdma->CIFCR = MDMA_CIFCR_CCTCIF;
    mdma_ready.Signal();
} */
