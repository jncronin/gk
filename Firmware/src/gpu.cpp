#include <stm32h7xx.h>
#include "gpu.h"
#include "osqueue.h"
#include "screen.h"
#include "cache.h"

__attribute__((section (".sram4"))) static BinarySemaphore gpu_ready;
__attribute__((section (".sram4"))) static FixedQueue<gpu_message, 8> gpu_msg_list;

extern Spinlock s_rtt;
#include "SEGGER_RTT.h"

extern Condition scr_vsync;

#define GPU_DEBUG 0

static inline void wait_dma2d()
{
    while(DMA2D->CR & DMA2D_CR_START)
    {
        gpu_ready.Wait(clock_cur_ms() + 20ULL);
    }
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
        case GK_PIXELFORMAT_L8:
            return col & 0xffU;
        default:
            return 0;
    }
}

void *gpu_thread(void *p)
{
    (void)p;

    RCC->AHB3ENR |= RCC_AHB3ENR_DMA2DEN;
    (void)RCC->AHB3ENR;

    NVIC_EnableIRQ(DMA2D_IRQn);

    while(true)
    {
        gpu_message g;
        if(!gpu_msg_list.Pop(&g))
            continue;


#if GPU_DEBUG
        auto ltdc_curfb = LTDC_Layer1->CFBAR;
        auto scr_curfb = (uint32_t)(uintptr_t)screen_get_frame_buffer();
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "gpu: @%u type: %d, dest_addr: %x, src_addr_color: %x, nlines: %x, row_width: %x\n",
                (unsigned int)clock_cur_ms(), g.type, g.dest_addr, g.src_addr_color, g.h, g.w);
            SEGGER_RTT_printf(0, "gpu: ltdc_fb: %x, scr_fb: %x\n", ltdc_curfb, scr_curfb);
        }
#endif

        // We should never be able to write to the current framebuffer
        {
            while(LTDC_Layer1->CFBAR == (uint32_t)(uintptr_t)screen_get_frame_buffer())
                Yield();
        }

        /* get details on pixel formats, strides etc */
        uint32_t dest_pf = g.dest_addr ? g.dest_pf : focus_process->screen_mode;
        int bpp = get_bpp(dest_pf);
        //uint32_t scr_fbuf = (uint32_t)(uintptr_t)screen_get_frame_buffer();
        uint32_t dest_addr = g.dest_addr ? g.dest_addr : (uint32_t)(uintptr_t)screen_get_frame_buffer();
        uint32_t dest_pitch = g.dest_addr ? g.dp : 640 * bpp;


        
        switch(g.type)
        {
            case gpu_message_type::FlipBuffers:
                wait_dma2d();
                screen_flip();
                scr_vsync.Wait();
                break;

            case gpu_message_type::SetBuffers:
                screen_set_frame_buffer((void *)g.dest_addr, (void*)g.src_addr_color, g.dest_pf);
                break;

            case gpu_message_type::CleanCache:
                {
                    auto start_addr = dest_addr + g.dx * bpp + g.dy * dest_pitch;
                    auto len = g.h * dest_pitch + g.w * bpp;
                    CleanM7Cache(start_addr, len, CacheType_t::Data);
                }
                break;

            case gpu_message_type::SignalThread:
                wait_dma2d();
                {
                    auto t = (Thread *)g.dest_addr;
                    {
                        CriticalGuard cg(t->sl);
                        t->ss_p.ival1 = 0;
                        t->ss.Signal();
                    }
                }
                break;

            case gpu_message_type::BlitColor:
                if(!g.w || !g.h)
                    break;
                wait_dma2d();
                DMA2D->OPFCCR = dest_pf;
                DMA2D->OCOLR = color_encode(g.src_addr_color, dest_pf);
                DMA2D->OMAR = dest_addr + g.dx * bpp + g.dy * dest_pitch;
                DMA2D->OOR = (dest_pitch / bpp) - g.w;
                DMA2D->NLR = (g.w << DMA2D_NLR_PL_Pos) | (g.h << DMA2D_NLR_NL_Pos);
                DMA2D->CR = DMA2D_CR_TCIE |
                    DMA2D_CR_TEIE |
                    (3UL << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                break;

            case gpu_message_type::BlitImage:
                if(!g.w || !g.h)
                    break;
                wait_dma2d();
                DMA2D->OPFCCR = dest_pf;
                DMA2D->OMAR = dest_addr + g.dx * bpp + g.dy * dest_pitch;
                DMA2D->OOR = (dest_pitch / bpp) - g.w;
                DMA2D->NLR = (g.w << DMA2D_NLR_PL_Pos) | (g.h << DMA2D_NLR_NL_Pos);

                {
                    // configure source, +/- pixel format correction
                    auto src_bpp = get_bpp(g.src_pf);
                    DMA2D->FGMAR = g.src_addr_color + g.sx * src_bpp + g.sy * g.sp;
                    DMA2D->FGPFCCR = g.src_pf;
                    DMA2D->FGOR = (g.sp / src_bpp) - g.w;

                    uint32_t mode = 0;
                    if(g.src_pf != dest_pf)
                        mode = 1U;
                    DMA2D->CR = DMA2D_CR_TCIE | 
                        DMA2D_CR_TEIE |
                        (mode << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                }
                break;
        }
#if GPU_DEBUG
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "gpu: @%u complete\n",
                (unsigned int)clock_cur_ms());
        }
#endif

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
    gpu_msg_list.Push(g);
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
        gpu_message msg = msgs[i];
        if(msg.type == gpu_message_type::SignalThread)
        {
            msg.dest_addr = (uint32_t)(uintptr_t)GetCurrentThreadForCore();
        }
        // TODO check sending thread has focus
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
    SEGGER_RTT_printf(0, "gpuint: @%u\n", clock_cur_ms());
#endif
    DMA2D->IFCR = DMA2D_IFCR_CTCIF | DMA2D_IFCR_CTEIF;
    gpu_ready.Signal();
}
