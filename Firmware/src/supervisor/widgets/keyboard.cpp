#include "widget.h"

static const constexpr float btn_w = 32.0f;
static const constexpr float btn_h = 32.0f;

KeyboardWidget::KeyboardWidget()
{
    int max_w = 0;
    int max_h = 0;

    bool first_member_set = false;

    for(auto &key : btns)
    {
        AddChildOnGrid(key, key.gx, key.gy);

        if(!first_member_set)
        {
            curx = key.gx;
            cury = key.gy;
            first_member_set = true;
        }

        int cur_r = key.x + key.w;
        int cur_b = key.y + key.h;
        if(cur_r > max_w) max_w = cur_r;
        if(cur_b > max_h) max_h = cur_b;

        key.kbd = this;
    }

    if(max_w > w) w = max_w;
    if(max_h > h) h = max_h;
}

KeyboardWidget::Keybutton::Keybutton(int _key, std::string _text, float _x, float _y, float _w, float _h)
{
    key = _key;
    x = _x * btn_w;
    y = _y * btn_h;
    w = _w * btn_w;
    h = _h * btn_h;
    gx = ((_x + _w/2.0f) * btn_w) / 4.0f;
    gy = ((_y + _h/2.0f) * btn_h) / 4.0f;
    OnClick = _OnClick;
    OnClickBegin = _OnClickBegin;
    border_width = 1.0f;
    text = _text;
    Font = FONT_SMALL;
}

void KeyboardWidget::Keybutton::_OnClick(Widget *w, coord_t x, coord_t y)
{
    auto _this = reinterpret_cast<KeyboardWidget::Keybutton *>(w);
    if(_this->kbd && _this->kbd->OnKeyboardButtonClick)
    {
        _this->kbd->OnKeyboardButtonClick(_this->kbd, x + _this->x, y + _this->y, _this->key);
    }
}

void KeyboardWidget::Keybutton::_OnClickBegin(Widget *w, coord_t x, coord_t y)
{
    auto _this = reinterpret_cast<KeyboardWidget::Keybutton *>(w);
    if(_this->kbd && _this->kbd->OnKeyboardButtonClickBegin)
    {
        _this->kbd->OnKeyboardButtonClickBegin(_this->kbd, x + _this->x, y + _this->y, _this->key);
    }
}

bool KeyboardWidget::HandleMove(int _x, int _y)
{
    if(_x)
    {
        return TryMoveHoriz(_x);
    }
    else
    {
        return TryMoveVert(_y);
    }
}
