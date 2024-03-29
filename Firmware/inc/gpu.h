#ifndef GPU_H
#define GPU_H

#include <cstdint>
#include <cstddef>

#include "_gk_gpu.h"

void GPUEnqueueMessage(const gpu_message &msg);
void GPUEnqueueMessages(const gpu_message *msgs, size_t nmsg);
void GPUEnqueueFlip();
void GPUEnqueueFBColor(uint32_t col);
void GPUEnqueueBlitRectangle(void *src, int x, int y, int width, int height, int dest_x, int dest_y);
bool GPUBusy();

constexpr gpu_message GPUMessageFlip()
{
    gpu_message g = {};
    g.type = gpu_message_type::FlipBuffers;
    return g;
}

constexpr gpu_message GPUMessageFBColor(uint32_t c)
{
    gpu_message g = {};
    g.type = gpu_message_type::BlitColor;
    g.dest_addr = 0;
    g.dest_fbuf_relative = true;
    g.dest_pf = 0;
    g.nlines = 480;
    g.row_width = 640;
    g.src_addr_color = c;
    return g;
}

constexpr gpu_message GPUMessageBlitRectangle(void *src, int x, int y, int width, int height, int dest_x, int dest_y)
{
    gpu_message g = {};
    g.type = gpu_message_type::BlitImage;
    g.dest_addr = 4 * (dest_x + dest_y * 640);
    g.dest_fbuf_relative = true;
    g.dest_pf = 0;
    g.src_addr_color = (uint32_t)(uintptr_t)src + 4 * (x + y * 640);
    g.src_pf = 0;
    g.nlines = height;
    g.row_width = width;
    return g;
}

void *gpu_thread(void *p);

#endif
