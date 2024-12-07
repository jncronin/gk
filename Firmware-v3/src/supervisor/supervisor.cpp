#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "screen.h"
#include "cache.h"
#include "_gk_event.h"
#include "widgets/widget.h"
#include "btnled.h"
#include "brightness.h"
#include "sound.h"
#include "buttons.h"
#include "pwr.h"
#include "gk_conf.h"
#include "syscalls_int.h"

SRAM4_DATA Process p_supervisor;
static SRAM4_DATA bool overlay_visible = false;
static SRAM4_DATA bool volume_visible = false;
SRAM4_DATA static WidgetAnimationList wl;

bool is_overlay_visible()
{
    return overlay_visible | volume_visible;
}

extern Condition scr_vsync;

[[maybe_unused]] static void *supervisor_thread(void *p);

static const constexpr coord_t btn_overlay_h = 208;
static const constexpr coord_t btn_overlay_y = 480 - btn_overlay_h;
static const constexpr coord_t status_h = 32;

ButtonWidget rw_test, rw_test2;
ImageButtonWidget imb_bright_up, imb_bright_down;
GridWidget scr_overlay, scr_status;
LabelWidget lab_caption;
KeyboardWidget kw;
ProgressBarWidget pb_volume;

static unsigned int scr_alpha = 0;
static alpha_t volume_alpha = 0;
static kernel_time last_volume_change = kernel_time();

void init_supervisor()
{
    p_supervisor.argc = 0;
    p_supervisor.argv = nullptr;
    p_supervisor.brk = 0;
    p_supervisor.code_data = InvalidMemregion();
    p_supervisor.cwd = "/";
    p_supervisor.default_affinity = M7Only;
    p_supervisor.for_deletion = false;
    p_supervisor.heap = InvalidMemregion();
    p_supervisor.name = "supervisor";
    p_supervisor.next_key = 0;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
        p_supervisor.open_files[i] = nullptr;
    p_supervisor.screen_h = 480;
    p_supervisor.screen_w = 640;
    p_supervisor.gamepad_is_joystick = false;
    p_supervisor.gamepad_is_keyboard = true;
    p_supervisor.gamepad_to_scancode[GK_KEYVOLUP] = GK_SCANCODE_VOLUMEUP;
    p_supervisor.gamepad_to_scancode[GK_KEYVOLDOWN] = GK_SCANCODE_VOLUMEDOWN;

    memcpy(p_supervisor.p_mpu, mpu_default, sizeof(mpu_default));
    
    auto t = Thread::Create("supervisor_main", supervisor_thread, nullptr, true, 2,
        p_supervisor, PreferM7);
    Schedule(t);
}

WidgetAnimationList *GetAnimationList()
{
    return &wl;
}

bool anim_handle_volume_change(Widget *wdg, void *p, time_ms_t t)
{
    /* We may have more than one of these running at once if user keeps clicking button
        Therefore don't use the 't' variable but just base on absolute times.
        Delete ourselves if > valid time */
    const constexpr time_ms_t solid_time = 2000;
    const constexpr time_ms_t fade_time = 150;
    const constexpr time_ms_t valid_time = solid_time + fade_time;

    auto tdiff = (clock_cur() - last_volume_change).to_ms();

    if(tdiff < solid_time)
    {
        volume_alpha = 255;
        volume_visible = true;
    }
    else if(tdiff >= valid_time)
    {
        volume_alpha = 0;
        volume_visible = false;
    }
    else
    {
        volume_alpha = Anim_Interp_Linear(255, 0, tdiff - solid_time, fade_time);
        volume_visible = true;
    }

    return t >= valid_time;
}

bool anim_showhide_overlay(Widget *wdg, void *p, time_ms_t t)
{
    bool is_show = p != nullptr;
    if(is_show)
    {
        overlay_visible = true;
    }
    
    const unsigned long long tot_time = 150ULL;
    if(t >= tot_time)
    {
        // final position
        if(is_show)
        {
            scr_overlay.y = btn_overlay_y;
            scr_status.y = 0;
            scr_alpha = 0xffU;
        }
        else
        {
            //screen_set_overlay_alpha(0x0U);
            scr_overlay.y = 480;
            scr_status.y = -scr_status.h;
            overlay_visible = false;
        }
        return true;
    }

    coord_t yadj = btn_overlay_h * t / tot_time;
    coord_t yadj_status = scr_status.h * t / tot_time;
    unsigned long aadj = 0x100UL * t / tot_time;
    if(aadj > 0xffU) aadj = 0xffU;

    if(is_show)
    {
        yadj = 480 - yadj;
        yadj_status = -status_h + yadj_status;
    }
    else
    {
        yadj = btn_overlay_y + yadj;
        yadj_status = -yadj_status;
        aadj = 0xffU - aadj;
    }
    scr_overlay.y = yadj;
    scr_status.y = yadj_status;
    scr_alpha = aadj;
    return false;
}

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

void btn_exit_click(Widget *w, coord_t x, coord_t y)
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
            AddAnimation(wl, clock_cur_ms(), anim_handle_quit_failed, nullptr, (void *)fpid);
        }
    }
}

void imb_brightness_click(Widget *w, coord_t x, coord_t y)
{
    if(w == &imb_bright_down)
    {
        screen_set_brightness(screen_get_brightness() - 10);
    }
    else
    {
        screen_set_brightness(screen_get_brightness() + 10);
    }
}

void kbd_click_up(Widget *w, coord_t x, coord_t y, int key)
{
    Event e { .type = Event::KeyUp, .key = (unsigned short)key };
    deferred_call(syscall_pushevents, deferred_call(syscall_get_focus_pid), &e, 1);
}

void kbd_click_down(Widget *w, coord_t x, coord_t y, int key)
{
    Event e { .type = Event::KeyDown, .key = (unsigned short)key };
    deferred_call(syscall_pushevents, deferred_call(syscall_get_focus_pid), &e, 1);
}

void *supervisor_thread(void *p)
{
    // Set up widgets

    // Main overlay screen:
    scr_overlay.x = 0;
    scr_overlay.y = btn_overlay_y;
    scr_overlay.w = 640;
    scr_overlay.h = btn_overlay_h;

    RectangleWidget rw;
    rw.x = 0;
    rw.y = 0;
    rw.w = scr_overlay.w * 3;
    rw.h = scr_overlay.h;
    rw.bg_inactive_color = 0x87;
    rw.border_width = 0;

    scr_overlay.AddChild(rw);

    // Screen 0 is specific to the game.  Without any extra customisation, it simply shows
    //  the game name and an exit button.
    coord_t cur_scr = 0;

    lab_caption.x = cur_scr;
    lab_caption.y = 0;
    lab_caption.w = scr_overlay.w;
    lab_caption.h = 32;
    lab_caption.bg_inactive_color = 0x87;
    lab_caption.border_width = 0;
    lab_caption.text = "GKMenu";
    scr_overlay.AddChild(lab_caption);

    // TODO: game customisation
    ButtonWidget bw_exit;
    bw_exit.w = 80;
    bw_exit.h = 80;
    bw_exit.x = cur_scr + (scr_overlay.w - bw_exit.w) / 2;
    bw_exit.y = (scr_overlay.h - bw_exit.h) / 2;
    bw_exit.text = "Quit";
    bw_exit.OnClick = btn_exit_click;
    scr_overlay.AddChildOnGrid(bw_exit);

    // Screen 2 is options
    cur_scr += 640;

    LabelWidget l_options;
    l_options.x = cur_scr;
    l_options.y = 0;
    l_options.w = scr_overlay.w;
    l_options.h = 32;
    l_options.bg_inactive_color = 0x87;
    l_options.border_width = 0;
    l_options.text = "Options";
    scr_overlay.AddChild(l_options);

    imb_bright_down.x = cur_scr + 64;
    imb_bright_down.w = 80;
    imb_bright_down.y = 64;
    imb_bright_down.h = 80;
    imb_bright_down.image = brightness_down;
    imb_bright_down.img_w = 64;
    imb_bright_down.img_h = 64;
    imb_bright_down.OnClick = imb_brightness_click;
    scr_overlay.AddChildOnGrid(imb_bright_down);

    imb_bright_up.x = imb_bright_down.x + imb_bright_down.w + 32;
    imb_bright_up.w = 80;
    imb_bright_up.y = 64;
    imb_bright_up.h = 80;
    imb_bright_up.image = brightness_up;
    imb_bright_up.img_w = 64;
    imb_bright_up.img_h = 64;
    imb_bright_up.OnClick = imb_brightness_click;
    scr_overlay.AddChildOnGrid(imb_bright_up);

    // Screen 3 is an on-screen keyboard
    cur_scr += 640;

    kw.x = cur_scr + (scr_overlay.w - kw.w) / 2;
    kw.y = 8;
    kw.OnKeyboardButtonClick = kbd_click_up;
    kw.OnKeyboardButtonClickBegin = kbd_click_down;
    scr_overlay.AddChildOnGrid(kw);

    // Volume control    
    pb_volume.x = 640-64-32;
    pb_volume.w = 64;
    pb_volume.y = 48;
    pb_volume.h = 192;
    pb_volume.bg_inactive_color = 0x87;
    pb_volume.border_width = 0;
    pb_volume.SetOrientation(ProgressBarWidget::Orientation_t::BottomToTop);
    pb_volume.SetMaxValue(100);
    pb_volume.SetCurValue(sound_get_volume());
    pb_volume.pad = 12;

    // Status bar
    scr_status.x = 0;
    scr_status.y = -status_h;
    scr_status.w = 640;
    scr_status.h = status_h;

    RectangleWidget rw_status;
    rw_status.x = 0;
    rw_status.y = 0;
    rw_status.w = scr_status.w;
    rw_status.h = scr_status.h;
    rw_status.bg_inactive_color = 0x87;
    rw_status.border_width = 0;

    LabelWidget l_time;
    l_time.x = 0;
    l_time.y = 0;
    l_time.h = scr_status.h;
    l_time.w = 320;
    l_time.text_hoffset = HOffset::Left;
    l_time.text = "Unknown";
    l_time.border_width = 0;
    l_time.bg_inactive_color = 0x87;

    scr_status.AddChild(rw_status);
    scr_status.AddChild(l_time);

    // process messages
    while(true)
    {
        Event e;
        [[maybe_unused]] bool do_update = false;
        [[maybe_unused]] bool do_volume_update = false;
        bool has_event = p_supervisor.events.Pop(&e,
            HasAnimations(wl) ? kernel_time::from_ms(1000ULL / 60ULL) : kernel_time::from_ms(1000));
        if(RunAnimations(wl, clock_cur_ms()))
        {
            do_update = true;
            do_volume_update = true;
        }

        static kernel_time last_temp_report;
        if(clock_cur() > (last_temp_report + kernel_time::from_ms(1000)))
        {
            // dump temp to klog, eventually to screen
            auto temp = temp_get_core();
            auto vdd = pwr_get_vdd();

            klog("supervisor: temp: %fC, vdd: %fV, SBS->CCVALR: %x, SBS->CCSWVALR: %x\n",
                temp, vdd, SBS->CCVALR, SBS->CCSWVALR);

            last_temp_report = clock_cur();
        }

        if(has_event)
        {
            switch(e.type)
            {
                case Event::KeyDown:
                    switch(e.key)
                    {
                        case GK_SCANCODE_MENU:
                        {
                            if(overlay_visible)
                            {
                                AddAnimation(wl, clock_cur_ms(), anim_showhide_overlay, &scr_overlay, (void*)0);
                            }
                            else
                            {
                                AddAnimation(wl, clock_cur_ms(), anim_showhide_overlay, &scr_overlay, (void*)1);
                            }
                            /*overlay_visible = !overlay_visible;
                            if(overlay_visible)
                            {
                                do_update = true;
                            }
                            else
                            {
                                screen_set_overlay_alpha(0x0U);
                            }*/
                        }
                        break;

                        case GK_SCANCODE_UP:
                        case GK_SCANCODE_DOWN:
                        case GK_SCANCODE_LEFT:
                        case GK_SCANCODE_RIGHT:
                        case GK_SCANCODE_A:
                        case GK_SCANCODE_LCTRL:
                        case GK_SCANCODE_RETURN:
                        {
                            auto ck = e.key;
                            if(ck == GK_SCANCODE_A || ck == GK_SCANCODE_LCTRL)
                            {
                                ck = GK_SCANCODE_RETURN;
                            }
                            scr_overlay.KeyPressDown(ck);
                            do_update = true;
                        }
                        break;

                        case GK_SCANCODE_VOLUMEDOWN:
                        case GK_SCANCODE_VOLUMEUP:
                            if(e.key == GK_SCANCODE_VOLUMEDOWN)
                                sound_set_volume(sound_get_volume() - 10);
                            else
                                sound_set_volume(sound_get_volume() + 10);

                            volume_visible = true;
                            do_volume_update = true;
                            last_volume_change = clock_cur();

                            pb_volume.SetCurValue(sound_get_volume());

                            AddAnimation(wl, clock_cur_ms(), anim_handle_volume_change, &pb_volume, nullptr);

                            klog("supervisor: sound set to %d\n", sound_get_volume());
                            break;
                    }
                    break;

                case Event::KeyUp:
                    switch(e.key)
                    {
                        case GK_SCANCODE_UP:
                        case GK_SCANCODE_DOWN:
                        case GK_SCANCODE_LEFT:
                        case GK_SCANCODE_RIGHT:
                        case GK_SCANCODE_A:
                        case GK_SCANCODE_LCTRL:
                        case GK_SCANCODE_RETURN:
                        {
                            auto ck = e.key;
                            if(ck == GK_SCANCODE_A || ck == GK_SCANCODE_LCTRL)
                            {
                                ck = GK_SCANCODE_RETURN;
                            }
                            scr_overlay.KeyPressUp(ck);
                            do_update = true;
                        }
                    }
                    break;

                case Event::CaptionChange:
                    {
                        const auto &capt = focus_process ? 
                            (focus_process->window_title.empty() ?
                                focus_process->name :
                                focus_process->window_title) :
                            "GK";

                        const auto &scr_capt = focus_process ?
                            (" (" + std::to_string(focus_process->screen_w) + "x" +
                            std::to_string(focus_process->screen_h) + ")") : "";

                        lab_caption.text = capt + scr_capt;
                        do_update = true;
                    }
                    break;

                default:
                    // ignore
                    break;
            }
        }


        if(overlay_visible || (volume_visible && do_volume_update))
        {
            auto fb = (color_t *)screen_get_overlay_frame_buffer();
            // clear screen
            for(coord_t cy = 0; cy < fb_h; cy++)
            {
                for(coord_t cx = 0; cx < fb_w; cx++)
                {
                    fb[cx + cy * fb_stride] = 0;
                }
            }

            if(overlay_visible)
            {
                // update time/date
                timespec tp;
                clock_get_now(&tp);
                auto t = localtime(&tp.tv_sec);
                char buf[64];
                strftime(buf, 63, "%F %T", t);
                buf[63] = 0;
                l_time.text = std::string(buf);

                scr_overlay.Update(scr_alpha);
                scr_status.Update(scr_alpha);
            }
            if(volume_visible)
                pb_volume.Update(volume_alpha);
            screen_flip_overlay(nullptr, true, 255);
            scr_vsync.Wait();
        }
        else if(!is_overlay_visible())
        {
            screen_flip_overlay(nullptr, false, 0);
        }
    }

    return nullptr;
}

bool supervisor_is_active(unsigned int *x, unsigned int *y, unsigned int *w, unsigned int *h)
{
    if(overlay_visible)
    {
        if(x) *x = 0;
        if(y) *y = 240;
        if(w) *w = 640;
        if(h) *h = 240;
    }
    return overlay_visible; 
}
