#include "widgets/widget.h"
#include "process.h"
#include "syscalls_int.h"
#include "INIReader.h"

WidgetAnimationList *GetAnimationList();

/* data for a key click */
struct key_click
{
    std::vector<Event> events;
    bool is_quit = false;
};

void Process::set_osd(const std::string &_osd_text)
{
    has_osd = true;
    osd_prepped = false;
    osd = nullptr;
    osd_text = _osd_text;
}

static GridWidget _def_osd;
static bool _def_osd_init = false;
extern LabelWidget lab_caption;
extern GridWidget scr_overlay;
static ButtonWidget bw_exit;
static RectangleWidget rw;

bool anim_handle_quit_failed(Widget *wdg, void *p, time_ms_t t)
{
    const constexpr time_ms_t quit_delay = 5000;
    
    if(t < quit_delay)
        return false;

    auto pid = (pid_t)p;
    
    if(deferred_call(syscall_get_pid_valid, pid))
    {
        klog("supervisor: request to quit pid %u failed, force-quitting\n", pid);

        deferred_call(syscall_kill, pid, SIGKILL);
    }

    return true;
}

static void btn_exit_click(Widget *w, coord_t x, coord_t y)
{
    auto fpid = deferred_call(syscall_get_focus_pid);
    if(fpid >= 0)
    {
        auto fppid = deferred_call(syscall_get_proc_ppid, fpid);
        extern pid_t pid_gkmenu;

        // only quit processes started by gkmenu
        if(fppid >= 0 && fppid == pid_gkmenu)
        {
            deferred_call(syscall_kill, fpid, SIGKILL);
        }
    }
}

GridWidget *default_osd()
{
    if(!_def_osd_init)
    {
        _def_osd.x = 0;
        _def_osd.y = 0;
        _def_osd.w = scr_overlay.w;
        _def_osd.h = scr_overlay.h;

        // background
        rw.x = 0;
        rw.y = 0;
        rw.w = scr_overlay.w;
        rw.h = scr_overlay.h;
        rw.bg_inactive_color = 0x87;
        rw.border_width = 0;
        _def_osd.AddChild(rw);

        // Display name of current game
        lab_caption.x = 0;
        lab_caption.y = 0;
        lab_caption.w = scr_overlay.w;
        lab_caption.h = 32;
        lab_caption.bg_inactive_color = 0x87;
        lab_caption.border_width = 0;
        lab_caption.text = "GKMenu";
        _def_osd.AddChild(lab_caption);

        // TODO: game customisation
        bw_exit.w = 80;
        bw_exit.h = 80;
        bw_exit.x = 0 + (scr_overlay.w - bw_exit.w) / 2;
        bw_exit.y = (scr_overlay.h - bw_exit.h) / 2;
        bw_exit.text = "Quit";
        bw_exit.OnClick = btn_exit_click;
        _def_osd.AddChildOnGrid(bw_exit);
    }
    return &_def_osd;
}

struct ini_str_provider
{
    char *buf;
    size_t pos;
};

static char *strreader(char *ret, int num, void *stream)
{
    auto p = reinterpret_cast<ini_str_provider *>(stream);
    if(p->buf[p->pos] == 0)
        return nullptr;
    
    auto lend = strchrnul(&p->buf[p->pos], '\n');

    auto maxread = lend - &p->buf[p->pos];
    auto toread = std::min(maxread, num-1);

    strncpy(ret, &p->buf[p->pos], toread);
    ret[toread] = 0;

    p->pos += toread;

    while(p->buf[p->pos] == '\n')
        p->pos = p->pos + 1;

    return ret;
}

static bool is_type(const std::string &tname,
    const std::string &basetname)
{
    if(tname == basetname)
        return true;
    else if(basetname == "widget")
        return true;
    else if(basetname == "statictextprovider" &&
        (is_type(tname, "button") ||
        is_type(tname, "label")))
        return true;
    

    return false;
}

static int getint(const std::string &s)
{
    return strtol(s.c_str(), nullptr, 0);
}

#define astr(x) if(name == #x) cw->x = val;
#define aint(x) if(name == #x) cw->x = getint(val);

static unsigned short str_to_key(const std::string &s);
static void key_handler(Widget *w, coord_t x, coord_t y);

static void add_property(Widget *w, const std::string &tname,
     const std::string &name, const std::string &val)
{
    // have to use most derived class names here as rtti is not guaranteed to work
    if(name == "x")
        w->x = getint(val);
    else if(name == "y")
        w->y = getint(val);
    else if(name == "w")
        w->w = getint(val);
    else if(name == "h")
        w->h = getint(val);
    else if(name == "ch_x")
        w->ch_x = getint(val);
    else if(name == "ch_y")
        w->ch_y = getint(val);
    else if(is_type(tname, "label"))
    {
        auto cw = (LabelWidget *)w;
        astr(text)
        else aint(border_width)
        else aint(border_inactive_color)
        else aint(border_clicked_color)
        else aint(border_highlight_color)
        else aint(bg_inactive_color)
        else aint(bg_clicked_color)
        else aint(bg_highlight_color)
        else aint(text_inactive_color)
        else aint(text_clicked_color)
        else aint(text_highlight_color)
    }
    else if(is_type(tname, "button"))
    {
        auto cw = (ButtonWidget *)w;
        astr(text)
        else aint(border_width)
        else aint(border_inactive_color)
        else aint(border_clicked_color)
        else aint(border_highlight_color)
        else aint(bg_inactive_color)
        else aint(bg_clicked_color)
        else aint(bg_highlight_color)
        else aint(text_inactive_color)
        else aint(text_clicked_color)
        else aint(text_highlight_color)
        else if(name == "click")
        {
            if(!cw->d)
            {
                cw->d = new key_click();
                if(cw->d)
                    cw->OnClick = key_handler;
                else
                {
                    klog("supervisor: couldn't create new std::vector\n");
                }
            }
            if(cw->d)
            {
                auto i = str_to_key(val);
                auto &sv = ((key_click *)cw->d)->events;
                Event epush { .type = Event::KeyDown, .key = i };
                Event erelease { .type = Event::KeyUp, .key = i };
                sv.push_back(epush);
                sv.push_back(erelease);
            }
        }
        else if(name == "clickquit")
        {
            if(!cw->d)
            {
                cw->d = new key_click();
                if(cw->d)
                    cw->OnClick = key_handler;
                else
                {
                    klog("supervisor: couldn't create new std::vector\n");
                }
            }
            if(cw->d)
            {
                ((key_click *)cw->d)->is_quit = true;
            }
        }
    }
}

Widget *Process::get_osd()
{
    if(!has_osd)
    {
        return default_osd();
    }
    if(!osd_prepped)
    {
        // prepare the OSD by parsing the .ini file
        ini_str_provider iprov { .buf = (char *)osd_text.c_str(), .pos = 0 };
        INIReader rdr(strreader, (void *)&iprov);

        auto nosd = new GridWidget();
        if(!nosd)
        {
            klog("supervisor: couldn't create new GridWidget\n");
            return default_osd();
        }
        nosd->x = 0;
        nosd->y = 0;
        nosd->w = scr_overlay.w;
        nosd->h = scr_overlay.h;

        for(const auto &sect : rdr.SectionLinenums())
        {
            auto sname = rdr.GetSection(sect);
            Widget *w = nullptr;
            bool on_grid = true;
            int gx = -1;
            int gy = -1;

            if(sname == "label")
            {
                w = new LabelWidget();
                on_grid = false;
            }
            else if(sname == "button")
            {
                w = new ButtonWidget();
            }

            if(w)
            {
                for(const auto &key : rdr.KeyLinenums(sect))
                {
                    auto kname = rdr.GetKey(key);
                    auto kval = rdr.Get(key);

                    if(kname == "gx")
                        gx = getint(kval);
                    else if(kname == "gy")
                        gy = getint(kval);
                    else
                        add_property(w, sname, kname, kval);
                }

                if(on_grid)
                    nosd->AddChildOnGrid(*w, gx, gy);
                else
                    nosd->AddChild(*w);
            }
        }

        osd = nosd;

        osd_prepped = true;
        osd_text.clear();
    }
    return osd;
}

struct passkeystrokes_data
{
    std::vector<Event> events;
    unsigned int next_idx;
};

static bool anim_passkeystrokes(Widget *wdg, void *p, time_ms_t t)
{
    auto p_next_idx = reinterpret_cast<passkeystrokes_data *>(p);
    auto next_idx = p_next_idx->next_idx;
    if(next_idx < p_next_idx->events.size())
    {
        if(t > next_idx * 17)
        {
            // one frame 16.6 ms
            auto cev = p_next_idx->events[next_idx];
            if(focus_process)
            {
                focus_process->events.Push(cev);
            }

            next_idx++;
            p_next_idx->next_idx = next_idx;
        }
    }

    if(next_idx >= p_next_idx->events.size())
    {
        delete p_next_idx;
        return true;
    }

    return false;
}

void key_handler(Widget *w, coord_t x, coord_t y)
{
    if(w->d && focus_process)
    {
        auto l = reinterpret_cast<key_click *>(w->d);
        if(l->events.size() > 0)
        {
            // begin passing keystrokes at regular intervals

            // hold the next keystroke to pass
            auto next_idx = new passkeystrokes_data();
            if(next_idx)
            {
                next_idx->next_idx = 0;
                next_idx->events = l->events;       // copy in case process is destroyed during animation
                AddAnimation(*GetAnimationList(), clock_cur_ms(), anim_passkeystrokes, nullptr, next_idx);
            }
            else
            {
                // failed to allocate structures - just pass the keystrokes (but we will probably crash anyway)
                for(const auto &e : l->events)
                {
                    focus_process->events.Push(e);
                }
            }
        }

        if(l->is_quit)
        {
            auto fpid = deferred_call(syscall_get_focus_pid);

            // backup quit incase the above didn't work
            AddAnimation(*GetAnimationList(), clock_cur_ms(), anim_handle_quit_failed, nullptr, (void *)fpid);
        }
    }
}

#define SK(x) if(s == #x) return x;

static unsigned short _str_to_key(const std::string &s)
{
    SK(GK_SCANCODE_0);
    SK(GK_SCANCODE_1);
    SK(GK_SCANCODE_2);
    SK(GK_SCANCODE_3);
    SK(GK_SCANCODE_4);
    SK(GK_SCANCODE_5);
    SK(GK_SCANCODE_6);
    SK(GK_SCANCODE_7);
    SK(GK_SCANCODE_8);
    SK(GK_SCANCODE_9);
    SK(GK_SCANCODE_Q);
    SK(GK_SCANCODE_W);
    SK(GK_SCANCODE_E);
    SK(GK_SCANCODE_R);
    SK(GK_SCANCODE_T);
    SK(GK_SCANCODE_Y);
    SK(GK_SCANCODE_U);
    SK(GK_SCANCODE_I);
    SK(GK_SCANCODE_O);
    SK(GK_SCANCODE_P);
    SK(GK_SCANCODE_A);
    SK(GK_SCANCODE_S);
    SK(GK_SCANCODE_D);
    SK(GK_SCANCODE_F);
    SK(GK_SCANCODE_G);
    SK(GK_SCANCODE_H);
    SK(GK_SCANCODE_J);
    SK(GK_SCANCODE_K);
    SK(GK_SCANCODE_L);
    SK(GK_SCANCODE_Z);
    SK(GK_SCANCODE_X);
    SK(GK_SCANCODE_C);
    SK(GK_SCANCODE_V);
    SK(GK_SCANCODE_B);
    SK(GK_SCANCODE_N);
    SK(GK_SCANCODE_M);
    SK(GK_SCANCODE_F1);
    SK(GK_SCANCODE_F2);
    SK(GK_SCANCODE_F3);
    SK(GK_SCANCODE_F4);
    SK(GK_SCANCODE_F5);
    SK(GK_SCANCODE_F6);
    SK(GK_SCANCODE_F7);
    SK(GK_SCANCODE_F8);
    SK(GK_SCANCODE_F9);
    SK(GK_SCANCODE_F10);
    SK(GK_SCANCODE_F11);
    SK(GK_SCANCODE_F12);
    SK(GK_SCANCODE_RETURN);
    SK(GK_SCANCODE_LCTRL);
    SK(GK_SCANCODE_LALT);
    SK(GK_SCANCODE_LSHIFT);
    SK(GK_SCANCODE_RCTRL);
    SK(GK_SCANCODE_RALT);
    SK(GK_SCANCODE_RSHIFT);
    SK(GK_SCANCODE_SPACE);
    SK(GK_SCANCODE_COMMA);
    SK(GK_SCANCODE_PERIOD);
    SK(GK_SCANCODE_MINUS);
    SK(GK_SCANCODE_EQUALS);
    SK(GK_SCANCODE_GRAVE);
    SK(GK_SCANCODE_KP_0);
    SK(GK_SCANCODE_KP_1);
    SK(GK_SCANCODE_KP_2);
    SK(GK_SCANCODE_KP_3);
    SK(GK_SCANCODE_KP_4);
    SK(GK_SCANCODE_KP_5);
    SK(GK_SCANCODE_KP_6);
    SK(GK_SCANCODE_KP_7);
    SK(GK_SCANCODE_KP_8);
    SK(GK_SCANCODE_KP_9);
    SK(GK_SCANCODE_KP_PERCENT);
    SK(GK_SCANCODE_KP_PERIOD);
    SK(GK_SCANCODE_KP_PLUS);
    SK(GK_SCANCODE_KP_PLUSMINUS);
    SK(GK_SCANCODE_KP_DIVIDE);
    SK(GK_SCANCODE_KP_ENTER);
    SK(GK_SCANCODE_KP_MULTIPLY);
    SK(GK_SCANCODE_KP_MINUS);
    SK(GK_SCANCODE_INSERT);
    SK(GK_SCANCODE_DELETE);
    SK(GK_SCANCODE_PRINTSCREEN);
    SK(GK_SCANCODE_LEFTBRACKET);
    SK(GK_SCANCODE_RIGHTBRACKET);
    SK(GK_SCANCODE_VOLUMEUP);
    SK(GK_SCANCODE_VOLUMEDOWN);
    SK(GK_SCANCODE_TAB);
    SK(GK_SCANCODE_WWW);
    SK(GK_SCANCODE_MENU);
    SK(GK_SCANCODE_APP1);
    SK(GK_SCANCODE_APP2);
    SK(GK_SCANCODE_APPLICATION);
    SK(GK_SCANCODE_PAUSE);
    SK(GK_MODIFIER_SHIFT);
    SK(GK_MODIFIER_CTRL);
    SK(GK_MODIFIER_ALT);
    return 0;
}

unsigned short str_to_key(const std::string &s)
{
    unsigned short ret = 0;
    size_t pos_start = 0, pos_end;
    while((pos_end = s.find("|", pos_start)) != std::string::npos)
    {
        std::string token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + 1;
        ret |= _str_to_key(token);
    }
    ret |= _str_to_key(s.substr(pos_start));
    return ret;
}
