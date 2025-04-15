#include "widget.h"

bool Widget::CanActivate()
{
    return false;
}

bool Widget::CanClick()
{
    return false;
}

void Widget::StartHover()
{ }

void Widget::EndHover()
{ }

void Widget::Activate()
{ }

void Widget::Deactivate()
{ }

void Widget::GetAbsolutePosition(coord_t *absx, coord_t *absy)
{
    coord_t retx = 0;
    coord_t rety = 0;

    Widget *c = this;
    while(c)
    {
        retx += c->x;
        rety += c->y;
        c = c->parent;
    }

    if(absx) *absx = retx;
    if(absy) *absy = rety;
}

bool NonactivatableWidget::CanActivate()
{
    return false;
}

bool ActivatableWidget::CanActivate()
{
    return true;
}

bool ClickableWidget::CanClick()
{
    return true;
}

bool Widget::IsHighlighted()
{
    if(parent == nullptr)
    {
        return true;
    }
    else
    {
        return parent->IsChildHighlighted(*this);
    }
}

bool Widget::IsChildHighlighted(const Widget &child)
{
    return false;
}

bool Widget::IsActivated()
{
    return false;
}

void Widget::KeyPressDown(unsigned short key)
{
}

void Widget::KeyPressUp(unsigned short  key)
{
}

bool Widget::CanHighlight()
{
    return false;
}

bool Widget::HandleMove(int _x, int _y)
{
    return false;
}

bool ClickableWidget::CanHighlight()
{
    return true;
}

void ClickableWidget::KeyPressDown(unsigned short  key)
{
    if(key == GK_SCANCODE_RETURN)
    {
        is_clicked = true;
        if(OnClickBegin)
        {
            OnClickBegin(this, 0, 0);
        }
    }
}

void ClickableWidget::KeyPressUp(unsigned short  key)
{
    if(key == GK_SCANCODE_RETURN)
    {
        if(is_clicked)
        {
            is_clicked = false;
            if(OnClick)
            {
                OnClick(this, 0, 0);
            }
        }
    }
}

void Widget::SetClickedAppearance(bool v)
{
    is_pretend_clicked = v;
}

void Widget::Clear()
{
}

std::vector<Widget *> Widget::GetChildren()
{
    return std::vector<Widget *>();
}
