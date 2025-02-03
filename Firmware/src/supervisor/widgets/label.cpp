#include "widget.h"

void LabelWidget::Update(alpha_t alpha)
{
    if(!visible) return;
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    switch(screen_get_hardware_scale_horiz())
    {
        case x1:
            RenderBackground(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBorder(actx, acty, w, h, border_inactive_color, border_width, alpha);
            RenderText(actx, acty, w, h, text, text_inactive_color, bg_inactive_color, text_hoffset, text_voffset, Font, alpha);
            break;
        case x2:
            RenderBackground<2>(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBorder<2>(actx, acty, w, h, border_inactive_color, border_width, alpha);
            RenderText<2>(actx, acty, w, h, text, text_inactive_color, bg_inactive_color, text_hoffset, text_voffset, Font, alpha);
            break;
        case x4:
            RenderBackground<4>(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBorder<4>(actx, acty, w, h, border_inactive_color, border_width, alpha);
            RenderText<4>(actx, acty, w, h, text, text_inactive_color, bg_inactive_color, text_hoffset, text_voffset, Font, alpha);
            break;
    }
}
