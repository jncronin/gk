#ifndef SCREEN_H
#define SCREEN_H

#include "ostypes.h"
#include "osmutex.h"
#include "gk_conf.h"
#include "util.h"

void init_screen();
uintptr_t screen_update();
std::pair<uintptr_t, uintptr_t> screen_current();
std::pair<uintptr_t, uintptr_t> _screen_current();
PMemBlock screen_get_buf(unsigned int layer, unsigned int buf);
size_t screen_get_bpp_for_pf(unsigned int pf);
void screen_clear_all_userspace();

int screen_get_brightness();
int screen_set_brightness(int brightness);
double screen_get_fps(unsigned int layer = 0);
void *screen_get_overlay_frame_buffer();

constexpr size_t scr_layer_size_bytes = align_power_2(GK_MAX_SCREEN_WIDTH * GK_MAX_SCREEN_HEIGHT * 4);
constexpr unsigned int scr_n_layers = 2;
constexpr unsigned int scr_n_bufs = 3;

#endif
