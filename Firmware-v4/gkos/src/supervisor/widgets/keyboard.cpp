#include "widget.h"
#include <cmath>

static const constexpr float btn_w = 40.0f;
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

KeyboardWidget::Keybutton::Keybutton(int _key, std::string _text, float _x, float _y, float _w, float _h,
    std::string _shift_text)
{
    key = _key;
    x = _x * btn_w;
    y = _y * btn_h;
    w = _w * btn_w;
    h = _h * btn_h;
    gx = std::rint(((_x + _w/2.0f) * btn_w) / 4.0f);
    gy = std::rint(((_y + _h/2.0f) * btn_h) / 4.0f);
    OnClick = _OnClick;
    OnClickBegin = _OnClickBegin;
    border_width = 1.0f;
    text = _text;
    unshift_text = _text;
    if(_shift_text.empty())
        shift_text = text;
    else
        shift_text = _shift_text;
    Font = FONT_SMALL;
}

KeyboardWidget::Keybutton::Keybutton(int _key, std::string _text, float _x, float _y, std::string _shift_text)
{
    float _w = 1.0f;
    float _h = 1.0f;
    key = _key;
    x = _x * btn_w;
    y = _y * btn_h;
    w = _w * btn_w;
    h = _h * btn_h;
    gx = std::rint(((_x + _w/2.0f) * btn_w) / 4.0f);
    gy = std::rint(((_y + _h/2.0f) * btn_h) / 4.0f);
    OnClick = _OnClick;
    OnClickBegin = _OnClickBegin;
    border_width = 1.0f;
    text = _text;
    unshift_text = _text;
    if(_shift_text.empty())
        shift_text = text;
    else
        shift_text = _shift_text;
    Font = FONT_SMALL;
}

void KeyboardWidget::Keybutton::_OnClick(Widget *w, coord_t x, coord_t y)
{
    auto _this = reinterpret_cast<KeyboardWidget::Keybutton *>(w);
    if(_this->kbd)
    {
        if(_this->kbd->OnKeyboardButtonClick)
        {
            // handle modifier keys
            bool was_modifier = false;
            if(_this->key == GK_SCANCODE_LSHIFT || _this->key == GK_SCANCODE_RSHIFT)
            {
                _this->kbd->is_shift = !_this->kbd->is_shift;
                was_modifier = true;
            }
            else if(_this->key == GK_SCANCODE_LCTRL || _this->key == GK_SCANCODE_RCTRL)
            {
                _this->kbd->is_ctrl = !_this->kbd->is_ctrl;
                was_modifier = true;
            }
            else if(_this->key == GK_SCANCODE_LALT || _this->key == GK_SCANCODE_RALT)
            {
                _this->kbd->is_alt = !_this->kbd->is_alt;
                was_modifier = true;
            }
            else if(_this->key == GK_SCANCODE_CAPSLOCK)
            {
                _this->kbd->is_capslock = !_this->kbd->is_capslock;
            }
            _this->kbd->OnKeyboardButtonClick(_this->kbd, x + _this->x, y + _this->y, _this->key,
                _this->kbd->is_shift || _this->kbd->is_capslock, _this->kbd->is_ctrl, _this->kbd->is_alt);
            if(!was_modifier)
            {
                // shift auto cancels after any other key click
                _this->kbd->is_shift = false;
            }
        }
    }
}

void KeyboardWidget::Keybutton::_OnClickBegin(Widget *w, coord_t x, coord_t y)
{
    auto _this = reinterpret_cast<KeyboardWidget::Keybutton *>(w);
    if(_this->kbd && _this->kbd->OnKeyboardButtonClickBegin)
    {
        _this->kbd->OnKeyboardButtonClickBegin(_this->kbd, x + _this->x, y + _this->y, _this->key,
            _this->kbd->is_shift || _this->kbd->is_capslock, _this->kbd->is_ctrl, _this->kbd->is_alt);
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

void KeyboardWidget::Update(alpha_t alpha)
{
    if(!visible) return;
    for(auto &key : btns)
    {
        if(is_shift || is_capslock)
        {
            key.text = key.shift_text;
        }
        else
        {
            key.text = key.unshift_text;
        }
        if(key.key == GK_SCANCODE_LSHIFT || key.key == GK_SCANCODE_RSHIFT)
        {
            key.SetClickedAppearance(is_shift);
        }
        else if(key.key == GK_SCANCODE_LALT || key.key == GK_SCANCODE_RALT)
        {
            key.SetClickedAppearance(is_alt);
        }
        else if(key.key == GK_SCANCODE_LCTRL || key.key == GK_SCANCODE_RCTRL)
        {
            key.SetClickedAppearance(is_ctrl);
        }
        else if(key.key == GK_SCANCODE_CAPSLOCK)
        {
            key.SetClickedAppearance(is_capslock);
        }
    }
    ContainerWidget::Update(alpha);
}
