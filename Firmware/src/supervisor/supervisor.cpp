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
#include "gk_conf.h"

SRAM4_DATA Process p_supervisor;
SRAM4_DATA static bool overlay_visible = false;
SRAM4_DATA static WidgetAnimationList wl;

extern Condition scr_vsync;

static void *supervisor_thread(void *p);

ButtonWidget rw_test, rw_test2;
ImageButtonWidget imb_bright_up, imb_bright_down;
GridWidget scr_test;
LabelWidget lab_caption;
KeyboardWidget kw;
static unsigned int scr_alpha = 0;

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
    
    auto t = Thread::Create("supervisor_main", supervisor_thread, nullptr, true, 2,
        p_supervisor, PreferM7);
    Schedule(t);
}

void test_onclick(Widget *w, coord_t x, coord_t y)
{
    static int col = 0;
    
    btnled_setcolor(0x0fU << (col * 8));
    col++;
    if(col >= 3) col = 0;
}

bool anim_showhide_overlay(Widget *wdg, void *p, unsigned long long int t)
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
            wdg->y = 240;
            scr_alpha = 0xffU;
        }
        else
        {
            screen_set_overlay_alpha(0x0U);
            overlay_visible = false;
        }
        return true;
    }

    coord_t yadj = 240ULL * t / tot_time;
    unsigned long aadj = 0x100UL * t / tot_time;
    if(aadj > 0xffU) aadj = 0xffU;

    if(is_show)
    {
        yadj = 480 - yadj;
    }
    else
    {
        yadj = 240 + yadj;
        aadj = 0xffU - aadj;
    }
    wdg->y = yadj;
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

void kbd_click_up(Widget *w, coord_t x, coord_t y, int key)
{
    focus_process->events.Push({ .type = Event::KeyUp, .key = (unsigned short)key });
}

void kbd_click_down(Widget *w, coord_t x, coord_t y, int key)
{
    focus_process->events.Push({ .type = Event::KeyDown, .key = (unsigned short)key });
}

void *supervisor_thread(void *p)
{
    rw_test.x = 32;
    rw_test.w = 200;
    rw_test.y = 64;
    rw_test.h = 100;
    rw_test.text = "Hi there";
    rw_test.OnClick = test_onclick;

    rw_test2.x = 264;
    rw_test2.w = 200;
    rw_test2.y = 64;
    rw_test2.h = 100;
    rw_test2.text = "Eh?";

    imb_bright_down.x = 640-80-16-80-16;
    imb_bright_down.w = 80;
    imb_bright_down.y = 64;
    imb_bright_down.h = 80;
    imb_bright_down.image = brightness_down;
    imb_bright_down.img_w = 64;
    imb_bright_down.img_h = 64;
    imb_bright_down.OnClick = imb_brightness_click;

    imb_bright_up.x = 640-80-16;
    imb_bright_up.w = 80;
    imb_bright_up.y = 64;
    imb_bright_up.h = 80;
    imb_bright_up.image = brightness_up;
    imb_bright_up.img_w = 64;
    imb_bright_up.img_h = 64;
    imb_bright_up.OnClick = imb_brightness_click;

    lab_caption.x = 16;
    lab_caption.y = 16;
    lab_caption.w = 640 - 32;
    lab_caption.h = 32;
    lab_caption.text = "GK";

    scr_test.x = 0;
    scr_test.y = 240;
    scr_test.w = 640;
    scr_test.h = 240;

    RectangleWidget rw;
    rw.x = 0;
    rw.y = 0;
    rw.w = fb_w;
    rw.h = fb_h/2;
    rw.bg_inactive_color = 0x87;
    rw.border_width = 0;

    scr_test.AddChild(rw);
    //scr_test.AddChildOnGrid(rw_test);
    //scr_test.AddChildOnGrid(rw_test2);
    //scr_test.AddChildOnGrid(imb_bright_down);
    //scr_test.AddChildOnGrid(imb_bright_up);
    //scr_test.AddChild(lab_caption);

    kw.x = (640 - kw.w) / 2;
    kw.y = 8;
    kw.OnKeyboardButtonClick = kbd_click_up;
    kw.OnKeyboardButtonClickBegin = kbd_click_down;
    scr_test.AddChildOnGrid(kw);

    Widget *cur_scr = &scr_test;

    // process messages
    while(true)
    {
        Event e;
        bool do_update = false;
        bool has_event = p_supervisor.events.Pop(&e,
            HasAnimations(wl) ? kernel_time::from_ms(1000ULL / 60ULL) : kernel_time());
        if(RunAnimations(wl, clock_cur_ms()))
        {
            do_update = true;
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
                                AddAnimation(wl, clock_cur_ms(), anim_showhide_overlay, &scr_test, (void*)0);
                            }
                            else
                            {
                                AddAnimation(wl, clock_cur_ms(), anim_showhide_overlay, &scr_test, (void*)1);
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
                            cur_scr->KeyPressDown(ck);
                            do_update = true;
                        }
                        break;

                        case GK_SCANCODE_VOLUMEDOWN:
                            sound_set_volume(sound_get_volume() - 10);
                            // fallthrough
                        case GK_SCANCODE_VOLUMEUP:
                            sound_set_volume(sound_get_volume() + 10);

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
                            cur_scr->KeyPressUp(ck);
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

        if(overlay_visible && do_update)
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

            cur_scr->Update();
            screen_flip_overlay(true, scr_alpha);
            scr_vsync.Wait();
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
