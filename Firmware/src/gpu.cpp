#include <stm32h7xx.h>
#include "gpu.h"
#include "osqueue.h"
#include "screen.h"

__attribute__((section (".sram4"))) static Condition gpu_ready;
__attribute__((section (".sram4"))) static FixedQueue<gpu_message, 8> gpu_msg_list;

extern Spinlock s_rtt;
#include "SEGGER_RTT.h"

void gpu_thread(void *p)
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

#ifdef GPU_DEBUG
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "gpu: type: %d, dest_addr: %x, src_addr_color: %x\n",
                g.type, g.dest_addr, g.src_addr_color);
        }
#endif
        
        switch(g.type)
        {
            case gpu_message_type::FlipBuffers:
                screen_flip();
                break;

            case gpu_message_type::BlitColor:
                if(!g.row_width || !g.nlines)
                    break;
                DMA2D->OPFCCR = g.dest_pf;
                DMA2D->OCOLR = g.src_addr_color;
                DMA2D->OMAR = g.dest_addr + (g.dest_fbuf_relative ? (uint32_t)(uintptr_t)screen_get_frame_buffer() : 0UL);
                DMA2D->OOR = 640 - g.row_width;
                DMA2D->NLR = (g.row_width << DMA2D_NLR_PL_Pos) | (g.nlines << DMA2D_NLR_NL_Pos);
                DMA2D->CR = DMA2D_CR_TCIE |
                    DMA2D_CR_TEIE |
                    (3UL << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                gpu_ready.Wait();
                break;

            case gpu_message_type::BlitImage:
                if(!g.row_width || !g.nlines)
                    break;
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
                gpu_ready.Wait();
                break;
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
    gpu_msg_list.Push(g);
}

bool GPUBusy()
{
    return !gpu_msg_list.empty();
}

void GPUEnqueueMessages(const gpu_message *msgs, size_t nmsg)
{
    if(!msgs) return;
    auto cpsr = DisableInterrupts();
    for(size_t i = 0; i < nmsg; i++)
    {
        if(!gpu_msg_list.Push(msgs[i]))
        {
            RestoreInterrupts(cpsr);
            return;
        }
    }
    RestoreInterrupts(cpsr);
    return;
}

extern "C" void DMA2D_IRQHandler()
{
    gpu_ready.Signal();
    DMA2D->IFCR = DMA2D_IFCR_CTCIF | DMA2D_IFCR_CTEIF;
}
