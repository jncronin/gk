#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "screen.h"
#include "cache.h"
#include "_gk_event.h"
#include "widgets/widget.h"
#include "gk_conf.h"

SRAM4_DATA Process p_supervisor;
SRAM4_DATA static bool overlay_visible = false;

static void *supervisor_thread(void *p);

ButtonWidget rw_test, rw_test2;
GridWidget scr_test;

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

void *supervisor_thread(void *p)
{
    rw_test.x = 32;
    rw_test.w = 200;
    rw_test.y = 32;
    rw_test.h = 100;
    rw_test.text = "Hi there";

    rw_test2.x = 264;
    rw_test2.w = 200;
    rw_test2.y = 32;
    rw_test2.h = 100;
    rw_test2.text = "Eh?";

    scr_test.x = 0;
    scr_test.y = 240;
    scr_test.w = 640;
    scr_test.h = 240;

    scr_test.AddChild(rw_test);
    scr_test.AddChild(rw_test2);

    Widget *cur_scr = &scr_test;

    // process messages
    while(true)
    {
        Event e;
        bool do_update = false;
        while(!p_supervisor.events.Pop(&e));

        switch(e.type)
        {
            case Event::KeyDown:
                switch(e.key)
                {
                    case ' ':
                    {
                        overlay_visible = !overlay_visible;
                        if(overlay_visible)
                        {
                            do_update = true;
                        }
                        else
                        {
                            screen_set_overlay_alpha(0x0U);
                        }
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

        if(overlay_visible && do_update)
        {
            auto fb = (color_t *)screen_get_overlay_frame_buffer();
            // clear screen
            for(coord_t cy = 0; cy < 480; cy++)
            {
                for(coord_t cx = 0; cx < 640; cx++)
                {
                    fb[cx + cy * fb_stride] = 0;
                }
            }

            cur_scr->Update();
            screen_flip_overlay();
            screen_set_overlay_alpha(0xffU);
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
