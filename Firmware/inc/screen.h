#ifndef SCREEN_H
#define SCREEN_H

void init_screen();
void *screen_flip();
void *screen_get_frame_buffer();
void screen_set_frame_buffer(void *b0, void *b1, void *gpu_scratch);

#endif
