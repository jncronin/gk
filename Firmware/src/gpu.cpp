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
    while(DMA2D->CR & DMA2D_CR_START);
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

        auto ltdc_curfb = LTDC_Layer1->CFBAR;
        auto scr_curfb = (uint32_t)(uintptr_t)screen_get_frame_buffer();

#if GPU_DEBUG
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "gpu: @%u type: %d, dest_addr: %x, src_addr_color: %x, nlines: %x, row_width: %x\n",
                (unsigned int)clock_cur_ms(), g.type, g.dest_addr, g.src_addr_color, g.nlines, g.row_width);
            SEGGER_RTT_printf(0, "gpu: ltdc_fb: %x, scr_fb: %x\n", ltdc_curfb, scr_curfb);
        }
#endif

        // We should never be able to write to the current framebuffer
        {
            while(LTDC_Layer1->CFBAR == (uint32_t)(uintptr_t)screen_get_frame_buffer())
                Yield();
        }
        
        switch(g.type)
        {
            case gpu_message_type::FlipBuffers:
                wait_dma2d();
                screen_flip();
                scr_vsync.Wait();
                break;

            case gpu_message_type::SetBuffers:
                wait_dma2d();
                screen_set_frame_buffer((void *)g.dest_addr, (void*)g.src_addr_color, g.dest_pf);
                break;

            case gpu_message_type::CleanCache:
                wait_dma2d();
                {
                    auto start_addr = g.dest_addr + (g.dest_fbuf_relative ? (uint32_t)(uintptr_t)screen_get_frame_buffer() : 0UL);
                    auto len = g.row_width * 4 + g.nlines * 640 * 4;
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
                if(!g.row_width || !g.nlines)
                    break;
                wait_dma2d();
                DMA2D->OPFCCR = g.dest_pf;
                DMA2D->OCOLR = g.src_addr_color;
                DMA2D->OMAR = g.dest_addr + (g.dest_fbuf_relative ? (uint32_t)(uintptr_t)screen_get_frame_buffer() : 0UL);
                DMA2D->OOR = 640 - g.row_width;
                DMA2D->NLR = (g.row_width << DMA2D_NLR_PL_Pos) | (g.nlines << DMA2D_NLR_NL_Pos);
                DMA2D->CR = DMA2D_CR_TCIE |
                    DMA2D_CR_TEIE |
                    (3UL << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                gpu_ready.Wait(clock_cur_ms() + 20ULL);
                break;

            case gpu_message_type::BlitImage:
                if(!g.row_width || !g.nlines)
                    break;
                wait_dma2d();
                DMA2D->OPFCCR = g.dest_pf;
                DMA2D->OMAR = g.dest_addr + (g.dest_fbuf_relative ? (uint32_t)(uintptr_t)screen_get_frame_buffer() : 0UL);
                DMA2D->OOR = 640 - g.row_width;
                DMA2D->NLR = (g.row_width << DMA2D_NLR_PL_Pos) | (g.nlines << DMA2D_NLR_NL_Pos);
                DMA2D->FGMAR = g.src_addr_color;
                DMA2D->FGPFCCR = 0;
                DMA2D->FGOR = 640 - g.row_width;
                DMA2D->CR = DMA2D_CR_TCIE | 
                    DMA2D_CR_TEIE |
                    (0UL << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                gpu_ready.Wait(clock_cur_ms() + 20ULL);
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
