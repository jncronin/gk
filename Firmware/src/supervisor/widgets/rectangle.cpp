#include "widget.h"
#include "screen.h"

void RectangleWidget::Update()
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);
    RenderBackground(actx, acty, w, h, bg_inactive_color);
    RenderBorder(actx, acty, w, h, border_inactive_color, border_width);
}
