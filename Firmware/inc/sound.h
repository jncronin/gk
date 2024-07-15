#ifndef SOUND_H
#define SOUND_H

#include "memblk.h"

void init_sound();

int syscall_audiosetmode(int nchan, int nbits, int freq, size_t buf_size_bytes, int *_errno);
int syscall_audioenable(int enable, int *_errno);
int syscall_audioqueuebuffer(const void *buffer, void **next_buffer, int *_errno);
int syscall_audiowaitfree(int *_errno);

extern MemRegion sound_get_buffer();

int sound_set_volume(int vol_pct);
int sound_get_volume();

#endif
