#ifndef SCREEN_H
#define SCREEN_H

#include "ostypes.h"
#include "osmutex.h"
#include "gk_conf.h"
#include "util.h"

void init_screen();
uintptr_t screen_update();
std::pair<uintptr_t, uintptr_t> screen_current();
PMemBlock screen_get_buf(unsigned int layer, unsigned int buf);

constexpr size_t scr_layer_size_bytes = align_power_2(GK_MAX_SCREEN_WIDTH * GK_MAX_SCREEN_HEIGHT * 4);
constexpr unsigned int scr_n_layers = 2;
constexpr unsigned int scr_n_bufs = 3;

#endif
