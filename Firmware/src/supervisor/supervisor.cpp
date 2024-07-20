#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "screen.h"
#include "cache.h"
#include "_gk_event.h"
#include "btnled.h"
#include "brightness.h"
#include "sound.h"
#include "lvgl.h"
#include "lv_drivers/lv_gk_display.h"
#include "gk_conf.h"

SRAM4_DATA Process p_supervisor;
SRAM4_DATA bool overlay_visible = false;

extern Condition scr_vsync;

static void *supervisor_thread(void *p);


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
    p_supervisor.screen_pf = GK_PIXELFORMAT_L8;
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

void kbd_click_up(Widget *w, coord_t x, coord_t y, int key)
{
    focus_process->events.Push({ .type = Event::KeyUp, .key = (unsigned short)key });
}

void kbd_click_down(Widget *w, coord_t x, coord_t y, int key)
{
    focus_process->events.Push({ .type = Event::KeyDown, .key = (unsigned short)key });
}

static void scr_anim_set_y_cb(void *var, int32_t v)
{
    klog("cb: %d\n", v);
    lv_obj_set_y((lv_obj_t *)var, v);
    if(v >= 480 && overlay_visible)
    {
        overlay_visible = false;
    }
    else if(v < 480 && !overlay_visible)
    {
        overlay_visible = true;
    }
}

void *supervisor_thread(void *p)
{
    lv_init();

    auto display = lv_gk_display_create();
    lv_display_set_default(display);

    /* Set transparent screen */
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_layer_bottom(), LV_OPA_TRANSP, LV_PART_MAIN);

    /* Remove scroll bars */
    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

    auto scr = lv_obj_create(lv_screen_active());
    lv_obj_set_size(scr, LV_PCT(100), 300);
    lv_obj_set_style_bg_color(scr, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_80, 0);
    lv_obj_set_y(scr, 480);


    // process messages
    while(true)
    {
        Event e;
        [[maybe_unused]] bool do_update = false;
        auto thandler_delay = kernel_time::from_ms(lv_timer_handler());
        klog("supervisor: thandler_delay: %llu\n", thandler_delay.to_us());

        bool has_event = thandler_delay.is_valid() ? p_supervisor.events.Pop(&e, thandler_delay) :
            p_supervisor.events.TryPop(&e);

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
                                lv_anim_t a;
                                lv_anim_init(&a);
                                lv_anim_set_var(&a, scr);
                                lv_anim_set_duration(&a, 500);
                                lv_anim_set_exec_cb(&a, scr_anim_set_y_cb);
                                lv_anim_set_values(&a, lv_obj_get_y(scr), 480);
                                lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
                                lv_anim_start(&a);

                            }
                            else
                            {
                                lv_anim_t a;
                                lv_anim_init(&a);
                                lv_anim_set_var(&a, scr);
                                lv_anim_set_duration(&a, 500);
                                lv_anim_set_exec_cb(&a, scr_anim_set_y_cb);
                                lv_anim_set_values(&a,  lv_obj_get_y(scr), 480 - lv_obj_get_height(scr));
                                lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
                                lv_anim_start(&a);
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
                            //cur_scr->KeyPressDown(ck);
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
                            //cur_scr->KeyPressUp(ck);
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

                        //lab_caption.text = capt + scr_capt;
                        do_update = true;
                    }
                    break;

                default:
                    // ignore
                    break;
            }
        }

/*
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

            //cur_scr->Update();
            //screen_flip_overlay(true, scr_alpha);
            //scr_vsync.Wait();
        }
*/    
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
