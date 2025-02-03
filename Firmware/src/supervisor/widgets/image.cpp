#include "widget.h"

void ImageWidget::Update(alpha_t alpha)
{
    if(!visible) return;
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    color_t bg_color, border_color;

    bg_color = bg_inactive_color;
    border_color = border_inactive_color;

    switch(screen_get_hardware_scale_horiz())
    {
        case x1:
            RenderBackground(actx, acty, w, h, bg_color, alpha);
            RenderBorder(actx, acty, w, h, border_color, border_width, alpha);
            RenderImage(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color, alpha, img_bpp, img_color);
            break;
        case x2:
            RenderBackground<2>(actx, acty, w, h, bg_color, alpha);
            RenderBorder<2>(actx, acty, w, h, border_color, border_width, alpha);
            RenderImage<2>(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color, alpha, img_bpp, img_color);
            break;
        case x4:
            RenderBackground<4>(actx, acty, w, h, bg_color, alpha);
            RenderBorder<4>(actx, acty, w, h, border_color, border_width, alpha);
            RenderImage<4>(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color, alpha, img_bpp, img_color);
            break;
    }
}
