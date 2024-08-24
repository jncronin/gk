#include "widget.h"

void LabelWidget::Update(alpha_t alpha)
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);
    RenderBackground(actx, acty, w, h, bg_inactive_color, alpha);
    RenderBorder(actx, acty, w, h, border_inactive_color, border_width, alpha);
    RenderText(actx, acty, w, h, text, text_inactive_color, bg_inactive_color, text_hoffset, text_voffset, Font, alpha);
}
