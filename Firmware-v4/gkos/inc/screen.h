#ifndef SCREEN_H
#define SCREEN_H

#include "ostypes.h"
#include "osmutex.h"
#include "gk_conf.h"
#include "util.h"
#include "_gk_gpu.h"

void init_screen();
uintptr_t screen_update();
int screen_current_buf();
std::pair<uintptr_t, uintptr_t> screen_current();
std::pair<uintptr_t, uintptr_t> _screen_current();
PMemBlock screen_get_buf(unsigned int layer, unsigned int buf);
size_t screen_get_bpp_for_pf(unsigned int pf);
void screen_clear_all_userspace();

int syscall_screenflip(unsigned int layer, unsigned int alpha, int *_errno);

std::pair<void *, uintptr_t> screen_get_layer_vaddr_paddr(unsigned int layer, unsigned int buf);

int screen_get_brightness();
int screen_set_brightness(int brightness, bool persist = true);
double screen_get_fps(unsigned int layer = 0);
void *screen_get_overlay_frame_buffer();

int screen_set_background_colour(uint32_t col);
int screen_set_startup_img(const void *img, unsigned int w = 200, unsigned int h = 240,
    unsigned int pf = GK_PIXELFORMAT_RGB8);

constexpr size_t scr_layer_size_bytes = align_power_2(GK_MAX_SCREEN_WIDTH * GK_MAX_SCREEN_HEIGHT * 4);
constexpr unsigned int scr_n_layers = 2;
constexpr unsigned int scr_n_bufs = 3;

#endif
