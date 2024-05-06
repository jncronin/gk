#include "widget.h"

void ButtonWidget::Update()
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    color_t bg_color, border_color, text_color;

    if(is_clicked)
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

    RenderBackground(actx, acty, w, h, bg_color);
    RenderBorder(actx, acty, w, h, border_color, border_width);
    RenderText(actx, acty, w, h, text, text_color, bg_color, text_hoffset, text_voffset, Font);
}

void ImageButtonWidget::Update()
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    color_t bg_color, border_color;

    if(is_clicked)
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

    RenderBackground(actx, acty, w, h, bg_color);
    RenderBorder(actx, acty, w, h, border_color, border_width);
    RenderImage(actx, acty, w, h, img_w, img_h, img_hoffset, img_voffset, image, bg_color);
}
