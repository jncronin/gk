#include "widget.h"

void ButtonWidget::Update(alpha_t alpha)
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    color_t bg_color, border_color, text_color;

    if(is_clicked || is_pretend_clicked)
    {
        bg_color = bg_clicked_color;
        border_color = border_clicked_color;
        text_color = text_clicked_color;
    }
    else if(IsHighlighted())
    {
        bg_color = bg_highlight_color;
        border_color = border_highlight_color;
        text_color = text_highlight_color;
    }
    else
    {
        bg_color = bg_inactive_color;
        border_color = border_inactive_color;
        text_color = text_inactive_color;
    }

    switch(screen_get_hardware_scale_horiz())
    {
        case x1:
            RenderBackground(actx, acty, w, h, bg_color, alpha);
            RenderBorder(actx, acty, w, h, border_color, border_width, alpha);
            RenderText(actx, acty, w, h, text, text_color, bg_color, text_hoffset, text_voffset, Font, alpha);
            break;
        case x2:
            RenderBackground<2>(actx, acty, w, h, bg_color, alpha);
            RenderBorder<2>(actx, acty, w, h, border_color, border_width, alpha);
            RenderText<2>(actx, acty, w, h, text, text_color, bg_color, text_hoffset, text_voffset, Font, alpha);
            break;
        case x4:
            RenderBackground<4>(actx, acty, w, h, bg_color, alpha);
            RenderBorder<4>(actx, acty, w, h, border_color, border_width, alpha);
            RenderText<4>(actx, acty, w, h, text, text_color, bg_color, text_hoffset, text_voffset, Font, alpha);
            break;
    }
}

void ImageButtonWidget::Update(alpha_t alpha)
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    color_t bg_color, border_color;

    if(is_clicked || is_pretend_clicked)
    {
        bg_color = bg_clicked_color;
        border_color = border_clicked_color;
    }
    else if(IsHighlighted())
    {
        bg_color = bg_highlight_color;
        border_color = border_highlight_color;
    }
    else
    {
        bg_color = bg_inactive_color;
        border_color = border_inactive_color;
    }

    switch(screen_get_hardware_scale_horiz())
    {
        case x1:
            RenderBackground(actx, acty, w, h, bg_color, alpha);
            RenderBorder(actx, acty, w, h, border_color, border_width, alpha);
            RenderImage(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color, alpha);
            break;
        case x2:
            RenderBackground<2>(actx, acty, w, h, bg_color, alpha);
            RenderBorder<2>(actx, acty, w, h, border_color, border_width, alpha);
            RenderImage<2>(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color, alpha);
            break;
        case x4:
            RenderBackground<4>(actx, acty, w, h, bg_color, alpha);
            RenderBorder<4>(actx, acty, w, h, border_color, border_width, alpha);
            RenderImage<4>(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color, alpha);
            break;
    }
}
