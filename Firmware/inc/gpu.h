#ifndef GPU_H
#define GPU_H

#include <cstdint>

enum gpu_message_type
{
    FlipBuffers,
    BlitColor,
    BlitImage
};

struct gpu_message
{
    gpu_message_type type;
    uint32_t dest_addr;
    uint32_t src_addr_color;
    uint32_t dest_pf;
    uint32_t src_pf;
    uint32_t nlines, row_width, row_stride;
    bool dest_fbuf_relative;
};

void GPUEnqueueMessage(const gpu_message &msg);
void GPUEnqueueFlip();
void GPUEnqueueFBColor(uint32_t col);

#endif
