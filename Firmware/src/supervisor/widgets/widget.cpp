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

void Widget::KeyPressDown(Scancodes key)
{
}

void Widget::KeyPressUp(Scancodes key)
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

void ClickableWidget::KeyPressDown(Scancodes key)
{
    if(key == Scancodes::KeyEnter)
    {
        is_clicked = true;
    }
}

void ClickableWidget::KeyPressUp(Scancodes key)
{
    if(key == Scancodes::KeyEnter)
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
