#if 0

#include "../lvgl-9.1.0/src/core/lv_refr.h"
#include "../lvgl-9.1.0/src/stdlib/lv_string.h"
#include "../lvgl-9.1.0/src/core/lv_global.h"
#include "../lvgl-9.1.0/src/lv_init.h"
#include "gk_conf.h"
#include "osqueue.h"
#include "_gk_event.h"

SRAM4_DATA static FixedQueue<Event, 32> input_queue;
SRAM4_DATA static lv_indev_data_t d_kbd, d_mouse, d_touch;
SRAM4_DATA static lv_indev_t *indev_kbd = nullptr, *indev_mouse = nullptr, *indev_touch = nullptr; 
static enum _lv_key_t gk_key_to_lv(unsigned short gkkey);

bool lv_gk_input_push_event(Event &e)
{
    bool kbd_event = false;
    bool mouse_event = false;
    bool touch_event = false;

    switch(e.type)
    {
        case Event::KeyUp:
        case Event::KeyDown:
            input_queue.Push(e);
            kbd_event = true;
            break;
        case Event::MouseDown:
        case Event::MouseUp:
        case Event::MouseMove:
            input_queue.Push(e);
            mouse_event = true;
            break;
        default:
            break;
    }

    if(mouse_event && indev_mouse)
        lv_indev_read(indev_mouse);
    if(kbd_event && indev_kbd)
        lv_indev_read(indev_kbd);
    if(touch_event && indev_touch)
        lv_indev_read(indev_touch);

    return true;
}

void gk_update_state()
{
    struct Event ev;
    while(input_queue.TryPop(&ev))
    {
        switch(ev.type)
        {
            case Event::KeyDown:
                d_kbd.key = gk_key_to_lv(ev.key);
                d_kbd.state = LV_INDEV_STATE_PRESSED;
                break;
            case Event::KeyUp:
                d_kbd.key = gk_key_to_lv(ev.key);
                d_kbd.state = LV_INDEV_STATE_RELEASED;
                break;
            case Event::MouseDown:
                if(ev.mouse_data.buttons & 0x1)
                    d_mouse.state = LV_INDEV_STATE_PRESSED;
                break;
            case Event::MouseUp:
                if(ev.mouse_data.buttons & 0x1)
                    d_mouse.state = LV_INDEV_STATE_RELEASED;
                break;
            case Event::MouseMove:
                if(ev.mouse_data.is_rel)
                {
                    d_mouse.point.x += ev.mouse_data.x;
                    d_mouse.point.y += ev.mouse_data.y;
                }
                else
                {
                    d_mouse.point.x = ev.mouse_data.x;
                    d_mouse.point.y = ev.mouse_data.y;
                }
                if(d_mouse.point.x < 0) d_mouse.point.x = 0;
                if(d_mouse.point.x >= 640) d_mouse.point.x = 639;
                if(d_mouse.point.y < 0) d_mouse.point.y = 0;
                if(d_mouse.point.y >= 480) d_mouse.point.y = 479;
                break;

            default:
                break;
        }
    }
}

void gk_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    gk_update_state();

    data->key = d_kbd.key;
    data->state = d_kbd.state;
}

void gk_kbd_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    gk_update_state();

    data->key = d_kbd.key;
    data->state = d_kbd.state;
}

void gk_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    gk_update_state();

    data->point.x = d_mouse.point.x;
    data->point.y = d_mouse.point.y;
    data->state = d_mouse.state;
}

lv_indev_t *lv_gk_kbd_create()
{
    if(indev_kbd) return indev_kbd;

    lv_indev_t *indev = lv_indev_create();
    if(indev == NULL) return NULL;

    d_kbd.key = 0;
    d_kbd.state = LV_INDEV_STATE_RELEASED;

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, gk_kbd_read_cb);
    lv_indev_set_mode(indev, lv_indev_mode_t::LV_INDEV_MODE_EVENT);

    indev_kbd = indev;

    return indev;
}

lv_indev_t *lv_gk_mouse_create()
{
    if(indev_mouse) return indev_mouse;

    lv_indev_t *indev = lv_indev_create();
    if(indev == NULL) return NULL;

    d_mouse.point.x = 0;
    d_mouse.point.y = 0;
    d_mouse.state = LV_INDEV_STATE_RELEASED;

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, gk_mouse_read_cb);
    lv_indev_set_mode(indev, lv_indev_mode_t::LV_INDEV_MODE_EVENT);

    indev_mouse = indev;

    return indev;
}

lv_indev_t *lv_gk_touchscreen_create()
{
    if(indev_touch) return indev_touch;

    lv_indev_t *indev = lv_indev_create();
    if(indev == NULL) return NULL;

    d_touch.point.x = 0;
    d_touch.point.y = 0;
    d_touch.state = LV_INDEV_STATE_RELEASED;

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, gk_touch_read_cb);
    lv_indev_set_mode(indev, lv_indev_mode_t::LV_INDEV_MODE_EVENT);

    indev_touch = indev;

    return indev;
}

enum _lv_key_t gk_key_to_lv(unsigned short gkkey)
{
    switch(gkkey)
    {
        case 82:
            return LV_KEY_UP;
        case 81:
            return LV_KEY_DOWN;
        case 79:
            return LV_KEY_RIGHT;
        case 80:
            return LV_KEY_LEFT;
        case 41:
            return LV_KEY_ESC;
        case 76:
            return LV_KEY_DEL;
        case 42:
            return LV_KEY_BACKSPACE;
        case 40:
            return LV_KEY_ENTER;
        case 258:
            return LV_KEY_NEXT;
        case 259:
            return LV_KEY_PREV;
        case 74:
            return LV_KEY_HOME;
        case 77:
            return LV_KEY_END;
        default:
            return (_lv_key_t)0;
    }
}

#endif
