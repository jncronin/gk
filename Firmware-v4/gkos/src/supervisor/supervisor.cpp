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
#include "pwr.h"
#include "gk_conf.h"
#include "syscalls_int.h"
#include <array>
#include "supervisor.h"
#include "power_images.h"
#include "wifi_airoc_if.h"

PProcess p_supervisor;
static bool overlay_visible = false;
static bool volume_visible = false;
static WidgetAnimationList wl;

GridWidget *default_osd();

const int n_screens = 5;
Widget *scrs[n_screens] = { 0 };

bool is_overlay_visible()
{
    return overlay_visible | volume_visible;
}

extern Condition scr_vsync;

[[maybe_unused]] static void *supervisor_thread(void *p);

static const constexpr coord_t btn_overlay_h = 208;
static const constexpr coord_t btn_overlay_y = 480 - btn_overlay_h;
static const constexpr coord_t status_h = 32;

ImageButtonWidget imb_bright_up, imb_bright_down;
ButtonWidget btn_wifi, btn_rawsd;
GridWidget scr_overlay, scr_status, scr1, scr2;
LabelWidget lab_caption;
KeyboardWidget kw;
ProgressBarWidget pb_volume;

static unsigned int scr_alpha = 0;
static alpha_t volume_alpha = 0;
static kernel_time last_volume_change = kernel_time();

static uintptr_t overlay_fb = 0;

void *screen_get_overlay_frame_buffer()
{
    return (void *)overlay_fb;
}

bool init_supervisor()
{
    p_supervisor = Process::Create("supervisor", true);
    p_supervisor->screen.screen_w = 800;
    p_supervisor->screen.screen_h = 480;
    p_supervisor->screen.screen_pf = GK_PIXELFORMAT_A4L4;
    p_supervisor->screen.screen_layer = 1;
    p_supervisor->screen.clut.clear();
    constexpr uint32_t palette[] = {
        0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
        0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
        0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
        0xff5555, 0xff55ff, 0xffff55, 0xffffff };
    for(auto pal : palette)
    {
        p_supervisor->screen.clut.push_back(pal);
    }
    p_supervisor->screen.new_clut = true;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYVOLUP] = GK_SCANCODE_VOLUMEUP;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYVOLDOWN] = GK_SCANCODE_VOLUMEDOWN;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYLEFT] = GK_SCANCODE_LEFT;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYRIGHT] = GK_SCANCODE_RIGHT;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYUP] = GK_SCANCODE_UP;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYDOWN] = GK_SCANCODE_DOWN;
    p_supervisor->keymap.gamepad_to_scancode[GK_KEYA] = GK_SCANCODE_RETURN;

    klog("kernel: starting supervisor\n");

    /* Reset all static variables in case of restart */
    overlay_visible = false;
    volume_visible = false;
    wl.clear();
    for(int i = 0; i < n_screens; i++)
    {
        if(scrs[i])
            scrs[i]->Clear();
        scrs[i] = nullptr;
    }
    scr_alpha = 0;
    volume_alpha = 0;
    last_volume_change = kernel_time();
    
    auto t = Thread::Create("supervisor_main", supervisor_thread, nullptr, true, GK_PRIORITY_NORMAL,
        p_supervisor);
    Schedule(t);

    return true;
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

    auto tdiff = kernel_time_to_ms(clock_cur() - last_volume_change);

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

static void btn_wifi_click(Widget *w, coord_t x, coord_t y)
{
    extern std::unique_ptr<WifiAirocNetInterface> airoc_if;
    airoc_if->SetActive(!airoc_if->GetLinkActive());
}

static void btn_rawsd_click(Widget *w, coord_t x, coord_t y)
{
    /*
    if(reboot_flags & GK_REBOOTFLAG_RAWSD)
    {
        reboot_flags &= ~GK_REBOOTFLAG_RAWSD;
        w->SetClickedAppearance(false);
    }
    else
    {
        reboot_flags |= GK_REBOOTFLAG_RAWSD;
        w->SetClickedAppearance(true);
    }
    */
}

void kbd_click_up(Widget *w, coord_t x, coord_t y, int key, bool is_shift, bool is_ctrl, bool is_alt)
{
    if(is_shift)
        key |= GK_MODIFIER_SHIFT;
    if(is_ctrl)
        key |= GK_MODIFIER_CTRL;
    if(is_alt)
        key |= GK_MODIFIER_ALT;
    Event e { .type = Event::KeyUp, .key = (unsigned short)key };
    auto focus_process = GetFocusProcess();
    if(focus_process)
    {
        focus_process->events.Push(e);
    }
}

void kbd_click_down(Widget *w, coord_t x, coord_t y, int key, bool is_shift, bool is_ctrl, bool is_alt)
{
    if(is_shift)
        key |= GK_MODIFIER_SHIFT;
    if(is_ctrl)
        key |= GK_MODIFIER_CTRL;
    if(is_alt)
        key |= GK_MODIFIER_ALT;
    Event e { .type = Event::KeyDown, .key = (unsigned short)key };
    auto focus_process = GetFocusProcess();
    if(focus_process)
    {
        focus_process->events.Push(e);
    }
}

void *supervisor_thread(void *p)
{
    overlay_fb = screen_update();

    // Set up widgets

    // Store the widgets that form part of the custom osd
    std::vector<Widget *> custom_osd;

    // Main overlay screen:
    scr_overlay.Clear();
    scr_overlay.x = 0;
    scr_overlay.y = btn_overlay_y;
    scr_overlay.w = GK_SCREEN_WIDTH;
    scr_overlay.h = btn_overlay_h;

    // Screen 0 is specific to the game.  Without any extra customisation, it simply shows
    //  the game name and an exit button.
    coord_t cur_scr = 0;
    auto focus_process = GetFocusProcess();
    //auto scr0 = focus_process ? focus_process->get_osd() : (Widget *)default_osd();
    auto scr0 = (Widget *)default_osd();
    scr0->x = cur_scr;

    // Screen 1 is options
    cur_scr += GK_SCREEN_WIDTH;
    scr1.Clear();
    scr1.x = cur_scr;
    scr1.y = 0;
    scr1.w = scr_overlay.w;
    scr1.h = scr_overlay.h;

    LabelWidget l_options;
    l_options.x = 0;
    l_options.y = 0;
    l_options.w = scr_overlay.w;
    l_options.h = 32;
    l_options.bg_inactive_color = 0xd4;
    l_options.border_width = 0;
    l_options.text = "Options";
    scr1.AddChild(l_options);

    imb_bright_down.x = 64;
    imb_bright_down.w = 80;
    imb_bright_down.y = 64;
    imb_bright_down.h = 80;
    imb_bright_down.image = brightness_down;
    imb_bright_down.img_w = 64;
    imb_bright_down.img_h = 64;
    imb_bright_down.OnClick = imb_brightness_click;
    scr1.AddChildOnGrid(imb_bright_down);

    imb_bright_up.x = imb_bright_down.x + imb_bright_down.w + 32;
    imb_bright_up.w = 80;
    imb_bright_up.y = 64;
    imb_bright_up.h = 80;
    imb_bright_up.image = brightness_up;
    imb_bright_up.img_w = 64;
    imb_bright_up.img_h = 64;
    imb_bright_up.OnClick = imb_brightness_click;
    scr1.AddChildOnGrid(imb_bright_up);

    btn_wifi.x = imb_bright_up.x + imb_bright_up.w + 32;
    btn_wifi.w = 80;
    btn_wifi.y = 64;
    btn_wifi.h = 80;
    btn_wifi.text = "Wifi";
    btn_wifi.OnClick = btn_wifi_click;
    scr1.AddChildOnGrid(btn_wifi);

    btn_rawsd.x = btn_wifi.x + btn_wifi.w + 32;
    btn_rawsd.w = 96;
    btn_rawsd.y = 64;
    btn_rawsd.h = 80;
    btn_rawsd.text = "Raw SD";
    btn_rawsd.OnClick = btn_rawsd_click;
    scr1.AddChildOnGrid(btn_rawsd);

    // Screen 2 is an on-screen keyboard
    cur_scr += GK_SCREEN_WIDTH;
    scr2.Clear();
    scr2.x = cur_scr;
    scr2.y = 0;
    scr2.w = scr_overlay.w;
    scr2.h = scr_overlay.h;

    kw.x = (scr_overlay.w - kw.w) / 2;
    kw.y = 8;
    kw.OnKeyboardButtonClick = kbd_click_up;
    kw.OnKeyboardButtonClickBegin = kbd_click_down;
    scr2.AddChildOnGrid(kw);

    // Add screens to main overlay
    scr_overlay.AddChildOnGrid(*scr0);
    scr_overlay.AddChildOnGrid(scr1);
    scr_overlay.AddChildOnGrid(scr2);

    scrs[0] = scr0;
    scrs[1] = &scr1;
    scrs[2] = &scr2;

    // Volume control    
    pb_volume.x = GK_SCREEN_WIDTH-64-32;
    pb_volume.w = 64;
    pb_volume.y = 48;
    pb_volume.h = 192;
    pb_volume.bg_inactive_color = 0xd4;
    pb_volume.border_width = 0;
    pb_volume.SetOrientation(ProgressBarWidget::Orientation_t::BottomToTop);
    pb_volume.SetMaxValue(100);
    pb_volume.SetCurValue(sound_get_volume());
    pb_volume.pad = 12;

    // Status bar
    scr_status.Clear();
    scr_status.x = 0;
    scr_status.y = -status_h;
    scr_status.w = GK_SCREEN_WIDTH;
    scr_status.h = status_h;

    RectangleWidget rw_status;
    rw_status.x = 0;
    rw_status.y = 0;
    rw_status.w = scr_status.w;
    rw_status.h = scr_status.h;
    rw_status.bg_inactive_color = 0xd4;
    rw_status.border_width = 0;

    LabelWidget l_time;
    l_time.x = 0;
    l_time.y = 0;
    l_time.h = scr_status.h;
    l_time.w = 320;
    l_time.text_hoffset = HOffset::Left;
    l_time.text = "Unknown";
    l_time.border_width = 0;
    l_time.bg_inactive_color = 0xd4;

    ImageWidget i_pwr;
    i_pwr.x = scr_status.w - 40;
    i_pwr.y = 0;
    i_pwr.w = 40;
    i_pwr.h = 32;
    i_pwr.border_width = 0;
    i_pwr.bg_inactive_color = 0xd4;
    i_pwr.img_h = 20;
    i_pwr.img_w = 40;
    i_pwr.img_hoffset = HOffset::Left;
    i_pwr.img_voffset = VOffset::Middle;
    i_pwr.image = battery_full;
    i_pwr.img_bpp = 4;

    ImageWidget i_charge;
    i_charge.x = i_pwr.x - 22;
    i_charge.y = 0;
    i_charge.w = 22;
    i_charge.h = 32;
    i_charge.border_width = 0;
    i_charge.bg_inactive_color = 0xd4;
    i_charge.img_w = 22;
    i_charge.img_h = 32;
    i_charge.img_hoffset = HOffset::Centre;
    i_charge.img_voffset = VOffset::Middle;
    i_charge.image = charge;
    i_charge.img_bpp = 4;

    ImageWidget i_wifi;
    i_wifi.x = i_charge.x - 40;
    i_wifi.y = 0;
    i_wifi.w = 40;
    i_wifi.h = 32;
    i_wifi.border_width = 0;
    i_wifi.bg_inactive_color = 0xd4;
    i_wifi.img_w = 40;
    i_wifi.img_h = 29;
    i_wifi.img_hoffset = HOffset::Centre;
    i_wifi.img_voffset = VOffset::Middle;
    i_wifi.image = wifi;
    i_wifi.img_bpp = 4;

    ImageWidget i_usb;
    i_usb.x = i_wifi.x - 41;
    i_usb.y = 0;
    i_usb.w = 41;
    i_usb.h = 32;
    i_usb.border_width = 0;
    i_usb.bg_inactive_color = 0xd4;
    i_usb.img_w = 41;
    i_usb.img_h = 26;
    i_usb.img_hoffset = HOffset::Centre;
    i_usb.img_voffset = VOffset::Middle;
    i_usb.image = usb;
    i_usb.img_bpp = 4;

    ImageWidget i_bt;
    i_bt.x = i_usb.x - 24;
    i_bt.y = 0;
    i_bt.w = 24;
    i_bt.h = 32;
    i_bt.border_width = 0;
    i_bt.bg_inactive_color = 0xd4;
    i_bt.img_w = 24;
    i_bt.img_h = 32;
    i_bt.img_hoffset = HOffset::Centre;
    i_bt.img_voffset = VOffset::Middle;
    i_bt.image = bt;
    i_bt.img_bpp = 4;

    scr_status.AddChild(rw_status);
    scr_status.AddChild(l_time);
    scr_status.AddChild(i_pwr);
    scr_status.AddChild(i_charge);
    scr_status.AddChild(i_wifi);
    scr_status.AddChild(i_usb);
    scr_status.AddChild(i_bt);

    auto supervisor_start_time = clock_cur();
    kernel_time last_status_update = kernel_time_invalid();

    // process messages
    while(true)
    {
        Event e;
        [[maybe_unused]] bool do_update = false;
        [[maybe_unused]] bool do_volume_update = false;
        bool has_event = p_supervisor->events.Pop(&e,
            HasAnimations(wl) ? kernel_time_from_ms(1000ULL / 60ULL) : kernel_time_from_ms(1000));
        if(RunAnimations(wl, clock_cur_ms()))
        {
            do_update = true;
            do_volume_update = true;
        }

        if(has_event)
        {
            focus_process = GetFocusProcess();

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
                        break;

                        case GK_SCANCODE_POWER:
                            // perform power off on button up - prevents resetting whilst still pressed
                            //  and then power off during startup
                            // ignore spurious button presses at startup
                            if(clock_cur() >= supervisor_start_time + kernel_time_from_ms(500))
                            {
                                supervisor_shutdown_system();
                            }
                            break;
                    }
                    break;

                case Event::CaptionChange:
                    {
                        const auto &capt = focus_process ? 
                            (focus_process->window_title.empty() ?
                                focus_process->name :
                                focus_process->window_title) :
                            "GK";

                        int bpp = 0;
                        switch(focus_process->screen.screen_pf)
                        {
                            case GK_PIXELFORMAT_ARGB8888:
                                bpp = 32;
                                break;
                            case GK_PIXELFORMAT_RGB888:
                                bpp = 24;
                                break;
                            case GK_PIXELFORMAT_RGB565:
                                bpp = 16;
                                break;
                            case GK_PIXELFORMAT_L8:
                                bpp = 8;
                                break;
                        }

                        const auto &scr_capt = focus_process ?
                            (" (" + std::to_string(focus_process->screen.screen_w) + "x" +
                            std::to_string(focus_process->screen.screen_h) + "x" +
                            std::to_string(bpp) + "@" +
                            std::to_string(focus_process->screen.screen_refresh) +                          
                            ")") : "";

                        lab_caption.text = capt + scr_capt;

                        //auto new_osd = focus_process ?
                        //    focus_process->get_osd() :
                        //    default_osd();
                        auto new_osd = default_osd();

                        // set new osd
                        new_osd->x = scrs[0]->x;
                        new_osd->y = scrs[0]->y;
                        scr_overlay.ReplaceChildOnGrid(*scrs[0], *new_osd);
                        scrs[0] = new_osd;
                        for(auto cosd : custom_osd)
                        {
                            scr_overlay.RemoveChild(*cosd);
                        }
                        
                        do_update = true;
                    }
                    break;

                case Event::SupervisorSetVisible:
                    AddAnimation(wl, clock_cur_ms(), anim_showhide_overlay, &scr_overlay, (void*)(int)e.setvis_data.visible);
                    if(e.setvis_data.screen >= 0 && e.setvis_data.screen < n_screens)
                        scr_overlay.SetHighlightedChild(*scrs[e.setvis_data.screen]);
                    break;

                default:
                    // ignore
                    break;
            }
        }

        if(overlay_visible || (volume_visible && do_volume_update))
        {
            
            auto fb = (color_t *)overlay_fb;
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
                if(!kernel_time_is_valid(last_status_update) || clock_cur() >= (last_status_update + kernel_time_from_ms(1000)))
                {
                    // update time/date
                    timespec tp;
                    clock_get_realtime(&tp);
                    auto t = localtime(&tp.tv_sec);
                    char buf[64];
                    strftime(buf, 63, "%F %T", t);
                    buf[63] = 0;
                    char buf_line[128];
                    snprintf(buf_line, 127, "%s %.1f FPS %.1fC %.2fV %.2fW CPU %.2f", buf, screen_get_fps(), (double)tavg,
                        (double)vsys, (double)psys, sched.CPUUsage());
                    buf_line[127] = 0;
                    l_time.text = std::string(buf_line);

                    klog("supervisor: %s\n", buf_line);

                    // update status images
                    /*
                    pwr_status pstat;
                    pwr_get_status(&pstat);
                    if(pstat.battery_present)
                    {
                        if(pstat.is_full)
                            i_pwr.image = battery_full;
                        else if(pstat.state_of_charge >= 75.0)
                            i_pwr.image = battery_3;
                        else if(pstat.state_of_charge >= 50.0)
                            i_pwr.image = battery_2;
                        else if(pstat.state_of_charge >= 25.0)
                            i_pwr.image = battery_1;
                        else
                            i_pwr.image = battery_empty;
                        i_pwr.visible = true;
                    }
                    else
                    {
                        i_pwr.visible = false;
                    }
                    i_charge.visible = pstat.is_charging;

                    extern TUSBNetInterface rndis_if;
                    i_usb.visible = rndis_if.GetLinkActive();
                    i_bt.visible = false;   // TODO
                    */

                    extern std::unique_ptr<WifiAirocNetInterface> airoc_if;
                    i_wifi.visible = airoc_if && airoc_if->GetLinkActive();

                    i_charge.visible = pmic_vbus_ok.load();
                    i_wifi.visible = false;
                    i_usb.visible = false;
                    i_bt.visible = false;

                    last_status_update = clock_cur();
                }

                scr_overlay.Update(scr_alpha);
                scr_status.Update(scr_alpha);
            }
            if(volume_visible)
                pb_volume.Update(volume_alpha);
            overlay_fb = screen_update();
            //screen_flip_overlay(nullptr, true, 255);
            //scr_vsync.Wait();
        }
        else if(!is_overlay_visible())
        {
            //screen_flip_overlay(nullptr, false, 0);
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
        if(w) *w = 800;
        if(h) *h = 240;
    }
    return overlay_visible; 
}

int syscall_setsupervisorvisible(int visible, int screen, int *_errno)
{
    Event e;
    e.type = Event::SupervisorSetVisible;
    e.setvis_data.visible = visible;
    e.setvis_data.screen = screen;
    p_supervisor->events.Push(e);
    return 0;
}
