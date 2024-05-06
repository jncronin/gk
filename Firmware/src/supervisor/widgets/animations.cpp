#include "widget.h"
#include <list>

void AddAnimation(WidgetAnimationList &wl, unsigned long int cur_ms,
    WidgetAnimation anim, Widget *wdg, void *p)
{
    WidgetAnimation_t wa;
    wa.anim = anim;
    wa.p = p;
    wa.w = wdg;
    wa.start_time = cur_ms;
    wl.push_back(wa);
}

bool RunAnimations(WidgetAnimationList &wl, unsigned long int cur_ms)
{
    if(wl.size() == 0)
        return false;

    auto iter = wl.begin();
    while(iter != wl.end())
    {
        auto curt = cur_ms - iter->start_time;
        if(iter->anim(iter->w, iter->p, curt))
        {
            iter = wl.erase(iter);
        }
        else
        {
            iter++;
        }
    }
    return true;
}
