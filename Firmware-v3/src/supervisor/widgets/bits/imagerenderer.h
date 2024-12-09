template<unsigned int x_scale = 1, unsigned int y_scale = 1>
void RenderImage(coord_t x, coord_t y, coord_t w, coord_t h,
            coord_t img_w, coord_t img_h,
            HOffset hoffset, VOffset voffset,
            const color_t *img, color_t bg_color, alpha_t alpha)
{
    auto fb = (color_t *)screen_get_overlay_frame_buffer();

    switch(voffset)
    {
        case Top:
            break;
        case Middle:
            y = y + (h/2) - (img_h/2);
            break;
        case Bottom:
            y = y + h - img_h;
            break;
    }
    switch(hoffset)
    {
        case Left:
            break;
        case Centre:
            x = x + w/2 - (img_w/2);
            break;
        case Right:
            x = x + w - img_w;
            break;
    }

    for(coord_t cy = 0; cy < img_h; cy++)
    {
        for(coord_t cx = 0; cx < img_w; cx++)
        {
            auto actx = cx + x;
            auto acty = cy + y;
            if(actx < 0 || actx >= fb_w || acty < 0 || acty >= fb_h)
                continue;

            auto cv = MultiplyAlpha(img[cx + cy * img_w], alpha);
            fb[actx / x_scale + acty / y_scale * fb_stride / x_scale] = DoBlend(cv, bg_color);
        }
    }
}
