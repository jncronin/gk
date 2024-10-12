#include "widget.h"
#include "screen.h"

void RectangleWidget::Update(alpha_t alpha)
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);
    RenderBackground(actx, acty, w, h, bg_inactive_color, alpha);
    RenderBorder(actx, acty, w, h, border_inactive_color, border_width, alpha);
}
