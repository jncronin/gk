#ifndef WIDGET_H
#define WIDGET_H

#include <string>
#include <vector>
#include <list>

typedef unsigned char color_t;
typedef short int coord_t;
constexpr const coord_t fb_w = 640;
constexpr const coord_t fb_h = 480;
constexpr const unsigned int fb_stride = fb_w * sizeof(color_t);

constexpr const color_t default_inactive_bg_color = 0xd4;
constexpr const color_t default_clicked_bg_color = 0xd1;
constexpr const color_t default_highlight_bg_color = 0xd4;

constexpr const color_t default_inactive_text_color = 0xf0;
constexpr const color_t default_clicked_text_color = 0xff;
constexpr const color_t default_highlight_text_color = 0xf0;

constexpr const color_t default_inactive_border_color = 0xf0;
constexpr const color_t default_clicked_border_color = 0xff;
constexpr const color_t default_highlight_border_color = 0xff;

constexpr const coord_t default_border_width = 8;

/* key scancodes */
enum Scancodes
{
    KeyRight = 79,
    KeyLeft = 80,
    KeyDown = 81,
    KeyUp = 82,
    KeyA = 'a',
    KeyB = 'b',
    KeyX = 'x',
    KeyY = 'y',
    KeyEnter = 40,
    KeyLCtrl = 224,
};

/* blending */
color_t DoBlend(color_t fg, color_t bg);

enum HOffset { Left, Centre, Right };
enum VOffset { Top, Middle, Bottom };

class Widget
{
    public:
        virtual void Update() = 0;

        virtual void StartHover();
        virtual void EndHover();

        virtual void Activate();
        virtual void Deactivate();

        virtual bool CanActivate();
        virtual bool CanClick();
        virtual bool CanHighlight();

        virtual bool IsActivated();

        int x, y, w, h;
        int ctrl_id;

        Widget *parent;

        void (*OnClick)(coord_t x, coord_t y);

        void GetAbsolutePosition(coord_t *x, coord_t *y);

        virtual bool IsHighlighted();
        virtual bool IsChildHighlighted(const Widget &child);

        virtual void KeyPressDown(Scancodes scancode);
        virtual void KeyPressUp(Scancodes scancode);

    protected:
        bool is_clicked = false;

        bool new_hover = false;
        bool new_activated = false;
};

// Animations
typedef bool (*WidgetAnimation)(Widget *wdg, void *p, unsigned long long int ms_since_start);
struct WidgetAnimation_t
{
    WidgetAnimation anim;
    Widget *w;
    void *p;
    unsigned long long int start_time;
};
using WidgetAnimationList = std::list<WidgetAnimation_t>;

bool RunAnimations(WidgetAnimationList &wl, unsigned long int cur_ms);
void AddAnimation(WidgetAnimationList &wl, unsigned long int cur_ms,
    WidgetAnimation anim, Widget *wdg, void *p);


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
        virtual void KeyPressDown(Scancodes key);
        virtual void KeyPressUp(Scancodes key);
};

class TextRenderer
{
    public:
        const static int FONT_LARGE = 0;
        const static int FONT_SMALL = 1;

    protected:
        void RenderText(coord_t x, coord_t y, coord_t w, coord_t h,
            const std::string &text,
            color_t fg_color, color_t bg_color,
            HOffset hoffset, VOffset voffset,
            int font);
};

class BorderRenderer
{
    protected:
        void RenderBorder(coord_t x, coord_t y, coord_t w, coord_t h,
            color_t border_color, coord_t border_width);
};

class BackgroundRenderer
{
    protected:
        void RenderBackground(coord_t x, coord_t y, coord_t w, coord_t h,
            color_t bg_color);
};

class ImageRenderer
{
    protected:
        void RenderImage(coord_t x, coord_t y, coord_t w, coord_t h,
            coord_t img_w, coord_t img_h,
            HOffset hoffset, VOffset voffset,
            const color_t *img, color_t bg_color);
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
};

class RectangleWidget : public NonactivatableWidget, public BorderRenderer, public BackgroundRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider
{
    public:
        void Update();
};

class LabelWidget : public NonactivatableWidget, public BorderRenderer, public BackgroundRenderer,
    public TextRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticTextProvider
{
    public:
        void Update();
};

class ButtonWidget : public ClickableWidget, public BorderRenderer, public BackgroundRenderer,
    public TextRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticTextProvider
{
    public:
        void Update();
};

class ImageButtonWidget : public ClickableWidget, public BorderRenderer, public BackgroundRenderer,
    public ImageRenderer,
    public StaticBorderProvider, public StaticBackgroundProvider, public StaticImageProvider
{
    public:
        void Update();
};

class ContainerWidget : public NonactivatableWidget
{
    public:
        void Update();
        virtual Widget *GetHighlightedChild() = 0;
        virtual bool CanHighlight();
        void AddChild(Widget &child);
    
    protected:
        std::vector<Widget *> children;
        ContainerWidget() {}
};

class GridWidget : public ContainerWidget
{
    public:
        void AddChildOnGrid(Widget &child, int x = -1, int y = -1);
        virtual bool IsChildHighlighted(const Widget &child);
        virtual Widget *GetHighlightedChild();
        virtual void KeyPressDown(Scancodes scancode);
        virtual void KeyPressUp(Scancodes scancode);

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

#endif
