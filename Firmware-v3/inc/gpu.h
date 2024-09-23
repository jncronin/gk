#ifndef GPU_H
#define GPU_H

#include <cstdint>
#include <cstddef>

#include "_gk_gpu.h"

void GPUEnqueueMessage(const gpu_message &msg);
size_t GPUEnqueueMessages(const gpu_message *msgs, size_t nmsg);
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
    g.dest_pf = 0;
    g.dx = 0;
    g.dy = 0;
    g.w = 640;
    g.h = 480;
    g.src_addr_color = c;
    return g;
}

constexpr gpu_message GPUMessageBlitRectangle(void *src, int x, int y, int width, int height, int dest_x, int dest_y)
{
    gpu_message g = {};
    g.type = gpu_message_type::BlitImage;
    g.dest_addr = 0;
    g.dest_pf = 0;
    g.dx = dest_x;
    g.dy = dest_y;
    g.w = width;
    g.h = height;
    g.src_addr_color = (uint32_t)(uintptr_t)src;
    g.src_pf = 0;
    g.sp = 640 * 4;
    g.sx = x;
    g.sy = y;
    return g;
}

void *gpu_thread(void *p);

#endif
