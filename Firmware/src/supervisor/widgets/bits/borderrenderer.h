template <unsigned int x_scale = 1, unsigned int y_scale = 1>
void RenderBorder(coord_t x, coord_t y, coord_t w, coord_t h,
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
            fb[cx / x_scale + cy / y_scale * fb_stride / x_scale] = _border_color;
        }
    }

    // bottom
    for(coord_t cy = y + h - border_width; cy < y + h; cy++)
    {
        for(coord_t cx = x; cx < x + w; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx / x_scale + cy / y_scale * fb_stride / x_scale] = _border_color;
        }
    }

    // left
    for(coord_t cy = y + border_width; cy < y + h - border_width; cy++)
    {
        for(coord_t cx = x; cx < x + border_width; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx / x_scale + cy / y_scale * fb_stride / x_scale] = _border_color;
        }
    }

    // right
    for(coord_t cy = y + border_width; cy < y + h - border_width; cy++)
    {
        for(coord_t cx = x + w - border_width; cx < x + w; cx++)
        {
            if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                continue;
            fb[cx / x_scale + cy / y_scale * fb_stride / x_scale] = _border_color;
        }
    }
}
