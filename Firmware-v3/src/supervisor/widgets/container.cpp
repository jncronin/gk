#include "widget.h"

unsigned long int clock_cur_ms();

struct scroll_params
{
    coord_t x_from, x_to, y_from, y_to;
    unsigned int tot_t;
};

void ContainerWidget::Update(alpha_t alpha)
{
    for(auto &child : children)
    {
        child->Update(alpha);
    }
}

void ContainerWidget::AddChild(Widget &child)
{
    children.push_back(&child);
    child.parent = this;
}

void ContainerWidget::RemoveChild(Widget &child)
{
    for(auto iter = children.begin(); iter < children.end();)
    {
        if(*iter == &child)
            iter = children.erase(iter);
        else
            iter++;
    }
}

void GridWidget::RemoveChild(Widget &child)
{
    for(auto row : rows)
    {
        for(unsigned int i = 0; i < row.size(); i++)
        {
            if(row[i] == &child)
                row[i] = nullptr;
        }
    }
    ContainerWidget::RemoveChild(child);
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

void GridWidget::SetHighlightedChild(const Widget &child)
{
    for(unsigned int cy = 0; cy < rows.size(); cy++)
    {
        const auto row = rows[cy];
        for(unsigned int cx = 0; cx < row.size(); cx++)
        {
            const auto c = row[cx];
            if(c && c == &child)
            {
                cury = cy;
                curx = cx;
                ScrollToSelected();
                return;
            }
        }
    }
}

void GridWidget::ReplaceChildOnGrid(Widget &cur_child, Widget &new_child)
{
    for(unsigned int cy = 0; cy < rows.size(); cy++)
    {
        auto &row = rows[cy];
        for(unsigned int cx = 0; cx < row.size(); cx++)
        {
            auto c = row[cx];
            if(c && c == &cur_child)
            {
                row[cx] = &new_child;
            }
        }
    }

    for(unsigned int i = 0; i < children.size(); i++)
    {
        auto c = children[i];
        if(c && c == &cur_child)
        {
            children[i] = &new_child;
        }
    }

    new_child.parent = this;
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

bool GridWidget::HandleMove(int xm, int ym)
{
    auto child = GetHighlightedChild();
    if(child)
    {
        if(xm != 0)
            return child->HandleMove(xm, 0) || TryMoveHoriz(xm);
        else if(ym != 0)
            return child->HandleMove(0, ym) || TryMoveVert(ym);
    }
    else
    {
        if(xm != 0)
            return TryMoveHoriz(xm);
        else if(ym != 0)
            return TryMoveVert(ym);
    }
    return false;
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
        if(sp->x_from != sp->x_to)
            w->x = Anim_Interp_Linear(sp->x_from, sp->x_to, ms_since_start, sp->tot_t);
        if(sp->y_from != sp->y_to)
            w->y = Anim_Interp_Linear(sp->y_from, sp->y_to, ms_since_start, sp->tot_t);
        return false;
    }
}

void ContainerWidget::ScrollToSelected()
{
    // scroll so that the selected item fits in the container
    //  first make the rightmost/bottommost extent fit, then the leftmost/topmost
    //  this means for children bigger than the container we alighn the topleft corners

    coord_t x_scroll = 0;
    coord_t y_scroll = 0;

    auto sel = GetHighlightedChild();
    if(sel == nullptr)
        return;
    auto sel_x = sel->x;
    auto sel_y = sel->y;

    auto new_x = sel->x;
    auto new_y = sel->y;
    if((sel_x + sel->w) > w)
        new_x = w - sel->w;
    if((sel_y + sel->h) > h)
        new_y = h - sel->y;
    if(sel_x < 0)
        new_x = 0;
    if(sel_y < 0)
        new_y = 0;

    x_scroll = new_x - sel_x;
    y_scroll = new_y - sel_y;

    ScrollTo(x_scroll, y_scroll);
}

void ContainerWidget::ScrollTo(coord_t x_scroll, coord_t y_scroll)
{
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

Widget *GridWidget::GetEdgeChild(int xedge, int yedge)
{
    // just handle four edges for now
    if(yedge < 0)
    {
        for(unsigned int i = 0; i < rows.size(); i++)
        {
            auto &crow = rows[i];
            for(unsigned int j = 0; j < crow.size(); j++)
            {
                if(crow[j] != nullptr)
                    return crow[j];
            }
        }
    }
    else if(yedge > 0)
    {
        for(int i = rows.size() - 1; i >= 0; i--)
        {
            auto &crow = rows[i];
            for(unsigned int j = 0; j < crow.size(); j++)
            {
                if(crow[j] != nullptr)
                    return crow[j];
            }
        }
    }
    else if(xedge < 0)
    {
        for(unsigned int i = 0; i < rows.size(); i++)
        {
            auto &crow = rows[i];
            for(unsigned int j = 0; j < crow.size(); j++)
            {
                if(crow[j] != nullptr)
                    return crow[j];
            }
        }
    }
    else if(xedge > 0)
    {
        for(unsigned int i = 0; i < rows.size(); i++)
        {
            auto &crow = rows[i];
            for(int j = crow.size() - 1; j >= 0; j--)
            {
                if(crow[j] != nullptr)
                    return crow[j];
            }
        }
    }
    return nullptr;
}
