#include "widget.h"
#include "screen.h"

void BorderRenderer::RenderBorder(coord_t x, coord_t y, coord_t w, coord_t h,
            color_t border_color, coord_t border_width, alpha_t alpha)
{
    if(border_width == 0)
        return;
    
    auto fb = (color_t *)screen_get_overlay_frame_buffer();

    auto _border_color = MultiplyAlpha(border_color, alpha);
    
    // top
    for(coord_t cy = y; cy < y + border_width; cy++)
    {
        for(coord_t cx = x; cx < x + w; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx + cy * fb_stride] = _border_color;
        }
    }

    // bottom
    for(coord_t cy = y + h - border_width; cy < y + h; cy++)
    {
        for(coord_t cx = x; cx < x + w; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx + cy * fb_stride] = _border_color;
        }
    }

    // left
    for(coord_t cy = y + border_width; cy < y + h - border_width; cy++)
    {
        for(coord_t cx = x; cx < x + border_width; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx + cy * fb_stride] = _border_color;
        }
    }

    // right
    for(coord_t cy = y + border_width; cy < y + h - border_width; cy++)
    {
        for(coord_t cx = x + w - border_width; cx < x + w; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx + cy * fb_stride] = _border_color;
        }
    }
}

void BackgroundRenderer::RenderBackground(coord_t x, coord_t y, coord_t w, coord_t h,
            color_t bg_color, alpha_t alpha)
{
    auto fb = (color_t *)screen_get_overlay_frame_buffer();

    auto _bg_color = MultiplyAlpha(bg_color, alpha);
    
    for(coord_t cy = y; cy < y + h; cy++)
    {
        for(coord_t cx = x; cx < x + w; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx + cy * fb_stride] = _bg_color;
        }
    }
}
