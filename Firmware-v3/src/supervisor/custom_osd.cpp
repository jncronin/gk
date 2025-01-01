#include "widgets/widget.h"
#include "process.h"
#include "syscalls_int.h"
#include "INIReader.h"

WidgetAnimationList *GetAnimationList();

void Process::set_osd(const std::string &_osd_text)
{
    has_osd = true;
    osd_prepped = false;
    osd.clear();
    osd_text = _osd_text;
}

static std::vector<Widget *> _def_osd;
static bool _def_osd_init = false;
extern LabelWidget lab_caption;
extern GridWidget scr_overlay;
static ButtonWidget bw_exit;


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
            // TODO: make game-specific
            Event e[2];
            e[0].type = Event::KeyDown;
            e[0].key = GK_SCANCODE_F12;
            e[1].type = Event::KeyUp;
            e[1].key = GK_SCANCODE_F12;
            deferred_call(syscall_pushevents, fpid, e, 2);

            // backup quit incase the above didn't work
            AddAnimation(*GetAnimationList(), clock_cur_ms(), anim_handle_quit_failed, nullptr, (void *)fpid);
        }
    }
}

const std::vector<Widget *> &default_osd()
{
    if(!_def_osd_init)
    {
        // TODO: game customisation
        bw_exit.w = 80;
        bw_exit.h = 80;
        bw_exit.x = 0 + (scr_overlay.w - bw_exit.w) / 2;
        bw_exit.y = (scr_overlay.h - bw_exit.h) / 2;
        bw_exit.text = "Quit";
        bw_exit.OnClick = btn_exit_click;
        _def_osd.push_back(&bw_exit);
    }
    return _def_osd;
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

const std::vector<Widget *> &Process::get_osd()
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

        for(const auto &sect : rdr.SectionLinenums())
        {
            auto sname = rdr.GetSection(sect);
            for(const auto &key : rdr.KeyLinenums(sect))
            {
                auto kname = rdr.GetKey(key);
                auto kval = rdr.Get(key);

                klog("supervisor: osd: %s [%s=%s]\n", sname.c_str(),
                    kname.c_str(), kval.c_str());
            }

        }
    }
    return osd;
}