#ifndef SCREEN_H
#define SCREEN_H

#include "ostypes.h"
#include "osmutex.h"
#include "gk_conf.h"
#include "util.h"

void init_screen();
uintptr_t screen_update();
int screen_map_for_process(const VMemBlock &vmem, unsigned int layer, unsigned int buf,
    uintptr_t ttbr0);

constexpr size_t scr_layer_size_bytes = align_power_2(GK_SCREEN_WIDTH * GK_SCREEN_HEIGHT * 4);
constexpr unsigned int scr_n_layers = 2;
constexpr unsigned int scr_n_bufs = 3;

#endif
