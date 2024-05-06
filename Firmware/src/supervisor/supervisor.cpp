#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "screen.h"
#include "cache.h"
#include "_gk_event.h"
#include "widgets/widget.h"
#include "btnled.h"
#include "brightness.h"
#include "gk_conf.h"

SRAM4_DATA Process p_supervisor;
SRAM4_DATA static bool overlay_visible = false;
SRAM4_DATA static WidgetAnimationList wl;

extern Condition scr_vsync;

static void *supervisor_thread(void *p);

ButtonWidget rw_test, rw_test2;
ImageButtonWidget imb_bright_up, imb_bright_down;
GridWidget scr_test;
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
    p_supervisor.mr_params = InvalidMemregion();
    p_supervisor.name = "supervisor";
    p_supervisor.next_key = 0;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
        p_supervisor.open_files[i] = nullptr;
    p_supervisor.screen_h = 480;
    p_supervisor.screen_w = 640;
    p_supervisor.gamepad_is_joystick = false;
    p_supervisor.gamepad_is_keyboard = true;
    
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

void *supervisor_thread(void *p)
{
    rw_test.x = 32;
    rw_test.w = 200;
    rw_test.y = 32;
    rw_test.h = 100;
    rw_test.text = "Hi there";
    rw_test.OnClick = test_onclick;

    rw_test2.x = 264;
    rw_test2.w = 200;
    rw_test2.y = 32;
    rw_test2.h = 100;
    rw_test2.text = "Eh?";

    imb_bright_down.x = 640-80-16-80-16;
    imb_bright_down.w = 80;
    imb_bright_down.y = 32;
    imb_bright_down.h = 80;
    imb_bright_down.image = brightness_down;
    imb_bright_down.img_w = 64;
    imb_bright_down.img_h = 64;
    imb_bright_down.OnClick = imb_brightness_click;

    imb_bright_up.x = 640-80-16;
    imb_bright_up.w = 80;
    imb_bright_up.y = 32;
    imb_bright_up.h = 80;
    imb_bright_up.image = brightness_up;
    imb_bright_up.img_w = 64;
    imb_bright_up.img_h = 64;
    imb_bright_up.OnClick = imb_brightness_click;

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
    scr_test.AddChildOnGrid(rw_test);
    //scr_test.AddChildOnGrid(rw_test2);
    scr_test.AddChildOnGrid(imb_bright_down);
    scr_test.AddChildOnGrid(imb_bright_up);

    Widget *cur_scr = &scr_test;

    // process messages
    while(true)
    {
        Event e;
        bool do_update = false;
        bool has_event = p_supervisor.events.Pop(&e, 1000ULL / 60ULL);
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
                        case ' ':
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

                        case (unsigned short)Scancodes::KeyUp:
                        case (unsigned short)Scancodes::KeyDown:
                        case (unsigned short)Scancodes::KeyLeft:
                        case (unsigned short)Scancodes::KeyRight:
                        case (unsigned short)Scancodes::KeyA:
                        case (unsigned short)Scancodes::KeyLCtrl:
                        {
                            auto ck = (Scancodes)e.key;
                            if(ck == Scancodes::KeyA || ck == Scancodes::KeyLCtrl)
                            {
                                ck = Scancodes::KeyEnter;
                            }
                            cur_scr->KeyPressDown(ck);
                            do_update = true;
                        }
                    }
                    break;

                case Event::KeyUp:
                    switch(e.key)
                    {
                        case (unsigned short)Scancodes::KeyUp:
                        case (unsigned short)Scancodes::KeyDown:
                        case (unsigned short)Scancodes::KeyLeft:
                        case (unsigned short)Scancodes::KeyRight:
                        case (unsigned short)Scancodes::KeyA:
                        case (unsigned short)Scancodes::KeyLCtrl:
                        {
                            auto ck = (Scancodes)e.key;
                            if(ck == Scancodes::KeyA || ck == Scancodes::KeyLCtrl)
                            {
                                ck = Scancodes::KeyEnter;
                            }
                            cur_scr->KeyPressUp(ck);
                            do_update = true;
                        }
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
