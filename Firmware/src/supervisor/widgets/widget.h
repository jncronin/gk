#ifndef WIDGET_H
#define WIDGET_H

#include <string>
#include <vector>
#include <list>
#include <limits>

#include "screen.h"
#include "font_large.h"
#include "font_small.h"

typedef unsigned char color_t;
typedef unsigned char alpha_t;
typedef short int coord_t;
typedef unsigned long long int time_ms_t;
constexpr const coord_t fb_w = 640;
constexpr const coord_t fb_h = 480;
constexpr const unsigned int fb_stride = fb_w * sizeof(color_t);

constexpr const color_t default_inactive_bg_color = 0xd4;
constexpr const color_t default_clicked_bg_color = 0xd1;
constexpr const color_t default_highlight_bg_color = 0xd4;

constexpr const color_t default_inactive_text_color = 0xff;
constexpr const color_t default_clicked_text_color = 0xff;
constexpr const color_t default_highlight_text_color = 0xff;

constexpr const color_t default_inactive_border_color = 0xf0;
constexpr const color_t default_clicked_border_color = 0xff;
constexpr const color_t default_highlight_border_color = 0xff;

constexpr const color_t default_inactive_fg_color = 0xd1;
constexpr const color_t default_clicked_fg_color = 0xdf;
constexpr const color_t default_highlight_fg_color = 0xd1;

constexpr const coord_t default_border_width = 8;

/* blending */
color_t DoBlend(color_t fg, color_t bg);
template <typename Tc, typename Ta> Tc MultiplyAlpha(Tc col, Ta alp);

enum HOffset { Left, Centre, Right };
enum VOffset { Top, Middle, Bottom };

class Widget
{
    public:
        virtual void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max()) = 0;

        virtual void StartHover();
        virtual void EndHover();

        virtual void Activate();
        virtual void Deactivate();

        virtual bool CanActivate();
        virtual bool CanClick();
        virtual bool CanHighlight();

        virtual bool IsActivated();

        virtual bool HandleMove(int x, int y);

        int x, y, w, h;
        int ch_x, ch_y;
        void *d;

        Widget *parent;

        void (*OnClick)(Widget *w, coord_t x, coord_t y) = nullptr;
        void (*OnClickBegin)(Widget *w, coord_t x, coord_t y) = nullptr;

        void GetAbsolutePosition(coord_t *x, coord_t *y);

        virtual bool IsHighlighted();
        virtual bool IsChildHighlighted(const Widget &child);

        virtual void KeyPressDown(unsigned short scancode);
        virtual void KeyPressUp(unsigned short  scancode);

        virtual void SetClickedAppearance(bool v);

    protected:
        bool is_clicked = false;
        bool is_pretend_clicked = false;

        bool new_hover = false;
        bool new_activated = false;
};

// Animations
typedef bool (*WidgetAnimation)(Widget *wdg, void *p, time_ms_t ms_since_start);
struct WidgetAnimation_t
{
    WidgetAnimation anim;
    Widget *w;
    void *p;
    time_ms_t start_time;
};
using WidgetAnimationList = std::list<WidgetAnimation_t>;

bool RunAnimations(WidgetAnimationList &wl, time_ms_t cur_ms);
bool HasAnimations(WidgetAnimationList &wl);
void AddAnimation(WidgetAnimationList &wl, time_ms_t cur_ms,
    WidgetAnimation anim, Widget *wdg, void *p);
WidgetAnimationList *GetAnimationList();
template<typename T> T Anim_Interp_Linear(T from, T to, time_ms_t t_into, time_ms_t t_tot)
{
    if(t_into >= t_tot)
        return to;
    
    // integer maths for now, need to get t_into/t_tot as signed otherwise will not support negative movements
    auto t_x_256 = (int)((t_into * 256) / t_tot);
    return from + (to - from) * t_x_256 / 256;
}

// Derived widget classes

class ActivatableWidget : public Widget
{
    public:
        bool CanActivate();
};

class NonactivatableWidget : public Widget
{
    public:
        bool CanActivate();
};

class ClickableWidget : public ActivatableWidget
{
    public:
        bool CanClick();
        virtual bool CanHighlight();
        virtual void KeyPressDown(unsigned short  key);
        virtual void KeyPressUp(unsigned short  key);
};

class TextRenderer
{
    public:
        const static int FONT_LARGE = 0;
        const static int FONT_SMALL = 1;

    protected:
#include "bits/textrenderer.h"
};

class BorderRenderer
{
    protected:
#include "bits/borderrenderer.h"
};

class BackgroundRenderer
{
    protected:
#include "bits/backgroundrenderer.h"
};

class ImageRenderer
{
    protected:
#include "bits/imagerenderer.h"
};

class DynamicTextProvider
{
    public:
        const std::string &(*GetCurrentText)();
};

class StaticBorderProvider
{
    public:
        coord_t border_width = default_border_width;
        color_t border_inactive_color = default_inactive_border_color;
        color_t border_clicked_color = default_clicked_border_color;
        color_t border_highlight_color = default_highlight_border_color;
};

class StaticBackgroundProvider
{
    public:
        color_t bg_inactive_color = default_inactive_bg_color;
        color_t bg_clicked_color = default_clicked_bg_color;
        color_t bg_highlight_color = default_highlight_bg_color;
};

class StaticForegreoundProvider
{
    public:
        color_t fg_inactive_color = default_inactive_fg_color;
        color_t fg_clicked_color = default_clicked_fg_color;
        color_t fg_highlight_color = default_highlight_fg_color;
};

class StaticTextProvider
{
    public:
        std::string text;
        color_t text_inactive_color = default_inactive_text_color;
        color_t text_clicked_color = default_clicked_text_color;
        color_t text_highlight_color = default_highlight_text_color;
        HOffset text_hoffset = HOffset::Centre;
        VOffset text_voffset = VOffset::Middle;
        int Font = TextRenderer::FONT_LARGE;
};

class StaticImageProvider
{
    public:
        const color_t *image;
        coord_t img_w, img_h;
        HOffset img_hoffset = HOffset::Centre;
        VOffset img_voffset = VOffset::Middle;
        unsigned int img_bpp = 0;
        color_t img_color = default_inactive_text_color;
};

class RectangleWidget : public NonactivatableWidget, public BorderRenderer, public BackgroundRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider
{
    public:
        void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());
};

class LabelWidget : public NonactivatableWidget, public BorderRenderer, public BackgroundRenderer,
    public TextRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticTextProvider
{
    public:
        void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());
};

class ButtonWidget : public ClickableWidget, public BorderRenderer, public BackgroundRenderer,
    public TextRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticTextProvider
{
    public:
        void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());
};

class ImageButtonWidget : public ClickableWidget, public BorderRenderer, public BackgroundRenderer,
    public ImageRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticImageProvider
{
    public:
        void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());
};

class ImageWidget : public NonactivatableWidget,
    public BorderRenderer, public BackgroundRenderer, public ImageRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticImageProvider
{
    public:
        void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());
};

class ContainerWidget : public NonactivatableWidget
{
    public:
        virtual void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());
        virtual Widget *GetHighlightedChild() = 0;
        virtual void SetHighlightedChild(const Widget &child) = 0;
        virtual bool CanHighlight();
        void AddChild(Widget &child);
        virtual void RemoveChild(Widget &child);
        void ScrollTo(coord_t x_scroll, coord_t y_scroll);
    
    protected:
        std::vector<Widget *> children;
        ContainerWidget() {}
        void ScrollToSelected();
};

class GridWidget : public ContainerWidget
{
    public:
        void AddChildOnGrid(Widget &child, int x = -1, int y = -1);
        void ReplaceChildOnGrid(Widget &cur_child, Widget &new_child);
        virtual void RemoveChild(Widget &child);
        virtual bool IsChildHighlighted(const Widget &child);
        virtual Widget *GetHighlightedChild();
        virtual void SetHighlightedChild(const Widget &child);
        virtual void KeyPressDown(unsigned short  scancode);
        virtual void KeyPressUp(unsigned short  scancode);
        virtual bool HandleMove(int x, int y);
        virtual Widget *GetEdgeChild(int xedge, int yedge);

    protected:
        using row_t = std::vector<Widget *>;
        std::vector<row_t> rows;

        row_t &GetRow(unsigned int row_id);
        void SetIndex(row_t &row, unsigned int index, Widget *widget);
        Widget *GetIndex(row_t &row, unsigned int index);

        unsigned int curx = 0;
        unsigned int cury = 0;

        bool TryMoveVert(int change);
        bool TryMoveHoriz(int change);
};

class HorizontalScreen : public GridWidget
{
    public:

};

class StaticPadProvider
{
    public:
        unsigned int pad = 2;
};

class ProgressBarWidget : public NonactivatableWidget,
    public BackgroundRenderer, public BorderRenderer,
    public StaticPadProvider, public StaticBorderProvider,
    public StaticBackgroundProvider, public StaticForegreoundProvider
{
    public:
        enum Orientation_t { RightToLeft, LeftToRight, BottomToTop, TopToBottom };

        Orientation_t GetOrientation();
        unsigned int GetMaxValue();
        unsigned int GetCurValue();

        void SetOrientation(Orientation_t orientation);
        void SetMaxValue(unsigned int max_value);
        void SetCurValue(unsigned int cur_value);

        void Update(alpha_t alpha = std::numeric_limits<alpha_t>::max());

    protected:
        Orientation_t o = RightToLeft;
        unsigned int cur_value = 50;
        unsigned int max_value = 100;
};

#include "widget_keyboard.h"

#endif
