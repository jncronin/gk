#include "widget.h"

unsigned long int clock_cur_ms();

struct scroll_params
{
    coord_t x_from, x_to, y_from, y_to;
    unsigned int tot_t;
};

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

void GridWidget::KeyPressUp(unsigned short  key)
{
    if(key == GK_SCANCODE_RETURN)
    {
        auto child = GetHighlightedChild();
        if(child)
        {
            child->KeyPressUp(key);
        }
    }
}

void GridWidget::KeyPressDown(unsigned short  key)
{
    auto child = GetHighlightedChild();
    if(key == GK_SCANCODE_RETURN)
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
            case GK_SCANCODE_LEFT:
                move_succeed = child->HandleMove(-1, 0) || TryMoveHoriz(-1);
                break;
            case GK_SCANCODE_RIGHT:
                move_succeed = child->HandleMove(1, 0) || TryMoveHoriz(1);
                break;
            case GK_SCANCODE_UP:
                move_succeed = child->HandleMove(0, -1) || TryMoveVert(-1);
                break;
            case GK_SCANCODE_DOWN:
                move_succeed = child->HandleMove(0, 1) || TryMoveVert(1);
                break;
            default:
                break;
        }

        if(move_succeed)
        {
            // ensure newly selected object is visible
            ScrollToSelected();
        }
        else
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

static bool anim_scroll(Widget *w, void *p, time_ms_t ms_since_start)
{
    auto sp = reinterpret_cast<scroll_params *>(p);
    if(ms_since_start >= sp->tot_t)
    {
        w->x = sp->x_to;
        w->y = sp->y_to;
        delete sp;
        return true;
    }
    else
    {
        w->x = Anim_Interp_Linear(sp->x_from, sp->x_to, ms_since_start, sp->tot_t);
        w->y = Anim_Interp_Linear(sp->y_from, sp->y_to, ms_since_start, sp->tot_t);
        return false;
    }
}

void ContainerWidget::ScrollToSelected()
{
    // we scroll all items by the current size of the container for now
    coord_t x_scroll = 0;
    coord_t y_scroll = 0;

    auto sel = GetHighlightedChild();
    auto sel_x = sel->x;
    auto sel_y = sel->y;

    while((sel_x + x_scroll) < 0) x_scroll += w;
    while((sel_x + x_scroll) >= w) x_scroll -= w;
    while((sel_y + y_scroll) < 0) y_scroll += h;
    while((sel_y + y_scroll) >= h) y_scroll -= h;

    if(x_scroll || y_scroll)
    {
        auto al = GetAnimationList();

        for(auto c : children)
        {
            if(al)
            {
                auto sp = new scroll_params();
                sp->x_from = c->x;
                sp->x_to = c->x + x_scroll;
                sp->y_from = c->y;
                sp->y_to = c->y + y_scroll;
                sp->tot_t = 150UL;
                AddAnimation(*al, clock_cur_ms(), anim_scroll, c, sp);
            }
            else
            {
                c->x += x_scroll;
                c->y += y_scroll;
            }
        }
    }
}
