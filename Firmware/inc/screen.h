#ifndef SCREEN_H
#define SCREEN_H

void init_screen();
void *screen_flip(void **old_buf = nullptr);
void *screen_get_frame_buffer(bool back_buf = true);
void screen_get_frame_buffers(void **back_buf, void **front_buf);
void screen_set_frame_buffer(void *b0, void *b1);

void screen_set_overlay_frame_buffer(void *b0, void *b1);
void *screen_get_overlay_frame_buffer(bool back_buf = true);
void *screen_flip_overlay(void **old_buf = nullptr, bool visible = false, int alpha = -1);
void screen_set_overlay_alpha(unsigned int alpha);

void screen_set_brightness(int pct, bool save = true);
int screen_get_brightness();

double screen_get_fps();

enum screen_hardware_scale { x1, x2, x4 };
int screen_set_hardware_scale(screen_hardware_scale scale_horiz,
    screen_hardware_scale scale_vert);
screen_hardware_scale screen_get_hardware_scale_horiz();

#endif
