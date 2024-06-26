#include "widget.h"

void ContainerWidget::Update()
{
    for(auto &child : children)
    {
        child->Update();
    }
}

void ContainerWidget::AddChild(Widget &child)
{
    children.push_back(&child);
    child.parent = this;
}

void GridWidget::AddChildOnGrid(Widget &child, int xpos, int ypos)
{
    if(ypos == -1)
        ypos = 0;
    if(xpos == -1)
        xpos = GetRow(ypos).size();

    SetIndex(GetRow(ypos), xpos, &child);
    
    ContainerWidget::AddChild(child);
}

GridWidget::row_t &GridWidget::GetRow(unsigned int idx)
{
    while(rows.size() <= idx)
    {
        rows.push_back(row_t());
    }
    return rows[idx];
}

void GridWidget::SetIndex(row_t &row, unsigned int idx, Widget *widget)
{
    while(row.size() <= idx)
    {
        row.push_back(nullptr);
    }
    row[idx] = widget;
}

bool GridWidget::IsChildHighlighted(const Widget &child)
{
    if(!IsHighlighted())
    {
        return false;
    }

    return GetHighlightedChild() == &child;
}

Widget *GridWidget::GetHighlightedChild()
{
    auto row = GetRow(cury);
    if(row.size() <= curx)
    {
        return nullptr;
    }
    return row[curx];
}

Widget *GridWidget::GetIndex(GridWidget::row_t &row, unsigned int idx)
{
    if(row.size() <= idx)
    {
        return nullptr;
    }
    return row[idx];
}

void GridWidget::KeyPressUp(Scancodes key)
{
    if(key == Scancodes::KeyEnter)
    {
        auto child = GetHighlightedChild();
        if(child)
        {
            child->KeyPressUp(key);
        }
    }
}

void GridWidget::KeyPressDown(Scancodes key)
{
    auto child = GetHighlightedChild();
    if(key == Scancodes::KeyEnter)
    {
        if(child)
        {
            child->KeyPressDown(key);
        }
    }
    else
    {
        bool move_succeed = false;
        switch(key)
        {
            case Scancodes::KeyLeft:
                move_succeed = child->HandleMove(-1, 0) || TryMoveHoriz(-1);
                break;
            case Scancodes::KeyRight:
                move_succeed = child->HandleMove(1, 0) || TryMoveHoriz(1);
                break;
            case Scancodes::KeyUp:
                move_succeed = child->HandleMove(0, -1) || TryMoveVert(-1);
                break;
            case Scancodes::KeyDown:
                move_succeed = child->HandleMove(0, 1) || TryMoveVert(1);
                break;
            default:
                break;
        }

        if(!move_succeed)
        {
            // TODO - move out to higher level container
        }
    }
}

bool ContainerWidget::CanHighlight()
{
    return true;
}

bool GridWidget::TryMoveHoriz(int dir)
{
    auto row = GetRow(cury);
    auto newx = (int)curx;
    while(true)
    {
        newx += dir;
        if(newx < 0)
            return false;
        if(newx >= (int)row.size())
            return false;
        if(row[newx] != nullptr && row[newx]->CanHighlight())
        {
            curx = newx;
            return true;
        }
    }
}

bool GridWidget::TryMoveVert(int dir)
{
    auto newy = (int)cury;
    while(true)
    {
        newy += dir;
        if(newy < 0)
            return false;
        if(newy >= (int)rows.size())
            return false;
        auto newrow = GetRow(newy);

        // first try matching same x value, then get nearest of leftwards or rightwards scan
        if(GetIndex(newrow, curx) != nullptr && GetIndex(newrow, curx)->CanHighlight())
        {
            cury = newy;
            return true;
        }
        int nearest_left = -1, nearest_right = -1;
        auto newx = (int)curx;
        while(true)
        {
            newx--;
            if(newx < 0)
                break;
            if(GetIndex(newrow, newx) != nullptr && GetIndex(newrow, newx)->CanHighlight())
            {
                nearest_left = newx;
                break;
            }
        }
        newx = (int)curx;
        while(true)
        {
            newx++;
            if(newx >= (int)newrow.size())
                break;
            if(GetIndex(newrow, newx) != nullptr && GetIndex(newrow, newx)->CanHighlight())
            {
                nearest_right = newx;
                break;
            }           
        }

        if(nearest_left == -1 && nearest_right != -1)
        {
            // something to right, nothing to left
            curx = nearest_right;
            cury = newy;
            return true;
        }
        if(nearest_right == -1 && nearest_left != -1)
        {
            // something to left, nothing to right
            curx = nearest_left;
            cury = newy;
            return true;
        }
        if(nearest_left != -1 && nearest_right != -1)
        {
            // something to both left and right - get closest
            int left_dist = curx - nearest_left;
            int right_dist = nearest_right - curx;

            if(right_dist < left_dist)
                curx = nearest_right;
            else
                curx = nearest_left;
            cury = newy;
            return true;
        }
    }
}
