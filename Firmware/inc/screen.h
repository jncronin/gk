#ifndef SCREEN_H
#define SCREEN_H

void init_screen();
void *screen_flip();
void *screen_get_frame_buffer();
void screen_set_frame_buffer(void *b0, void *b1);

void screen_set_overlay_frame_buffer(void *b0, void *b1);
void *screen_get_overlay_frame_buffer();
void *screen_flip_overlay(bool visible = false);
void screen_set_overlay_alpha(unsigned int alpha);

#endif
