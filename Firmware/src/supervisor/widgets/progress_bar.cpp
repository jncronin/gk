#include "widget.h"

void ProgressBarWidget::Update(alpha_t alpha)
{
    coord_t actx, acty;
    GetAbsolutePosition(&actx, &acty);

    coord_t fg_w = 0, fg_h = 0, fg_x = 0, fg_y = 0;

    auto cv = cur_value;
    if(cv > max_value) cv = max_value;

    switch(o)
    {
        case RightToLeft:
        case LeftToRight:
            fg_w = ((w - pad - pad) * cv) / max_value;
            fg_h = h - pad - pad;
            break;
        case TopToBottom:
        case BottomToTop:
            fg_h = ((h - pad - pad) * cv) / max_value;
            fg_w = w - pad - pad;
            break;
    }

    switch(o)
    {
        case RightToLeft:
            fg_x = pad;
            fg_y = pad;
            break;
        case LeftToRight:
            fg_x = w - pad - fg_w;
            fg_y = pad;
            break;
        case TopToBottom:
            fg_x = pad;
            fg_y = pad;
            break;
        case BottomToTop:
            fg_x = pad;
            fg_y = h - pad - fg_h;
            break;
    }

    switch(screen_get_hardware_scale_horiz())
    {
        case x1:
            RenderBackground(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBackground(fg_x + actx, fg_y + acty, fg_w, fg_h, fg_inactive_color, alpha);
            RenderBorder(actx, acty, w, h, border_inactive_color, border_width, alpha);
            break;
        case x2:
            RenderBackground<2,1>(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBackground<2,1>(fg_x + actx, fg_y + acty, fg_w, fg_h, fg_inactive_color, alpha);
            RenderBorder<2,1>(actx, acty, w, h, border_inactive_color, border_width, alpha);
            break;
        case x4:
            RenderBackground<4,1>(actx, acty, w, h, bg_inactive_color, alpha);
            RenderBackground<4,1>(fg_x + actx, fg_y + acty, fg_w, fg_h, fg_inactive_color, alpha);
            RenderBorder<4,1>(actx, acty, w, h, border_inactive_color, border_width, alpha);
            break;
    }
}

ProgressBarWidget::Orientation_t ProgressBarWidget::GetOrientation()
{
    return o;
}

unsigned int ProgressBarWidget::GetCurValue()
{
    return cur_value;
}

unsigned int ProgressBarWidget::GetMaxValue()
{
    return max_value;
}

void ProgressBarWidget::SetOrientation(ProgressBarWidget::Orientation_t v)
{
    o = v;
}

void ProgressBarWidget::SetCurValue(unsigned int v)
{
    cur_value = v;
}

void ProgressBarWidget::SetMaxValue(unsigned int v)
{
    max_value = v;
}
