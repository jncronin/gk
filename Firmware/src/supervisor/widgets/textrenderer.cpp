#include "widget.h"
#include "font_large.h"
#include "font_small.h"
#include "screen.h"

void TextRenderer::RenderText(coord_t x, coord_t y, coord_t w, coord_t h,
            const std::string &text,
            color_t fg_color, color_t bg_color,
            HOffset hoffset, VOffset voffset,
            int font)
{
    (void)w;
    (void)h;

    int fw, fh, fs, fbw, fo;
    const char *fontbuf;

    auto fb = (color_t *)screen_get_overlay_frame_buffer();

    switch(font)
    {
        case FONT_LARGE:
            fw = FONT_LARGE_WIDTH;
            fh = FONT_LARGE_HEIGHT;
            fs = FONT_LARGE_STRIDE;
            fontbuf = (const char *)font_large;
            break;

        case FONT_SMALL:
            fw = FONT_SMALL_WIDTH;
            fh = FONT_SMALL_HEIGHT;
            fs = FONT_SMALL_STRIDE;
            fontbuf = (const char *)font_small;
            break;

        default:
            return;
    }

    fbw = fs * 8;    // byte width
    fo = fbw - fw;  // blank space at start

    switch(voffset)
    {
        case Top:
            break;
        case Middle:
            y = y + (h/2) - (fh/2);
            break;
        case Bottom:
            y = y + h - fh;
            break;
    }
    switch(hoffset)
    {
        case Left:
            break;
        case Centre:
            x = x + w/2 - ((fw * text.length()) / 2);
            break;
        case Right:
            x = x + w - fw * text.length();
            break;
    }

    for(auto c : text)
    {
        const char *fnt = &fontbuf[(int)c * fs * fh];

        for(int fy = 0; fy < fh; fy++)
        {
            for(int fx = fo; fx < fbw; fx++)
            {
                auto cx = x + fx - fo;
                auto cy = y + fy;
                if(cy < 0 || cy >= fb_h || cx < 0 || cx >= fb_w)
                    continue;

                char f = fnt[fy * fs + fx / 8];
                if(f & (1UL << fx % 8))
                {
                    fb[cx + cy * fb_stride] = fg_color;
                }
            }
        }

        x += fw;

        c++;
    }
}
