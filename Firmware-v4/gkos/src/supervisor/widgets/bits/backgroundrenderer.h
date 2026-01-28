template <unsigned int x_scale = 1, unsigned int y_scale = 1>
void RenderBackground(coord_t x, coord_t y, coord_t w, coord_t h,
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
            fb[cx / x_scale + cy / y_scale * fb_stride / x_scale] = _bg_color;
        }
    }
}
