#include <stm32h7xx.h>
#include "gpu.h"
#include "osqueue.h"
#include "screen.h"

__attribute__((section (".sram4"))) static Condition gpu_ready;
__attribute__((section (".sram4"))) static FixedQueue<gpu_message, 8> gpu_msg_list;

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
        
        switch(g.type)
        {
            case gpu_message_type::FlipBuffers:
                screen_flip();
                break;

            case gpu_message_type::BlitColor:
                DMA2D->OPFCCR = g.dest_pf;
                DMA2D->OCOLR = g.src_addr_color;
                DMA2D->OMAR = g.dest_addr + (g.dest_fbuf_relative ? (uint32_t)(uintptr_t)screen_get_frame_buffer() : 0UL);
                DMA2D->OOR = 640 - g.row_width;
                DMA2D->NLR = (g.row_width << DMA2D_NLR_PL_Pos) | (g.nlines << DMA2D_NLR_NL_Pos);
                DMA2D->CR = DMA2D_CR_TCIE | (3UL << DMA2D_CR_MODE_Pos) | DMA2D_CR_START;
                gpu_ready.Wait();
                break;

            case gpu_message_type::BlitImage:
                // TODO
                while(true);
                break;
        }
    }
}

void GPUEnqueueFBColor(uint32_t c)
{
    gpu_message g;
    g.type = gpu_message_type::BlitColor;
    g.dest_addr = 0;
    g.dest_fbuf_relative = true;
    g.dest_pf = 0;
    g.nlines = 480;
    g.row_width = 640;
    g.src_addr_color = c;
    GPUEnqueueMessage(g);
}

void GPUEnqueueFlip()
{
    gpu_message g;
    g.type = gpu_message_type::FlipBuffers;
    GPUEnqueueMessage(g);
}

void GPUEnqueueMessage(const gpu_message &g)
{
    gpu_msg_list.Push(g);
}

extern "C" void DMA2D_IRQHandler()
{
    gpu_ready.Signal();
    DMA2D->IFCR = DMA2D_IFCR_CTCIF;
}
