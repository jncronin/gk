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
#include "lv_drivers/lv_gk_input.h"

SRAM4_DATA Process p_supervisor;
SRAM4_DATA static bool main_visible = false, vol_visible = false;
SRAM4_DATA bool overlay_visible = false;
SRAM4_DATA static kernel_time vol_tout;

extern Condition scr_vsync;

static void *supervisor_thread(void *p);


static unsigned int scr_alpha = 0;

static void handle_visible_change()
{
    if(main_visible || vol_visible)
    {
        overlay_visible = true;
    }
    else
    {
        overlay_visible = false;
    }
}

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
    p_supervisor.gamepad_to_scancode[GK_KEYA] = GK_SCANCODE_RETURN;
    
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
    if(v >= 480 && main_visible)
    {
        main_visible = false;
        handle_visible_change();
    }
    else if(v < 480 && !main_visible)
    {
        main_visible = true;
        handle_visible_change();
    }
}

static void vol_anim_set_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
    if(v == 0 && vol_visible)
    {
        vol_visible = false;
        handle_visible_change();
    }
}

void *supervisor_thread(void *p)
{
    lv_init();

    auto display = lv_gk_display_create();
    lv_display_set_default(display);

    auto indev_k = lv_gk_kbd_create();
    auto grp = lv_group_create();
    lv_indev_set_group(indev_k, grp);
    lv_group_set_wrap(grp, false);

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

    /* keyboard test */
    [[maybe_unused]] auto kbd = lv_keyboard_create(scr);
    lv_group_add_obj(grp, kbd);

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_pad_all(&style_bg, 2);
    lv_style_set_pad_gap(&style_bg, 0);
    lv_style_set_radius(&style_bg, 0);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_outline_width(&style_bg, 0);
    lv_style_set_bg_opa(&style_bg, LV_OPA_0);

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 0);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_opa(&style_btn, LV_OPA_100);
    lv_style_set_border_color(&style_btn, lv_color_black());
    lv_style_set_border_side(&style_btn, LV_BORDER_SIDE_INTERNAL);
    lv_style_set_bg_opa(&style_btn, LV_OPA_50);
    lv_style_set_radius(&style_btn, 0);

    static lv_style_t style_btn_focus;
    lv_style_init(&style_btn_focus);
    lv_style_set_border_color(&style_btn_focus, lv_color_white());

    lv_obj_add_style(kbd, &style_bg, 0);
    lv_obj_add_style(kbd, &style_bg, LV_STATE_FOCUSED);
    lv_obj_add_style(kbd, &style_bg, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(kbd, &style_btn, LV_PART_ITEMS);
    lv_obj_add_style(kbd, &style_btn, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_style(kbd, &style_btn_focus, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_add_style(kbd, &style_btn_focus, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(kbd, &style_btn_focus, LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUSED);
    lv_obj_add_style(kbd, &style_btn_focus, LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);

    /* volume control */
    auto vctrl = lv_obj_create(lv_screen_active());
    lv_obj_set_size(vctrl, LV_PCT(10), LV_PCT(30));
    lv_obj_set_style_bg_color(vctrl, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_bg_opa(vctrl, LV_OPA_80, 0);
    lv_obj_set_pos(vctrl, 560, 60);
    lv_obj_set_style_opa(vctrl, LV_OPA_0, 0);
    lv_obj_set_scrollbar_mode(vctrl, LV_SCROLLBAR_MODE_OFF);

    auto vol = lv_bar_create(vctrl);
    lv_bar_set_range(vol, 0, 100);
    lv_bar_set_value(vol, sound_get_volume(), LV_ANIM_OFF);
    lv_obj_set_size(vol, 32, 100);
    lv_obj_set_align(vol, LV_ALIGN_CENTER);


    // process messages
    while(true)
    {
        Event e;
        [[maybe_unused]] bool do_update = false;
        auto thandler_delay = kernel_time::from_ms(lv_timer_handler());
        klog("supervisor: thandler_delay: %llu\n", thandler_delay.to_us());

        auto now = clock_cur();
        auto v_delay = (vol_tout.is_valid() && vol_tout > now) ? (vol_tout - now) : kernel_time();

        if(thandler_delay.is_valid() && v_delay.is_valid() && (v_delay < thandler_delay))
            thandler_delay = v_delay;        

        bool has_event = thandler_delay.is_valid() ? p_supervisor.events.Pop(&e, thandler_delay) :
            p_supervisor.events.TryPop(&e);

        if(vol_tout.is_valid() && clock_cur() >= vol_tout)
        {
            vol_tout.invalidate();

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, vctrl);
            lv_anim_set_duration(&a, 500);
            lv_anim_set_exec_cb(&a, vol_anim_set_opa_cb);
            lv_anim_set_values(&a, LV_OPA_100, LV_OPA_0);
            lv_anim_set_path_cb(&a, lv_anim_path_linear);
            lv_anim_start(&a);
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
                            if(main_visible)
                            {
                                lv_anim_t a;
                                lv_anim_init(&a);
                                lv_anim_set_var(&a, scr);
                                lv_anim_set_duration(&a, 500);
                                lv_anim_set_exec_cb(&a, scr_anim_set_y_cb);
                                lv_anim_set_values(&a, lv_obj_get_y(scr), 480);
                                lv_anim_set_path_cb(&a, lv_anim_path_linear);
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
                                lv_anim_set_path_cb(&a, lv_anim_path_linear);
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
                            lv_gk_input_push_event(e);
                        }
                        break;

                        case GK_SCANCODE_VOLUMEDOWN:
                            sound_set_volume(sound_get_volume() - 10);
                            // fallthrough
                        case GK_SCANCODE_VOLUMEUP:
                            sound_set_volume(sound_get_volume() + 10);

                            klog("supervisor: sound set to %d\n", sound_get_volume());

                            lv_bar_set_value(vol, sound_get_volume(), LV_ANIM_OFF);
                            lv_obj_set_style_opa(vctrl, LV_OPA_100, 0);
                            vol_visible = true;
                            vol_tout = clock_cur() + kernel_time::from_ms(2000);
                            handle_visible_change();
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
                            lv_gk_input_push_event(e);
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
                    lv_gk_input_push_event(e);
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
