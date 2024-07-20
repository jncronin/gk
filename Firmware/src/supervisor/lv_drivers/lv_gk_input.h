#ifndef LV_GK_INPUT_H
#define LV_GK_INPUT_H

#include "../lvgl-9.1.0/src/core/lv_global.h"
#include "_gk_event.h"

lv_indev_t * lv_gk_mouse_create(void);
lv_indev_t * lv_gk_touchscreen_create(void);
lv_indev_t * lv_gk_kbd_create(void);
bool lv_gk_input_push_event(Event &e);

#endif
