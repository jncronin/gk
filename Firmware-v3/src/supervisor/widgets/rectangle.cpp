#include "widget.h"
#include "screen.h"

void RectangleWidget::Update(alpha_t alpha)
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    switch(screen_get_hardware_scale_horiz())
    {
        case x1:
            RenderBackground(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBorder(actx, acty, w, h, border_inactive_color, border_width, alpha);
            break;
        case x2:
            RenderBackground<2>(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBorder<2>(actx, acty, w, h, border_inactive_color, border_width, alpha);
            break;
        case x4:
            RenderBackground<4>(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBorder<4>(actx, acty, w, h, border_inactive_color, border_width, alpha);
            break;
    }
}
