#if 0

#include "../lvgl-9.1.0/src/core/lv_refr.h"
#include "../lvgl-9.1.0/src/stdlib/lv_string.h"
#include "../lvgl-9.1.0/src/core/lv_global.h"
#include "../lvgl-9.1.0/src/lv_init.h"

#include "process.h"
#include "screen.h"
#include "gpu.h"

extern Process *focus_process;

struct gk_display_data
{
    struct gpu_message *cmsgs;
    unsigned int cmsgs_size;
    unsigned int cmsgs_count;
    bool is_first;
    unsigned int gkpf;
    void *next_buffer;
    size_t w;
    size_t h;
    size_t stride;
};

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p);
struct gpu_message *get_next_msg(struct gk_display_data *dd);
static void release_disp_cb(lv_event_t * e);
static uint32_t get_ticks();

static const int nmsgs_start = 8;

lv_display_t *lv_gk_display_create()
{
    size_t w, h, stride;

    w = focus_process->screen_w;
    h = focus_process->screen_h;

    struct gk_display_data *dd = (gk_display_data *)malloc(sizeof(struct gk_display_data));
    if(dd == NULL) return NULL;
    memset(dd, 0, sizeof(gk_display_data));
    dd->cmsgs_size = nmsgs_start;
    dd->cmsgs_count = 0;
    dd->cmsgs = (gpu_message *)malloc(dd->cmsgs_size * sizeof(struct gpu_message));
    dd->gkpf = 6;
    if(dd->cmsgs == NULL)
    {
        lv_free(dd);
        return NULL;
    }

    lv_display_t *disp = lv_display_create(w, h);
    if(disp == NULL)
    {
        lv_free(dd->cmsgs);
        lv_free(dd);
        return NULL;
    }

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_AL44);
    stride = w;

    {
        /* Get first framebuffer */
        lv_display_set_buffers(disp, screen_get_overlay_frame_buffer(), NULL, h * stride, LV_DISPLAY_RENDER_MODE_DIRECT);
    }

    dd->is_first = true;
    dd->w = w;
    dd->h = h;
    dd->stride = stride;

    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, release_disp_cb, LV_EVENT_DELETE, disp);
    lv_display_set_driver_data(disp, dd);

    lv_tick_set_cb(get_ticks);
    
    return disp;
}

void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p)
{
    struct gk_display_data *dd = (gk_display_data *)lv_display_get_driver_data(disp);

    if(dd->is_first)
    {
        // Add a flip buffers message first
        struct gpu_message *gmsg = get_next_msg(dd);
        if(!gmsg)
            return;
        gmsg->type = FlipBuffers;
        gmsg->dest_addr = (uint32_t)(uintptr_t)&dd->next_buffer;
        gmsg->src_addr_color = 0;
        dd->is_first = false;
    }

    /* Add area, if any */
    if(area)
    {
        struct gpu_message *cgmsg = get_next_msg(dd);
        if(!cgmsg)
            return;
        cgmsg->type = BlitImage;
        cgmsg->dest_addr = 0;
        cgmsg->dest_pf = dd->gkpf;
        cgmsg->dp = dd->stride;
        cgmsg->dx = area->x1;
        cgmsg->dy = area->y1;
        cgmsg->dw = area->x2 - area->x1 + 1;
        cgmsg->dh = area->y2 - area->y1 + 1;
        cgmsg->src_addr_color = 0;
        cgmsg->src_pf = 0;
        cgmsg->sp = 0;
        cgmsg->sx = cgmsg->dx;
        cgmsg->sy = cgmsg->dy;
        cgmsg->w = cgmsg->dw;
        cgmsg->h = cgmsg->dh;
    }
    else
    {
        /* not sure if this occurs, but just copy all anyway */
        struct gpu_message *cgmsg = get_next_msg(dd);
        if(!cgmsg)
            return;
        cgmsg->type = BlitImage;
        cgmsg->dest_addr = 0;
        cgmsg->dest_pf = dd->gkpf;
        cgmsg->dp = dd->stride;
        cgmsg->dx = 0;
        cgmsg->dy = 0;
        cgmsg->dw = dd->w;
        cgmsg->dh = dd->h;
        cgmsg->src_addr_color = 0;
        cgmsg->src_pf = 0;
        cgmsg->sp = 0;
        cgmsg->sx = 0;
        cgmsg->sy = 0;
        cgmsg->w = dd->w;
        cgmsg->h = dd->h;
    }

    /* If last update in the frame, add a signal thread and return */
    if(lv_display_flush_is_last(disp))
    {
        struct gpu_message *cgmsg = get_next_msg(dd);
        if(!cgmsg)
            return;
        cgmsg->type = SignalThread;
        cgmsg->dest_addr = 0;
        cgmsg->src_addr_color = 0;

        GPUEnqueueMessages(dd->cmsgs, dd->cmsgs_count);
        GetCurrentThreadForCore()->ss.Wait(SimpleSignal::Set, 0);

        /* reset ready for next frame */
        dd->cmsgs_count = 0;

        lv_display_set_buffers(disp, dd->next_buffer, NULL,
            dd->stride * dd->h, LV_DISPLAY_RENDER_MODE_DIRECT);

        dd->is_first = true;
    }

    lv_display_flush_ready(disp);
}

struct gpu_message *get_next_msg(struct gk_display_data *dd)
{
    /* may need to realloc the buffer here */
    while(dd->cmsgs_count >= dd->cmsgs_size)
    {
        auto new_cmsgs = realloc(dd->cmsgs, dd->cmsgs_size * 2);
        if(new_cmsgs)
        {
            dd->cmsgs_size *= 2;
        }
        else
        {
            GPUEnqueueMessages(dd->cmsgs, dd->cmsgs_count);
            dd->cmsgs_count = 0;
        }
    }
    if(dd->cmsgs == NULL)
    {
        return NULL;
    }
    return &dd->cmsgs[dd->cmsgs_count++];
}

void release_disp_cb(lv_event_t *e)
{
    lv_display_t *disp = (lv_display_t *)lv_event_get_user_data(e);
    struct gk_display_data *dd = (gk_display_data *)lv_display_get_driver_data(disp);
    if(dd)
    {
        if(dd->cmsgs)
            lv_free(dd->cmsgs);
        lv_free(dd);
    }
    lv_display_set_driver_data(disp, NULL);
}

uint32_t get_ticks()
{
    return clock_cur_ms();
}

#endif
